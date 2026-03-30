from __future__ import annotations

from dataclasses import dataclass
from typing import List, Tuple

import numpy as np

from .mcts import MCTS
from .rules import ChessRules


@dataclass
class Sample:
    state: np.ndarray
    policy: np.ndarray
    value: float


def self_play_game(mcts: MCTS, max_moves: int = 220, temp_moves: int = 20) -> Tuple[List[Sample], str]:
    rules = ChessRules()
    states: List[np.ndarray] = []
    policies: List[np.ndarray] = []
    turns: List[int] = []

    for ply in range(max_moves):
        if rules.is_terminal():
            break
        states.append(rules.encode_planes())
        turns.append(1 if rules.board.turn else -1)

        t = 1.0 if ply < temp_moves else 1e-6
        move, pi = mcts.search(rules, temperature=t)
        policies.append(pi)
        if move is None:
            break
        rules.board.push(move)

    result = rules.game_result()
    if result == "1-0":
        z = 1.0
    elif result == "0-1":
        z = -1.0
    else:
        z = 0.0

    samples: List[Sample] = []
    for s, p, turn in zip(states, policies, turns):
        samples.append(Sample(state=s, policy=p, value=z * turn))
    return samples, result
