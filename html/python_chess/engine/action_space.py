from __future__ import annotations

import chess

# AlphaZero chess convention: 8x8x73 = 4672
ACTION_PLANES = 73
ACTION_SIZE = 64 * ACTION_PLANES


def move_to_index(move: chess.Move) -> int:
    """Encode move into AlphaZero-like 4672 action index.

    Plane layout (simplified):
    - 0..55: sliding moves (8 directions * 7 steps)
    - 56..63: knight moves (8)
    - 64..72: underpromotions (3 pieces x 3 directions)
    Queen promotions are mapped through normal move planes.
    """
    from_sq = move.from_square
    to_sq = move.to_square
    ff = chess.square_file(from_sq)
    rf = chess.square_rank(from_sq)
    ft = chess.square_file(to_sq)
    rt = chess.square_rank(to_sq)
    df = ft - ff
    dr = rt - rf

    # Knight
    knight_offsets = [
        (1, 2), (2, 1), (2, -1), (1, -2),
        (-1, -2), (-2, -1), (-2, 1), (-1, 2),
    ]
    if (df, dr) in knight_offsets:
        plane = 56 + knight_offsets.index((df, dr))
        return from_sq * ACTION_PLANES + plane

    # Underpromotion
    if move.promotion in {chess.KNIGHT, chess.BISHOP, chess.ROOK}:
        promo_piece_to_block = {
            chess.KNIGHT: 0,
            chess.BISHOP: 1,
            chess.ROOK: 2,
        }
        block = promo_piece_to_block[move.promotion]
        # direction in file: left/forward/right => -1,0,1
        file_dir = -1 if df < 0 else (1 if df > 0 else 0)
        dir_idx = {-1: 0, 0: 1, 1: 2}[file_dir]
        plane = 64 + block * 3 + dir_idx
        return from_sq * ACTION_PLANES + plane

    # Sliding (or queen promotion)
    directions = [
        (0, 1),   # N
        (0, -1),  # S
        (1, 0),   # E
        (-1, 0),  # W
        (1, 1),   # NE
        (-1, 1),  # NW
        (1, -1),  # SE
        (-1, -1), # SW
    ]

    step = max(abs(df), abs(dr))
    if step <= 0:
        return 0

    norm_df = 0 if df == 0 else (1 if df > 0 else -1)
    norm_dr = 0 if dr == 0 else (1 if dr > 0 else -1)

    if (norm_df, norm_dr) in directions and step <= 7:
        dir_idx = directions.index((norm_df, norm_dr))
        plane = dir_idx * 7 + (step - 1)
        return from_sq * ACTION_PLANES + plane

    return 0
