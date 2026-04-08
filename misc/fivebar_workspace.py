#!/usr/bin/env python3
"""
Planar 5-bar workspace scanner (two motors on one edge of a 40cm x 40cm plane).

What it does:
1) Configure L1/L2 and motor placement on one side (default: bottom edge).
2) Sample workspace with 1cm x 1cm grid (configurable).
3) Compute IK reachability for both left/right 2-link chains to each grid point.
4) Save:
   - colored reachable map (PNG)
   - per-grid-point linkage state table (CSV)

Model assumptions:
- Motor bases are fixed at y=0 on the bottom edge.
- End-effector point P(x, y) lies in [0, W] x [0, H].
- Each side is a 2-link arm with lengths L1 (proximal), L2 (distal).
- A point is reachable if BOTH arms can reach it.
"""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

import numpy as np

try:
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap, BoundaryNorm
    HAS_MPL = True
except Exception:
    HAS_MPL = False


@dataclass
class ArmSolution:
    theta1_deg: float
    theta2_deg: float
    elbow: str  # "up" or "down"


@dataclass
class PointResult:
    x_cm: float
    y_cm: float
    left_count: int
    right_count: int
    combo_count: int
    reachable: int
    state_class: int
    sample_branch: str
    left_theta1_deg: float
    left_theta2_deg: float
    right_theta1_deg: float
    right_theta2_deg: float


def two_link_ik(base_x: float, base_y: float, x: float, y: float, l1: float, l2: float) -> List[ArmSolution]:
    """Return all geometric IK branches for one planar 2-link chain.

    Angles are in standard math frame:
    - theta1: base joint absolute angle
    - theta2: elbow joint relative angle (link2 relative to link1)
    """
    dx = x - base_x
    dy = y - base_y
    r2 = dx * dx + dy * dy
    r = math.sqrt(r2)

    # Basic reachability for annulus.
    if r > l1 + l2 + 1e-9:
        return []
    if r < abs(l1 - l2) - 1e-9:
        return []

    c2 = (r2 - l1 * l1 - l2 * l2) / (2.0 * l1 * l2)
    c2 = max(-1.0, min(1.0, c2))
    s2_abs = math.sqrt(max(0.0, 1.0 - c2 * c2))

    gamma = math.atan2(dy, dx)

    sols: List[ArmSolution] = []
    for s2, elbow in ((s2_abs, "up"), (-s2_abs, "down")):
        theta2 = math.atan2(s2, c2)
        # Standard 2-link IK formula.
        theta1 = gamma - math.atan2(l2 * s2, l1 + l2 * c2)
        sols.append(
            ArmSolution(
                theta1_deg=math.degrees(theta1),
                theta2_deg=math.degrees(theta2),
                elbow=elbow,
            )
        )

    # At singular boundary, up/down collapse to same solution.
    if s2_abs < 1e-8:
        return sols[:1]
    return sols


def classify_state(left_n: int, right_n: int) -> Tuple[int, int]:
    """Return (reachable, state_class) used for plotting/color.

    state_class:
      0 unreachable (any side has 0)
      1 boundary-singular (combo=1)
      2 two-combo region (combo=2)
      3 four-combo region (combo=4)
    """
    if left_n == 0 or right_n == 0:
        return 0, 0
    combo = left_n * right_n
    if combo <= 1:
        return 1, 1
    if combo == 2:
        return 1, 2
    return 1, 3


def choose_sample_branch(left_sols: List[ArmSolution], right_sols: List[ArmSolution]) -> Tuple[str, float, float, float, float]:
    """Pick one representative branch for CSV quick inspection.

    Priority branch: left=up, right=up if available.
    """
    if not left_sols or not right_sols:
        return "none", math.nan, math.nan, math.nan, math.nan

    # Preferred combo.
    for ls in left_sols:
        for rs in right_sols:
            if ls.elbow == "up" and rs.elbow == "up":
                return "L_up-R_up", ls.theta1_deg, ls.theta2_deg, rs.theta1_deg, rs.theta2_deg

    ls = left_sols[0]
    rs = right_sols[0]
    return f"L_{ls.elbow}-R_{rs.elbow}", ls.theta1_deg, ls.theta2_deg, rs.theta1_deg, rs.theta2_deg


