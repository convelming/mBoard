from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

import chess
import numpy as np

from .action_space import ACTION_SIZE, move_to_index


@dataclass
class MoveInfo:
    """Verbose move info for debugging/UI.

    Fields:
    - uci: machine-readable move, e.g. "e2e4"
    - san: human-readable move, e.g. "Nf3"
    - is_capture: whether this move captures
    - is_check: whether this move gives check
    - is_checkmate: whether this move gives checkmate
    """

    uci: str
    san: str
    is_capture: bool
    is_check: bool
    is_checkmate: bool


class ChessRules:
    """Rule/state wrapper built on python-chess.

    Main responsibilities:
    1) Maintain board state.
    2) Provide legal moves and legality checks.
    3) Encode state planes for neural network input.
    4) Build legal action mask over 4672 policy dimension.

    Input to network:
        encode_planes() -> np.ndarray shape [18, 8, 8], float32

    Output target from MCTS:
        policy_mask()   -> np.ndarray shape [4672], float32
    """

    def __init__(self, fen: Optional[str] = None):
        self.board = chess.Board(fen=fen) if fen else chess.Board()

    def fen(self) -> str:
        """Current board state in FEN format."""
        return self.board.fen()

    def reset(self) -> None:
        """Reset to initial chess position."""
        self.board.reset()

    def legal_moves(self) -> List[str]:
        """Return legal moves in UCI form."""
        return [m.uci() for m in self.board.legal_moves]

    def legal_moves_verbose(self) -> List[MoveInfo]:
        """Return legal moves with extra metadata useful for frontend logs."""
        out: List[MoveInfo] = []
        for m in self.board.legal_moves:
            san = self.board.san(m)
            is_capture = self.board.is_capture(m)
            b2 = self.board.copy(stack=False)
            b2.push(m)
            out.append(
                MoveInfo(
                    uci=m.uci(),
                    san=san,
                    is_capture=is_capture,
                    is_check=b2.is_check(),
                    is_checkmate=b2.is_checkmate(),
                )
            )
        return out

    def push_uci(self, uci: str) -> bool:
        """Apply user move if legal.

        Args:
            uci: move string like "e2e4" or promotion "e7e8q".

        Returns:
            True if move is legal and applied; False otherwise.
        """
        try:
            move = chess.Move.from_uci(uci)
        except ValueError:
            return False
        if move not in self.board.legal_moves:
            return False
        self.board.push(move)
        return True

    def game_result(self) -> str:
        """Return game result marker: "1-0", "0-1", "1/2-1/2", or "*"."""
        if self.board.is_checkmate():
            # If board.turn is BLACK at checkmate, white just moved and won.
            return "1-0" if self.board.turn == chess.BLACK else "0-1"
        if self.board.is_stalemate() or self.board.is_insufficient_material() or self.board.can_claim_draw():
            return "1/2-1/2"
        return "*"

    def is_terminal(self) -> bool:
        """True if game is over (including claimable draw)."""
        return self.board.is_game_over(claim_draw=True)

    def policy_mask(self) -> np.ndarray:
        """Build legal-action mask over fixed policy head.

        Returns:
            np.ndarray [4672], where legal indices are 1.0 and illegal are 0.0.
        """
        mask = np.zeros(ACTION_SIZE, dtype=np.float32)
        for m in self.board.legal_moves:
            mask[move_to_index(m)] = 1.0
        return mask

    def encode_planes(self) -> np.ndarray:
        """Encode board to network input planes.

        Shape:
            [18, 8, 8], float32

        Plane semantics:
            0..5   : white P,N,B,R,Q,K occupancy
            6..11  : black P,N,B,R,Q,K occupancy
            12     : side to move (all ones if white to move, else zeros)
            13     : white king-side castling right
            14     : white queen-side castling right
            15     : black king-side castling right
            16     : black queen-side castling right
            17     : normalized halfmove clock

        Coordinate convention:
            planes[c, rank, file], rank/file in python-chess indexing.
        """
        planes = np.zeros((18, 8, 8), dtype=np.float32)

        piece_order = [
            chess.PAWN,
            chess.KNIGHT,
            chess.BISHOP,
            chess.ROOK,
            chess.QUEEN,
            chess.KING,
        ]

        for p_idx, p in enumerate(piece_order):
            for sq in self.board.pieces(p, chess.WHITE):
                r = chess.square_rank(sq)
                f = chess.square_file(sq)
                planes[p_idx, r, f] = 1.0
            for sq in self.board.pieces(p, chess.BLACK):
                r = chess.square_rank(sq)
                f = chess.square_file(sq)
                planes[p_idx + 6, r, f] = 1.0

        planes[12, :, :] = 1.0 if self.board.turn == chess.WHITE else 0.0
        planes[13, :, :] = 1.0 if self.board.has_kingside_castling_rights(chess.WHITE) else 0.0
        planes[14, :, :] = 1.0 if self.board.has_queenside_castling_rights(chess.WHITE) else 0.0
        planes[15, :, :] = 1.0 if self.board.has_kingside_castling_rights(chess.BLACK) else 0.0
        planes[16, :, :] = 1.0 if self.board.has_queenside_castling_rights(chess.BLACK) else 0.0
        planes[17, :, :] = min(1.0, self.board.halfmove_clock / 100.0)

        return planes
