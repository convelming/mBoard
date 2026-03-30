from __future__ import annotations

from dataclasses import dataclass
from typing import List, Tuple

import numpy as np

from .mcts import MCTS
from .rules import ChessRules


@dataclass
class Sample:
    """Single supervised sample extracted from self-play.

    Fields:
    - state:  [18, 8, 8] board planes
    - policy: [4672] improved policy pi from MCTS root visits
    - value:  scalar z in {-1, 0, 1} from side-to-move perspective at this state
    """

    state: np.ndarray
    policy: np.ndarray
    value: float


def self_play_game(mcts: MCTS, max_moves: int = 220, temp_moves: int = 20) -> Tuple[List[Sample], str]:
    """Run one self-play game and produce training samples.

    Process:
    1) For each ply, run MCTS from current state.
    2) Save (state, pi, side_to_move_sign).
    3) Play selected move.
    4) At terminal, convert final result to z and map to each saved state's perspective.

    Args:
        mcts: configured MCTS instance
        max_moves: hard stop to avoid pathological very long games
        temp_moves: initial plies using high exploration temperature (t=1.0)

    Returns:
        samples: list of Sample
        result: final PGN-style marker "1-0", "0-1", "1/2-1/2", or "*"
    """
    rules = ChessRules()
    states: List[np.ndarray] = []
    policies: List[np.ndarray] = []
    turns: List[int] = []

    for ply in range(max_moves):
        if rules.is_terminal():
            break

        states.append(rules.encode_planes())
        turns.append(1 if rules.board.turn else -1)

        # Exploration early, near-greedy later.
        temperature = 1.0 if ply < temp_moves else 1e-6
        move, pi = mcts.search(rules, temperature=temperature)
        policies.append(pi)

        if move is None:
            break

        rules.board.push(move)

    result = rules.game_result()
    if result == "1-0":
        z_final = 1.0
    elif result == "0-1":
        z_final = -1.0
    else:
        z_final = 0.0

    samples: List[Sample] = []
    for s, p, turn in zip(states, policies, turns):
        samples.append(Sample(state=s, policy=p, value=z_final * turn))

    return samples, result
