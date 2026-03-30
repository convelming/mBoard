from __future__ import annotations

import torch
import torch.nn as nn

from .action_space import ACTION_SIZE


class ResidualBlock(nn.Module):
    def __init__(self, channels: int):
        super().__init__()
        self.c1 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.b1 = nn.BatchNorm2d(channels)
        self.c2 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.b2 = nn.BatchNorm2d(channels)
        self.act = nn.ReLU(inplace=True)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        r = x
        x = self.act(self.b1(self.c1(x)))
        x = self.b2(self.c2(x))
        return self.act(x + r)


class AlphaZeroNet(nn.Module):
    def __init__(self, in_planes: int = 18, channels: int = 128, blocks: int = 6):
        super().__init__()
        self.stem = nn.Sequential(
            nn.Conv2d(in_planes, channels, 3, padding=1, bias=False),
            nn.BatchNorm2d(channels),
            nn.ReLU(inplace=True),
        )
        self.tower = nn.Sequential(*[ResidualBlock(channels) for _ in range(blocks)])

        # policy head
        self.p_conv = nn.Conv2d(channels, 32, 1, bias=False)
        self.p_bn = nn.BatchNorm2d(32)
        self.p_fc = nn.Linear(32 * 8 * 8, ACTION_SIZE)

        # value head
        self.v_conv = nn.Conv2d(channels, 32, 1, bias=False)
        self.v_bn = nn.BatchNorm2d(32)
        self.v_fc1 = nn.Linear(32 * 8 * 8, 128)
        self.v_fc2 = nn.Linear(128, 1)

        self.act = nn.ReLU(inplace=True)

    def forward(self, x: torch.Tensor):
        x = self.stem(x)
        x = self.tower(x)

        p = self.act(self.p_bn(self.p_conv(x)))
        p = p.flatten(1)
        p = self.p_fc(p)

        v = self.act(self.v_bn(self.v_conv(x)))
        v = v.flatten(1)
        v = self.act(self.v_fc1(v))
        v = torch.tanh(self.v_fc2(v))

        return p, v
