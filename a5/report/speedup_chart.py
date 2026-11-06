"""Draw the Q3.2/Q3.3 speedup chart used in Report.pdf.

Run: uv run --with matplotlib python report/speedup_chart.py
"""

from pathlib import Path

import matplotlib.pyplot as plt

CORES = [1, 2, 4, 8]
TIMES = {  # Q2 seconds: (local, ADA)
    "Given log (200 lines)": ([3.36, 3.61, 3.81, 5.30], [7.28, 7.93, 9.83, 15.36]),
    "Custom log (50,000 lines)": (
        [329.89, 184.38, 152.81, 128.90],
        [682.82, 420.77, 327.72, 308.58],
    ),
}
SURFACE, INK, INK2, GRID = "#ffffff", "#0b0b0b", "#52514e", "#e4e3df"
SERIES = [("Local", "#2a78d6", "o"), ("ADA", "#eb6834", "s")]

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 9, "text.color": INK})
fig, axes = plt.subplots(1, 2, figsize=(7.2, 3.0), facecolor=SURFACE)

for ax, (title, runs) in zip(axes, TIMES.items()):
    ax.set_facecolor(SURFACE)
    top = 8.5 if "Custom" in title else 1.2
    if "Custom" in title:
        ax.plot(CORES, CORES, color=INK2, lw=1, zorder=1)
        ax.text(7.6, 8.0, "ideal (linear)", color=INK2, ha="right", va="center", fontsize=8)
    ax.axhline(1, color=INK2, lw=1, zorder=1)
    for (name, color, marker), times in zip(SERIES, runs):
        speedup = [times[0] / t for t in times]
        ax.plot(CORES, speedup, color=color, lw=2, marker=marker, ms=7,
                mec=SURFACE, mew=2, solid_capstyle="round", zorder=3, label=name)
        ax.annotate(f"{speedup[-1]:.2f}×", (CORES[-1], speedup[-1]),
                    xytext=(6, (5 if name == "Local" else -5) if "Custom" in title else 0), textcoords="offset points", va="center",
                    color=INK, fontsize=8)
    ax.set_title(title, loc="left", fontsize=10, color=INK)
    ax.set_xticks(CORES)
    ax.set_xlim(0.7, 9.3)
    ax.set_ylim(0, top)
    ax.set_xlabel("Cores", color=INK2)
    ax.grid(axis="y", color=GRID, lw=1)
    ax.set_axisbelow(True)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(GRID)
    ax.tick_params(colors=INK2, length=0)

axes[0].set_ylabel("Speedup over 1 core", color=INK2)
axes[0].legend(frameon=False, loc="lower left", labelcolor=INK)
fig.tight_layout()
out = Path(__file__).with_name("speedup.svg")
fig.savefig(out, facecolor=SURFACE)
fig.savefig(out.with_suffix(".png"), dpi=150, facecolor=SURFACE)
print(f"wrote {out}")
