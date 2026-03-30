from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, Optional

import chess
import numpy as np
import torch

from .action_space import ACTION_SIZE, move_to_index
from .rules import ChessRules


@dataclass
class Node:
    prior: float
    visits: int = 0
    value_sum: float = 0.0
    children: Dict[chess.Move, "Node"] = field(default_factory=dict)

    def value(self) -> float:
        return 0.0 if self.visits == 0 else self.value_sum / self.visits


class MCTS:
    def __init__(self, model, device: str = "cpu", c_puct: float = 1.25, simulations: int = 96):
        self.model = model
        self.device = device
        self.c_puct = c_puct
        self.simulations = simulations

    @torch.no_grad()
    def _evaluate(self, rules: ChessRules):
        x = torch.from_numpy(rules.encode_planes()).unsqueeze(0).to(self.device)
        logits, value = self.model(x)
        policy = torch.softmax(logits, dim=-1).squeeze(0).cpu().numpy()

        mask = rules.policy_mask()
        policy = policy * mask
        s = policy.sum()
        if s <= 1e-8:
            policy = mask / max(mask.sum(), 1.0)
        else:
            policy /= s

        return policy, float(value.item())

    def _select(self, node: Node):
        best_move = None
        best_score = -1e9
        total = math.sqrt(max(node.visits, 1))

        for mv, child in node.children.items():
            q = child.value()
            u = self.c_puct * child.prior * total / (1 + child.visits)
            score = q + u
            if score > best_score:
                best_score = score
                best_move = mv
        return best_move

    def _expand(self, node: Node, rules: ChessRules, policy: np.ndarray):
        for mv in rules.board.legal_moves:
            idx = move_to_index(mv)
            node.children[mv] = Node(prior=float(policy[idx]))

    def _backup(self, path, value: float):
        sign = 1.0
        for n in reversed(path):
            n.visits += 1
            n.value_sum += sign * value
            sign = -sign

    def search(self, rules: ChessRules, temperature: float = 1.0):
        root = Node(prior=1.0)
        p, v = self._evaluate(rules)
        self._expand(root, rules, p)
        root.visits = 1
        root.value_sum = v

        for _ in range(self.simulations):
            sim_rules = ChessRules(rules.fen())
            node = root
            path = [node]

            while node.children:
                mv = self._select(node)
                if mv is None:
                    break
                sim_rules.board.push(mv)
                node = node.children[mv]
                path.append(node)

            if sim_rules.is_terminal():
                res = sim_rules.game_result()
                value = 0.0 if res == "1/2-1/2" else (1.0 if res == "1-0" else -1.0)
                self._backup(path, value)
                continue

            p_leaf, v_leaf = self._evaluate(sim_rules)
            self._expand(node, sim_rules, p_leaf)
            self._backup(path, v_leaf)

        moves = list(root.children.keys())
        visits = np.array([root.children[m].visits for m in moves], dtype=np.float32)
        if temperature <= 1e-5:
            probs = np.zeros_like(visits)
            probs[np.argmax(visits)] = 1.0
        else:
            visits = np.power(visits, 1.0 / temperature)
            probs = visits / max(visits.sum(), 1e-8)

        pi = np.zeros(ACTION_SIZE, dtype=np.float32)
        for mv, p_i in zip(moves, probs):
            pi[move_to_index(mv)] = p_i

        best_move = moves[int(np.argmax(probs))] if moves else None
        return best_move, pi
