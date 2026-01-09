#!/usr/bin/env python3

import sys
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from io import StringIO


colors = ["#DDAA33", "#BB5566", "#004488", "#3377FF"]


def get_plot(df, name):
    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(12, 5))

    mode_mapping = {
        "norm": "vanilla",
        "orig": "TSAN",
        "pass": "TSAN + slicing",
        "stan": "TSAN + slicing + static analysis",
    }
    mode_order = list(mode_mapping.values())
    df["mode_readable"] = df["mode"].replace(mode_mapping)
    mode_to_color = dict(zip(mode_mapping.keys(), colors))
    mode_to_color_plot = dict(zip(mode_order, colors))

    max_threads = df["threads"].max()
    sns.lineplot(
        data=df[df["threads"] == max_threads],
        x="size",
        y="time",
        hue="mode_readable",
        hue_order=mode_order,
        palette=mode_to_color_plot,
        marker="o",
        ax=ax1,
    )
    ax1.set_title(f"{name}: Overhead vs Problem size ({max_threads} threads)")
    ax1.set_xlabel("Problem Size")
    ax1.set_ylabel("Time (s)")
    ax1.legend(title="Tsan Runtime")

    max_size = df["size"].max()
    sns.lineplot(
        data=df[df["size"] == max_size],
        x="threads",
        y="time",
        hue="mode_readable",
        hue_order=mode_order,
        palette=mode_to_color_plot,
        marker="o",
        ax=ax2,
    )
    ax2.set_title(f"{name}: Overhead vs Number of Threads (size = {max_size})")
    ax2.set_xlabel("Number of Threads")
    ax2.set_ylabel("Time (s)")
    ax2.legend(title="Tsan Runtime")

    # Compute offset for label positions
    y_min, y_max = ax1.get_ylim()
    y_offset = 0.03 * (y_max - y_min)

    mean_times = df.groupby(["size", "mode"])["time"].mean().reset_index()
    pivoted = mean_times.pivot(index="size", columns="mode", values="time")
    percentages = pivoted.div(pivoted["norm"], axis=0)

    # Annotate slowdown on plots
    for s in percentages.index:
        for mode in ["orig", "pass", "stan"]:
            time_val = df[
                (df["threads"] == NUM_T_TO_SHOW)
                & (df["size"] == s)
                & (df["mode"] == mode)
            ]["time"].median()
            slowdown = percentages.loc[s, mode]
            color = mode_to_color[mode]
            # Offset: above for normal, below for modified
            if mode == "orig":
                offset = y_offset
                va = "bottom"
            else:  # modified
                offset = -y_offset
                va = "top"

            ax1.text(
                s,
                time_val + offset,
                f"{slowdown:.0f}×",
                ha="center",
                va="center",
                fontsize=8,
                color=color,
            )

    mean_times = df.groupby(["threads", "mode"])["time"].mean().reset_index()
    pivoted = mean_times.pivot(index="threads", columns="mode", values="time")
    percentages = pivoted.div(pivoted["norm"], axis=0)

    # Annotate slowdown on plots
    for thread in percentages.index:
        for mode in ["orig", "pass", "stan"]:
            time_val = df[
                (df["size"] == max_size)
                & (df["threads"] == thread)
                & (df["mode"] == mode)
            ]["time"].median()
            slowdown = percentages.loc[thread, mode]
            color = mode_to_color[mode]
            # Offset: above for normal, below for modified
            if mode == "orig":
                offset = y_offset
                va = "bottom"
            else:  # modified
                offset = -y_offset
                va = "top"

            ax2.text(
                thread,
                time_val + offset,
                f"{slowdown:.0f}×",
                ha="center",
                va="center",
                fontsize=8,
                color=color,
            )

    plt.tight_layout()
    plt.savefig(f"{name}.pdf")


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    args = sys.argv[1:]
    DATAPATH = args[0]

    # HPCCG
    df_hpccg = pd.read_csv(DATAPATH + "/hpccg.csv")
    df_hpccg["size"] = df_hpccg["config"].str.extract(r"(\d+)").astype(int)
    get_plot(df_hpccg, "HPCCG")

    # LULESH
    df_lulesh = pd.read_csv(DATAPATH + "/lulesh.csv")
    df_lulesh["size"] = df_lulesh["config"].str.extract(r"_s_(\d+)").astype(int)
    df_lulesh["iterations"] = df_lulesh["config"].str.extract(r"_i_(\d+)").astype(int)
    get_plot(df_lulesh, "LULESH")


if __name__ == "__main__":
    main()
