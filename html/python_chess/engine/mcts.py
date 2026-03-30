from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

import chess
import numpy as np
import torch

from .action_space import ACTION_SIZE, move_to_index
from .rules import ChessRules


@dataclass
class Node:
    """Single node in search tree.

    Attributes:
    - prior: prior probability from network policy p(s, a)
    - visits: visit count N(s, a)
    - value_sum: cumulative backed-up value W(s, a)
    - children: move -> child node
    """

    prior: float
    visits: int = 0
    value_sum: float = 0.0
    children: Dict[chess.Move, "Node"] = field(default_factory=dict)

    def value(self) -> float:
        """Mean action value Q = W / N."""
        return 0.0 if self.visits == 0 else self.value_sum / self.visits


class MCTS:
    """PUCT MCTS for AlphaZero-style policy improvement.

    Search step summary:
    1) Evaluate root with network -> masked policy and value.
    2) Repeat simulations:
       - Selection via PUCT score Q + U.
       - Expansion at leaf with network policy.
       - Backup leaf value along path with alternating sign.
    3) Convert root visit counts to improved policy pi.

    Inputs:
    - rules: current game state wrapper
    - network output dimensions: policy 4672, value scalar

    Outputs:
    - best_move: chess.Move selected from visit distribution
    - pi: np.ndarray [4672], normalized visit counts at root
    """

    def __init__(self, model, device: str = "cpu", c_puct: float = 1.25, simulations: int = 96):
        self.model = model
        self.device = device
        self.c_puct = c_puct
        self.simulations = simulations

    @torch.no_grad()
    def _evaluate(self, rules: ChessRules) -> Tuple[np.ndarray, float]:
        """Run network on state and apply legal action mask."""
        x = torch.from_numpy(rules.encode_planes()).unsqueeze(0).to(self.device)
        logits, value = self.model(x)
        policy = torch.softmax(logits, dim=-1).squeeze(0).cpu().numpy()

        mask = rules.policy_mask()
        policy = policy * mask

        # If all legal priors are zero due to numerical/model issue, fallback to uniform legal distribution.
        s = policy.sum()
        if s <= 1e-8:
            policy = mask / max(mask.sum(), 1.0)
        else:
            policy /= s

        return policy, float(value.item())

    def _select(self, node: Node):
        """Select child move by PUCT: Q + c_puct * P * sqrt(N_parent)/(1+N_child)."""
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
        """Create child nodes for all legal moves with their policy priors."""
        for mv in rules.board.legal_moves:
            idx = move_to_index(mv)
            node.children[mv] = Node(prior=float(policy[idx]))

    def _backup(self, path: List[Node], value: float):
        """Backpropagate value to root, flipping sign each ply (player alternation)."""
        sign = 1.0
        for n in reversed(path):
            n.visits += 1
            n.value_sum += sign * value
            sign = -sign

    def search(self, rules: ChessRules, temperature: float = 1.0):
        """Run full MCTS and return (selected_move, root_visit_policy).

        Args:
            rules: current state
            temperature: visit-count temperature when converting to distribution.
                         near 0 -> greedy; higher -> more exploratory.

        Returns:
            best_move: chess.Move or None
            pi: np.ndarray [4672], visit distribution at root
        """
        root = Node(prior=1.0)

        p_root, v_root = self._evaluate(rules)
        self._expand(root, rules, p_root)
        root.visits = 1
        root.value_sum = v_root

        for _ in range(self.simulations):
            sim_rules = ChessRules(rules.fen())
            node = root
            path = [node]

            # Selection
            while node.children:
                mv = self._select(node)
                if mv is None:
                    break
                sim_rules.board.push(mv)
                node = node.children[mv]
                path.append(node)

            # Terminal leaf shortcut
            if sim_rules.is_terminal():
                res = sim_rules.game_result()
                value = 0.0 if res == "1/2-1/2" else (1.0 if res == "1-0" else -1.0)
                self._backup(path, value)
                continue

            # Expansion + evaluation
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