def run_scan(
    width_cm: float,
    height_cm: float,
    l1_cm: float,
    l2_cm: float,
    left_motor_x_cm: float,
    right_motor_x_cm: float,
    left_motor_y_cm: float,
    right_motor_y_cm: float,
    grid_cm: float,
) -> Tuple[List[PointResult], np.ndarray, np.ndarray, np.ndarray]:
    xs = np.arange(0.0, width_cm + 1e-9, grid_cm)
    ys = np.arange(0.0, height_cm + 1e-9, grid_cm)

    state_map = np.zeros((len(ys), len(xs)), dtype=np.int32)
    combo_map = np.zeros((len(ys), len(xs)), dtype=np.int32)
    points: List[PointResult] = []

    for iy, y in enumerate(ys):
        for ix, x in enumerate(xs):
            left_sols = two_link_ik(left_motor_x_cm, left_motor_y_cm, x, y, l1_cm, l2_cm)
            right_sols = two_link_ik(right_motor_x_cm, right_motor_y_cm, x, y, l1_cm, l2_cm)

            left_n = len(left_sols)
            right_n = len(right_sols)
            combo = left_n * right_n
            reachable, state_class = classify_state(left_n, right_n)
            branch, lt1, lt2, rt1, rt2 = choose_sample_branch(left_sols, right_sols)

            state_map[iy, ix] = state_class
            combo_map[iy, ix] = combo

            points.append(
                PointResult(
                    x_cm=round(float(x), 4),
                    y_cm=round(float(y), 4),
                    left_count=left_n,
                    right_count=right_n,
                    combo_count=combo,
                    reachable=reachable,
                    state_class=state_class,
                    sample_branch=branch,
                    left_theta1_deg=lt1,
                    left_theta2_deg=lt2,
                    right_theta1_deg=rt1,
                    right_theta2_deg=rt2,
                )
            )

    return points, xs, ys, state_map


def save_csv(path: Path, results: List[PointResult]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "x_cm",
                "y_cm",
                "reachable",
                "state_class",
                "left_solution_count",
                "right_solution_count",
                "combo_count",
                "sample_branch",
                "left_theta1_deg",
                "left_theta2_deg",
                "right_theta1_deg",
                "right_theta2_deg",
            ]
        )
        for r in results:
            w.writerow(
                [
                    r.x_cm,
                    r.y_cm,
                    r.reachable,
                    r.state_class,
                    r.left_count,
                    r.right_count,
                    r.combo_count,
                    r.sample_branch,
                    "" if math.isnan(r.left_theta1_deg) else round(r.left_theta1_deg, 3),
                    "" if math.isnan(r.left_theta2_deg) else round(r.left_theta2_deg, 3),
                    "" if math.isnan(r.right_theta1_deg) else round(r.right_theta1_deg, 3),
                    "" if math.isnan(r.right_theta2_deg) else round(r.right_theta2_deg, 3),
                ]
            )


