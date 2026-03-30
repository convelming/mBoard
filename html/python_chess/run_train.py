from __future__ import annotations

import argparse
from pathlib import Path

from engine.train import AlphaZeroTrainer, TrainConfig


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Train AlphaZero-style chess model with CLI parameters.",
    )
    p.add_argument("--device", default="cpu", help="Training device, e.g. cpu or mps")
    p.add_argument("--iters", type=int, default=20, help="Number of training iterations")
    p.add_argument("--games-per-iter", type=int, default=4, help="Self-play games each iteration")
    p.add_argument("--sims", type=int, default=64, help="MCTS simulations per move")
    p.add_argument("--epochs", type=int, default=2, help="Gradient epochs each iteration")
    p.add_argument("--batch-size", type=int, default=64, help="Batch size for optimization")
    p.add_argument("--lr", type=float, default=1e-3, help="Learning rate")
    p.add_argument(
        "--checkpoint-every-epochs",
        type=int,
        default=10,
        help="Save checkpoint every N accumulated training epochs",
    )
    p.add_argument(
        "--out-dir",
        default=str(Path(__file__).resolve().parent / "checkpoints"),
        help="Directory to save checkpoints",
    )
    return p


def main() -> None:
    args = build_parser().parse_args()

    cfg = TrainConfig(
        device=args.device,
        iters=args.iters,
        games_per_iter=args.games_per_iter,
        sims=args.sims,
        epochs=args.epochs,
        batch_size=args.batch_size,
        lr=args.lr,
        out_dir=args.out_dir,
        checkpoint_every_epochs=max(1, args.checkpoint_every_epochs),
    )

    print("[run_train] config:")
    print(cfg)

    trainer = AlphaZeroTrainer(cfg)

    def log_event(ev: dict) -> None:
        print(ev)

    trainer.train_loop(on_log=log_event)
    print("[run_train] training finished")


if __name__ == "__main__":
    main()
