"""Build the data driven figures for the paper as PNG files.

These figures come from the real experiment outputs in results/. They are drawn
with matplotlib because they need heatmaps and histograms that are easier here
than in pgfplots. Each figure is saved into figures/ and is loaded by the paper
with \\includegraphics.

Figures produced:
  fig_vtable.png        DP value table heatmap with the optimal path.
  fig_headtohead.png    Greedy vs DP schedule on the same day.
  fig_profit_dist.png    Distribution of daily profit for the three methods.
  fig_price_context.png  Mean hourly price curve and a day by hour heatmap.
"""

import os
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import colors

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
RES = os.path.join(ROOT, "results")
FIG = os.path.join(ROOT, "figures")
DPI = 200

plt.rcParams.update({
    "font.size": 9,
    "axes.titlesize": 9,
    "axes.labelsize": 9,
    "legend.fontsize": 8,
    "xtick.labelsize": 8,
    "ytick.labelsize": 8,
    "figure.constrained_layout.use": True,
})


def fig_vtable():
    """DP value table heatmap. x is stage t, y is SoC level. Color is the value
    to go V_t(s). The black line is the optimal path that the schedule follows.
    """
    df = pd.read_csv(os.path.join(RES, "example_day_vtable.csv"))
    n_soc = df["soc_level"].max() + 1
    n_stage = df["stage"].max() + 1
    grid = np.full((n_soc, n_stage), np.nan)
    for _, r in df.iterrows():
        if not pd.isna(r["value"]):
            grid[int(r["soc_level"]), int(r["stage"])] = r["value"]

    path = pd.read_csv(os.path.join(RES, "example_day_path.csv"))

    fig, ax = plt.subplots(figsize=(5.0, 2.7))
    # Origin lower so SoC grows upward.
    im = ax.imshow(grid, origin="lower", aspect="auto", cmap="viridis",
                   interpolation="nearest")
    ax.plot(path["stage"], path["soc_level"], color="white", lw=2.2)
    ax.plot(path["stage"], path["soc_level"], color="black", lw=1.0,
            marker="o", markersize=2.5, label="Optimal path")
    ax.set_xlabel("Stage $t$ (hour)")
    ax.set_ylabel("SoC level $s$")
    ax.set_title("DP value table $V_t(s)$ and reconstructed optimal path")
    # Map SoC levels to fractions for readability.
    yt = [0, (n_soc - 1) // 2, n_soc - 1]
    ax.set_yticks(yt)
    ax.set_yticklabels(["0", "0.5", "1.0"])
    cb = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.02)
    cb.set_label("Value to go (EUR)")
    ax.legend(loc="lower left", framealpha=0.8)
    fig.savefig(os.path.join(FIG, "fig_vtable.png"), dpi=DPI)
    plt.close(fig)
    print("wrote fig_vtable.png")


def fig_headtohead():
    """Greedy vs DP on the same day. Two panels share the price line. Bars are
    the net action. The text shows the profit and cycles for each method.
    """
    dp = pd.read_csv(os.path.join(RES, "example_day.csv"))
    gr = pd.read_csv(os.path.join(RES, "example_day_greedy.csv"))
    cmp = pd.read_csv(os.path.join(RES, "example_day_compare.csv")).set_index("method")

    fig, axes = plt.subplots(1, 2, figsize=(6.6, 2.8), sharey=True)
    for ax, sched, name, key in [
        (axes[0], gr, "Greedy threshold", "greedy"),
        (axes[1], dp, "Dynamic programming", "dp"),
    ]:
        axb = ax.twinx()
        # Price line on the right axis.
        axb.plot(sched["hour"], sched["price"], color="tab:blue", lw=1.5,
                 marker="o", markersize=2, label="Price")
        axb.set_ylim(40, 130)
        # Net action bars and SoC line on the left axis.
        ax.bar(sched["hour"], sched["net_action"], width=0.7,
               color="tab:green", alpha=0.55, label="Charge(+)/Discharge(-)")
        ax.plot(sched["hour"], sched["soc"], color="tab:red", lw=1.3,
                marker="s", markersize=2, label="SoC")
        ax.set_ylim(-0.65, 1.15)
        ax.set_xlabel("Hour of day")
        net = cmp.loc[key, "net_profit"]
        cyc = cmp.loc[key, "full_cycles"]
        ax.set_title(f"{name}\nnet {net:.1f} EUR, {cyc:.1f} cycle")
        if ax is axes[1]:
            axb.set_ylabel("Price (EUR/MWh)")
        if ax is axes[0]:
            ax.set_ylabel("SoC and action (MWh)")
    # One shared legend.
    h1, l1 = axes[0].get_legend_handles_labels()
    axes[0].legend(h1, l1, loc="lower left", framealpha=0.85)
    fig.savefig(os.path.join(FIG, "fig_headtohead.png"), dpi=DPI)
    plt.close(fig)
    print("wrote fig_headtohead.png")


