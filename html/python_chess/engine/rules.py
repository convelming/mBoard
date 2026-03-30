from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

import chess
import numpy as np

from .action_space import ACTION_SIZE, move_to_index


@dataclass
class MoveInfo:
    uci: str
    san: str
    is_capture: bool
    is_check: bool
    is_checkmate: bool


class ChessRules:
    """Rule layer compatible with chess.js semantics via python-chess backend."""

    def __init__(self, fen: Optional[str] = None):
        self.board = chess.Board(fen=fen) if fen else chess.Board()

    def fen(self) -> str:
        return self.board.fen()

    def reset(self) -> None:
        self.board.reset()

    def legal_moves(self) -> List[str]:
        return [m.uci() for m in self.board.legal_moves]

    def legal_moves_verbose(self) -> List[MoveInfo]:
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
        try:
            move = chess.Move.from_uci(uci)
        except ValueError:
            return False
        if move not in self.board.legal_moves:
            return False
        self.board.push(move)
        return True

    def game_result(self) -> str:
        if self.board.is_checkmate():
            return "1-0" if self.board.turn == chess.BLACK else "0-1"
        if self.board.is_stalemate() or self.board.is_insufficient_material() or self.board.can_claim_draw():
            return "1/2-1/2"
        return "*"

    def is_terminal(self) -> bool:
        return self.board.is_game_over(claim_draw=True)

    def policy_mask(self) -> np.ndarray:
        mask = np.zeros(ACTION_SIZE, dtype=np.float32)
        for m in self.board.legal_moves:
            mask[move_to_index(m)] = 1.0
        return mask

    def encode_planes(self) -> np.ndarray:
        """Encode position to [18, 8, 8] planes.

        0..11: piece planes for side-to-move perspective
        12: side to move
        13: castling K
        14: castling Q
        15: castling k
        16: castling q
        17: halfmove clock normalized
        """
        planes = np.zeros((18, 8, 8), dtype=np.float32)

        piece_order = [
            chess.PAWN, chess.KNIGHT, chess.BISHOP,
            chess.ROOK, chess.QUEEN, chess.KING,
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
