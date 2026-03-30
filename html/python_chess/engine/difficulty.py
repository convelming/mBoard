from __future__ import annotations

from pathlib import Path
from typing import Dict


# You can map checkpoints to human-readable difficulty tiers.
DIFFICULTY_MAP: Dict[str, str] = {
    "az_iter_005.pt": "Beginner",
    "az_iter_020.pt": "Intermediate",
    "az_iter_050.pt": "Advanced",
    "az_iter_100.pt": "Expert",
}


def pick_checkpoint(difficulty: str, ckpt_dir: str = "checkpoints") -> str | None:
    p = Path(ckpt_dir)
    if not p.exists():
        return None

    inv = {v.lower(): k for k, v in DIFFICULTY_MAP.items()}
    target = inv.get(difficulty.lower())
    if target:
        full = p / target
        if full.exists():
            return str(full)

    # fallback to latest
    cks = sorted(p.glob("az_iter_*.pt"))
    return str(cks[-1]) if cks else None
