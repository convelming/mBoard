from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List

import numpy as np
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset

from .alphazero_model import AlphaZeroNet
from .mcts import MCTS
from .selfplay import self_play_game


@dataclass
class TrainConfig:
    device: str = "cpu"
    games_per_iter: int = 4
    iters: int = 10
    sims: int = 64
    epochs: int = 2
    batch_size: int = 64
    lr: float = 1e-3
    out_dir: str = "checkpoints"


class AlphaZeroTrainer:
    def __init__(self, cfg: TrainConfig):
        self.cfg = cfg
        self.model = AlphaZeroNet().to(cfg.device)
        self.optim = torch.optim.Adam(self.model.parameters(), lr=cfg.lr)
        Path(cfg.out_dir).mkdir(parents=True, exist_ok=True)

    def _fit(self, states: np.ndarray, policies: np.ndarray, values: np.ndarray):
        ds = TensorDataset(
            torch.tensor(states, dtype=torch.float32),
            torch.tensor(policies, dtype=torch.float32),
            torch.tensor(values, dtype=torch.float32).unsqueeze(-1),
        )
        dl = DataLoader(ds, batch_size=self.cfg.batch_size, shuffle=True)

        self.model.train()
        for _ in range(self.cfg.epochs):
            for x, p_t, v_t in dl:
                x = x.to(self.cfg.device)
                p_t = p_t.to(self.cfg.device)
                v_t = v_t.to(self.cfg.device)

                logits, v = self.model(x)
                logp = F.log_softmax(logits, dim=-1)
                policy_loss = -(p_t * logp).sum(dim=-1).mean()
                value_loss = F.mse_loss(v, v_t)
                loss = policy_loss + value_loss

                self.optim.zero_grad()
                loss.backward()
                self.optim.step()

    def train_loop(self, on_log: Callable[[dict], None] | None = None):
        for it in range(1, self.cfg.iters + 1):
            self.model.eval()
            mcts = MCTS(self.model, device=self.cfg.device, simulations=self.cfg.sims)

            samples = []
            results = {"1-0": 0, "0-1": 0, "1/2-1/2": 0, "*": 0}
            for g in range(self.cfg.games_per_iter):
                s, r = self_play_game(mcts)
                samples.extend(s)
                results[r] = results.get(r, 0) + 1
                if on_log:
                    on_log({"event": "selfplay_game", "iter": it, "game": g + 1, "result": r, "samples": len(s)})

            states = np.stack([x.state for x in samples], axis=0)
            policies = np.stack([x.policy for x in samples], axis=0)
            values = np.array([x.value for x in samples], dtype=np.float32)

            self._fit(states, policies, values)

            ckpt = Path(self.cfg.out_dir) / f"az_iter_{it:03d}.pt"
            torch.save({"model": self.model.state_dict(), "iter": it, "results": results}, ckpt)

            if on_log:
                on_log({"event": "iter_done", "iter": it, "checkpoint": str(ckpt), "results": results, "dataset": len(samples)})