def save_plot(
    path: Path,
    xs: np.ndarray,
    ys: np.ndarray,
    state_map: np.ndarray,
    left_motor_x_cm: float,
    right_motor_x_cm: float,
    left_motor_y_cm: float,
    right_motor_y_cm: float,
    l1_cm: float,
    l2_cm: float,
    motor_size_cm: float,
) -> None:
    if not HAS_MPL:
        raise RuntimeError("matplotlib is not available. Install it to save PNG plots.")

    path.parent.mkdir(parents=True, exist_ok=True)

    # 0 unreachable, 1 singular boundary, 2 mid, 3 high-combo
    # Singular boundary is highlighted in RED as requested.
    cmap = ListedColormap(["#d9d9d9", "#ff4d4f", "#8fd3ff", "#58b368"])
    norm = BoundaryNorm([-0.5, 0.5, 1.5, 2.5, 3.5], cmap.N)

    fig, ax = plt.subplots(figsize=(7.2, 7.2), dpi=140)
    im = ax.imshow(
        state_map,
        origin="lower",
        extent=[xs.min() - 0.5, xs.max() + 0.5, ys.min() - 0.5, ys.max() + 0.5],
        cmap=cmap,
        norm=norm,
        interpolation="nearest",
        aspect="equal",
    )

    ax.scatter(
        [left_motor_x_cm, right_motor_x_cm],
        [left_motor_y_cm, right_motor_y_cm],
        c="red",
        s=55,
        marker="s",
        label="Motor bases",
    )

    # Draw motor footprint squares (e.g. 42mm = 4.2cm).
    half = motor_size_cm / 2.0
    for mx, my in ((left_motor_x_cm, left_motor_y_cm), (right_motor_x_cm, right_motor_y_cm)):
        rect = plt.Rectangle((mx - half, my - half), motor_size_cm, motor_size_cm,
                             fill=False, edgecolor="#7a1f1f", linewidth=1.2, linestyle="--")
        ax.add_patch(rect)

    ax.set_title(f"5-bar Reachability Map (L1={l1_cm}cm, L2={l2_cm}cm)")
    ax.set_xlabel("X (cm)")
    ax.set_ylabel("Y (cm)")
    ax.set_xlim(xs.min() - 0.5, xs.max() + 0.5)
    ax.set_ylim(ys.min() - 0.5, ys.max() + 0.5)
    ax.grid(color="white", linewidth=0.35, alpha=0.5)

    # Optional explicit overlay for singular boundary points (state_class == 1).
    sing_iy, sing_ix = np.where(state_map == 1)
    if sing_ix.size > 0:
        ax.scatter(
            xs[sing_ix],
            ys[sing_iy],
            c="#b30000",
            s=9,
            marker="s",
            linewidths=0.0,
            alpha=0.9,
            label="Singular boundary",
        )

    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_ticks([0, 1, 2, 3])
    cbar.set_ticklabels([
        "0: unreachable",
        "1: singular (1 combo)",
        "2: 2 combos",
        "3: 4 combos",
    ])

    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Scan 5-bar reachable area on 40x40 cm plane.")
    p.add_argument("--width-cm", type=float, default=40.0)
    p.add_argument("--height-cm", type=float, default=40.0)
    p.add_argument("--l1-cm", type=float, required=True, help="Proximal link length L1 (cm)")
    p.add_argument("--l2-cm", type=float, required=True, help="Distal link length L2 (cm)")

    # Two motors on one side (default bottom edge y=0)
    p.add_argument("--left-motor-x-cm", type=float, default=10.0)
    p.add_argument("--right-motor-x-cm", type=float, default=30.0)
    p.add_argument("--motor-y-cm", type=float, default=0.0)
    p.add_argument("--left-motor-y-cm", type=float, default=None, help="Optional left motor Y (cm)")
    p.add_argument("--right-motor-y-cm", type=float, default=None, help="Optional right motor Y (cm)")
    p.add_argument("--motor-size-cm", type=float, default=4.2, help="Motor body size, e.g. 42mm => 4.2cm")

    p.add_argument("--grid-cm", type=float, default=1.0, help="Grid step in cm (default 1cm)")
    p.add_argument("--out-dir", type=str, default="./fivebar_out")
    p.add_argument("--name", type=str, default="fivebar_40x40")
    return p


def main() -> None:
    args = build_parser().parse_args()

    if args.left_motor_x_cm >= args.right_motor_x_cm:
        raise ValueError("left_motor_x_cm must be less than right_motor_x_cm")

    left_motor_y_cm = args.motor_y_cm if args.left_motor_y_cm is None else args.left_motor_y_cm
    right_motor_y_cm = args.motor_y_cm if args.right_motor_y_cm is None else args.right_motor_y_cm

    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    results, xs, ys, state_map = run_scan(
        width_cm=args.width_cm,
        height_cm=args.height_cm,
        l1_cm=args.l1_cm,
        l2_cm=args.l2_cm,
        left_motor_x_cm=args.left_motor_x_cm,
        right_motor_x_cm=args.right_motor_x_cm,
        left_motor_y_cm=left_motor_y_cm,
        right_motor_y_cm=right_motor_y_cm,
        grid_cm=args.grid_cm,
    )

    csv_path = out_dir / f"{args.name}_states.csv"
    png_path = out_dir / f"{args.name}_reachability.png"

    save_csv(csv_path, results)

    if HAS_MPL:
        save_plot(
            png_path,
            xs,
            ys,
            state_map,
            args.left_motor_x_cm,
            args.right_motor_x_cm,
            left_motor_y_cm,
            right_motor_y_cm,
            args.l1_cm,
            args.l2_cm,
            args.motor_size_cm,
        )
        png_info = str(png_path)
    else:
        png_info = "matplotlib not installed; plot not generated"

    reachable_n = sum(r.reachable for r in results)
    total_n = len(results)
    print("Scan completed")
    print(f"Grid points: {total_n}, reachable: {reachable_n}, ratio: {reachable_n/total_n:.3f}")
    print(f"CSV: {csv_path}")
    print(f"PNG: {png_info}")


if __name__ == "__main__":
    main()