def fig_profit_dist():
    """Distribution of daily true net profit for the three methods over 364
    days. A box plot plus the share of profitable days.
    """
    d = pd.read_csv(os.path.join(RES, "e1_daily.csv"))
    order = ["dp_deg", "dp_nodeg", "greedy"]
    labels = ["DP with wear", "DP without wear", "Greedy"]
    data = [d[d.method == m]["true_net_profit"].values for m in order]

    fig, ax = plt.subplots(figsize=(5.0, 2.8))
    bp = ax.boxplot(data, orientation="vertical", patch_artist=True,
                    showfliers=True, widths=0.55,
                    flierprops=dict(marker=".", markersize=3, alpha=0.4))
    cols = ["tab:green", "tab:red", "tab:orange"]
    for patch, c in zip(bp["boxes"], cols):
        patch.set_facecolor(c)
        patch.set_alpha(0.55)
    for med in bp["medians"]:
        med.set_color("black")
    ax.axhline(0, color="gray", lw=0.8, ls="--")
    ax.set_xticklabels(labels)
    ax.set_ylabel("Daily true net profit (EUR)")
    ax.set_title("Distribution of daily profit over 364 days")
    # Annotate the share of profitable days.
    for i, m in enumerate(order):
        vals = d[d.method == m]["true_net_profit"].values
        share = 100 * (vals > 0).mean()
        ax.text(i + 1, ax.get_ylim()[1] * 0.92, f"{share:.0f}% > 0",
                ha="center", va="top", fontsize=8)
    fig.savefig(os.path.join(FIG, "fig_profit_dist.png"), dpi=DPI)
    plt.close(fig)
    print("wrote fig_profit_dist.png")


def fig_price_context():
    """Two panels. Left: mean price for each hour of the day, with a band for
    the 25th to 75th percentile. Right: a heatmap of price by day and hour over
    the year, to show the daily and seasonal pattern.
    """
    df = pd.read_csv(os.path.join(ROOT, "data", "processed", "prices_2019.csv"))
    hours = [f"h{h:02d}" for h in range(24)]
    mat = df[hours].to_numpy()  # shape (days, 24)

    fig, axes = plt.subplots(1, 2, figsize=(6.6, 2.7))

    # Left: mean hourly curve with a percentile band.
    mean_h = mat.mean(axis=0)
    p25 = np.percentile(mat, 25, axis=0)
    p75 = np.percentile(mat, 75, axis=0)
    x = np.arange(24)
    axes[0].fill_between(x, p25, p75, color="tab:blue", alpha=0.2,
                         label="25-75 pct")
    axes[0].plot(x, mean_h, color="tab:blue", lw=1.8, marker="o",
                 markersize=2.5, label="Mean")
    axes[0].set_xlabel("Hour of day")
    axes[0].set_ylabel("Price (EUR/MWh)")
    axes[0].set_title("Mean daily price shape")
    axes[0].legend(loc="upper left", framealpha=0.85)

    # Right: heatmap day (x) by hour (y). Diverging map centered at zero to show
    # the negative price hours.
    vmax = np.percentile(np.abs(mat), 99)
    norm = colors.TwoSlopeNorm(vmin=-vmax, vcenter=0, vmax=vmax)
    im = axes[1].imshow(mat.T, origin="lower", aspect="auto", cmap="RdBu_r",
                        norm=norm, interpolation="nearest")
    axes[1].set_xlabel("Day of year")
    axes[1].set_ylabel("Hour")
    axes[1].set_title("Price by day and hour, 2019")
    axes[1].set_yticks([0, 6, 12, 18, 23])
    cb = fig.colorbar(im, ax=axes[1], fraction=0.046, pad=0.02)
    cb.set_label("EUR/MWh")
    fig.savefig(os.path.join(FIG, "fig_price_context.png"), dpi=DPI)
    plt.close(fig)
    print("wrote fig_price_context.png")


def main():
    os.makedirs(FIG, exist_ok=True)
    fig_vtable()
    fig_headtohead()
    fig_profit_dist()
    fig_price_context()
    print("All matplotlib figures written to figures/")


if __name__ == "__main__":
    main()
