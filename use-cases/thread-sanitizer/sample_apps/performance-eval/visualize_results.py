#!/usr/bin/env python3

import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.scale import FuncScale
from io import StringIO
from glob import glob


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    visualize_hpccg()
    visualize_lulesh()


colors = [
    "#DE3210",  # "vanilla"
    "#AA4466",  # "TSAN"
    "#BB9933",  # "TSAN + slicing"
    "#2277CD",  # "TSAN + slicing + static analysis"
]
mode_mapping = {
    "norm": "vanilla",
    "orig": "TSAN",
    "pass": "TSAN + slicing",
    "stan": "TSAN + slicing + static analysis",
}
mode_order = list(mode_mapping.values())
mode_to_color = dict(zip(mode_mapping.keys(), colors))
mode_to_color_plot = dict(zip(mode_order, colors))


# get largest size there all modes are present
def get_largest_value_for_group(df, col_name):
    def inner_loop(col_value):
        size_list = (
            df[df[col_name] == col_value]
            .groupby("mode")
            .size()
            .reset_index(name="count")
        )
        if len(size_list) < len(mode_order):
            return False

        size_list_max_count = size_list["count"].max()

        for sl in size_list["count"]:
            if sl < size_list_max_count * 0.9:
                return False

        return True

    for col_value in sorted(df[col_name].unique(), reverse=True):
        if inner_loop(col_value):
            return col_value

    return df[col_name].max()


def get_plot_problem_size(df, name, ax1, plt, y_offset):
    max_threads = get_largest_value_for_group(df, "threads")
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
    ax1.legend(title="Application Runtime")

    mean_times = df.groupby(["size", "mode"])["time"].mean().reset_index()
    pivoted = mean_times.pivot(index="size", columns="mode", values="time")
    percentages = pivoted.div(pivoted["norm"], axis=0)

    # Annotate slowdown on plots
    for s in percentages.index:
        for mode in ["orig", "pass", "stan"]:
            time_val = df[
                (df["threads"] == max_threads)
                & (df["size"] == s)
                & (df["mode"] == mode)
            ]["time"].median()
            slowdown = percentages.loc[s, mode]
            color = mode_to_color[mode]
            # draw slowdown multiplicator
            # below for static analysis
            if mode == "stan":
                offset = y_offset
                va = "bottom"
            else:
                offset = -y_offset
                va = "top"

            ax1.text(
                s,
                time_val + offset,
                f"{slowdown:.1f}×",
                ha="center",
                va=va,
                fontsize=8,
                color=color,
            )


def get_plot_thread_number(df, name, ax2, plt, y_offset):
    t_show = [0]
    t_show += [1, 2, 3, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96]
    t_show += [100]
    x_levels = np.array(t_show)

    # Forward: value -> position
    def forward(x):
        return np.interp(x, x_levels, np.arange(len(x_levels)))

    # Inverse: position -> value
    def inverse(x):
        return np.interp(x, np.arange(len(x_levels)), x_levels)

    # Register custom scale
    ax2.set_xscale(FuncScale(ax2, (forward, inverse)))
    ax2.set_xticks(x_levels)
    ax2.set_xticklabels(x_levels)

    max_size = get_largest_value_for_group(df, "size")
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
    ax2.legend(title="Application Runtime")

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

            # draw slowdown multiplicator
            # below for static analysis
            offset = y_offset
            va = "top"
            slow_factor = f"{slowdown:.0f}×" if 10 < slowdown else f"{slowdown:.1f}×"

            ax2.text(
                thread,
                time_val + offset,
                slow_factor,
                ha="center",
                va="center",
                fontsize=8,
                color=color,
            )


def get_plot(df, name, pdf_name, plotter):
    fig, ax = plt.subplots(figsize=(6.5, 7))

    # Compute offset for label positions
    y_min, y_max = ax.get_ylim()
    y_offset = 0.1 * (y_max - y_min)

    plotter(df, name, ax, plt, y_offset)

    plt.tight_layout()
    plt.savefig(f"{name}_{pdf_name}.pdf")
    print(f"Saving {name}_{pdf_name}.pdf")


def create_plots(df, name):
    df["mode_readable"] = df["mode"].replace(mode_mapping)

    get_plot(df, name, "problem_size", get_plot_problem_size)
    get_plot(df, name, "thread_count", get_plot_thread_number)


def data_from_csv(name):
    DATAPATH = sys.argv[1]
    files = glob(DATAPATH + "/" + name + "/*.csv")
    df = pd.concat((pd.read_csv(f) for f in files), ignore_index=True)
    return df


def visualize_hpccg():
    df_hpccg = data_from_csv("hpccg")
    df_hpccg["size"] = df_hpccg["config"].str.extract(r"(\d+)").astype(int)
    create_plots(df_hpccg, "HPCCG")


def visualize_lulesh():
    df_lulesh = data_from_csv("lulesh")
    df_lulesh["size"] = df_lulesh["config"].str.extract(r"_s_(\d+)").astype(int)
    df_lulesh["iterations"] = df_lulesh["config"].str.extract(r"_i_(\d+)").astype(int)
    create_plots(df_lulesh, "LULESH")


if __name__ == "__main__":
    main()
