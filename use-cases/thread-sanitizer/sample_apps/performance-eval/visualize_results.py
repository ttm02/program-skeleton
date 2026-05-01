#!/usr/bin/env python3

import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from io import StringIO
from glob import glob

from visualize_common import visualize_common as VISC

visc = VISC()


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    visualize_lulesh()
    visualize_hpccg()
    visualize_tealeaf()
    visualize_kripke()


# get largest size there all modes are present
def get_largest_value_for_group(df, col_name):
    def inner_loop(col_value):
        size_list = (
            df[df[col_name] == col_value]
            .groupby("mode")
            .size()
            .reset_index(name="count")
        )
        if len(size_list) < len(visc.mode_order):
            return False

        size_list_max_count = size_list["count"].max()

        for sl in size_list["count"]:
            if sl < size_list_max_count * 0.95:
                return False

        return True

    for col_value in sorted(df[col_name].unique(), reverse=True):
        if inner_loop(col_value):
            return col_value

    return df[col_name].max()


def annotate_overhead_factor(
    df, ax, col, other_col, max_other_col, mode_apply_list, y_offset
):

    def get_format_slowdown(slowdown):
        if slowdown < 10:
            return f"{slowdown:.1f}×"
        else:
            return f"{slowdown:.0f}×"

    def get_va(mode):
        if "static" in visc.mode_mapping[mode]:
            return "top"
        else:
            return "bottom"

    def get_offset(mode):
        if "static" in visc.mode_mapping[mode]:
            return -y_offset
        else:
            return y_offset

    df = df.copy()
    df = df[df[other_col] == max_other_col]

    mean_times = df.groupby([col, "mode"])["time"].mean().reset_index()
    pivoted = mean_times.pivot(index=col, columns="mode", values="time")
    percentages = pivoted.div(pivoted["vanilla"], axis=0)

    # Annotate slowdown on plots
    for i in percentages.index:
        for mode in mode_apply_list:
            if mode == "vanilla":
                continue
            if not (mode in df["mode"].unique()):
                continue

            time_val = df[(df[col] == i) & (df["mode"] == mode)]["time"].median()

            slowdown = percentages.loc[i, mode]
            color = visc.mode_to_color[mode]

            ax.text(
                i,
                time_val + get_offset(mode),
                get_format_slowdown(slowdown),
                ha="center",
                va=get_va(mode),
                fontsize=8,
                color=color,
            )


def create_plot_problem_size(df, name, ax1, plt, y_offset, mal):
    max_threads = get_largest_value_for_group(df, "threads")
    sns.lineplot(
        data=df[df["threads"] == max_threads],
        x="size",
        y="time",
        hue="mode_readable",
        hue_order=visc.mode_order,
        palette=visc.mode_to_color_plot,
        style="mode_readable",
        marker=True,
        ax=ax1,
    )
    ax1.set_title(f"{name}: Overhead vs Problem size ({max_threads} threads)")
    ax1.set_xlabel("Problem Size")
    ax1.set_ylabel("Time (s)")
    ax1.legend(title="Application Runtime")

    # try to make gap left and right very small, but zero looks ugly
    sizes = list(df["size"].unique())
    steps = int((1 / len(sizes)) * 20)
    p_show = [sizes[0] - 1]
    last = sizes[0]
    p_show += [last]
    for i in sizes[1:]:
        dist = i - last
        for _ in range(steps - 1):
            last += dist / steps
            p_show += [last]
        p_show += [i]
        last = i
    p_show += [sizes[-1] + 1]
    x_levels = np.array(p_show)

    # set labels only for size values on x axis
    ax1.set_xscale(VISC.get_level_scale(ax1, x_levels))
    ax1.set_xticks(sizes)
    ax1.set_xticklabels(sizes)

    def gfs(slowdown):
        return f"{slowdown:.1f}×"

    def gva(mode):
        if "static" in mode_mapping[mode]:
            return "top"
        else:
            return "bottom"

    def goff(mode):
        if "static" in mode_mapping[mode]:
            return -y_offset
        else:
            return y_offset

    annotate_overhead_factor(df, ax1, "size", "threads", max_threads, mal, y_offset)


def get_plot_thread_number(df, name, ax2, plt, y_offset, mal):
    t_show = [0]
    t_show += [1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 24, 32, 48, 64, 80, 96]
    t_show += [100]
    x_levels = np.array(t_show)

    # Register custom scale
    ax2.set_xscale(VISC.get_level_scale(ax2, x_levels))
    ax2.set_xticks(x_levels)
    ax2.set_xticklabels(x_levels)

    max_size = get_largest_value_for_group(df, "size")
    sns.lineplot(
        data=df[df["size"] == max_size],
        x="threads",
        y="time",
        hue="mode_readable",
        hue_order=visc.mode_order,
        palette=visc.mode_to_color_plot,
        style="mode_readable",
        marker=True,
        ax=ax2,
    )
    ax2.set_title(f"{name}: Overhead vs Number of Threads (size = {max_size})")
    ax2.set_xlabel("Number of Threads")
    ax2.set_ylabel("Time (s)")
    ax2.legend(title="Application Runtime")

    annotate_overhead_factor(df, ax2, "threads", "size", max_size, mal, y_offset)


def save_plot(df, name, pdf_name, name_ext, plotter, mode_apply_list):
    fig, ax = plt.subplots(figsize=(6.5, 7))

    # Compute offset for label positions
    y_min, y_max = ax.get_ylim()
    y_offset = 0.2 * (y_max - y_min)

    plotter(df, name, ax, plt, y_offset, mode_apply_list)
    plt.tight_layout()

    plt.savefig(f"{name}_{pdf_name}{name_ext}.pdf")
    print(f"Saving {name}_{pdf_name}{name_ext}.pdf")
    plt.close()


def get_plot(df, name, pdf_name, plotter):
    global visc
    visc = VISC()
    save_plot(df, name, pdf_name, "", plotter, visc.mode_apply_list_all)

    ### create better visibility what slicing or static analysis achieves
    ### TSAN without slicing has really very much overhead

    # show only slow methods
    mal_with_slicing = visc.mode_apply_list_all.copy()
    mal_with_slicing[:] = [s for s in mal_with_slicing if s.startswith("slicing")]
    mal_with_slicing.insert(0, "vanilla")
    visc.set_mode_lists(mal_with_slicing)
    df_1 = df[df["mode"].str.contains("slicing")]
    df_2 = df[df["mode"] == "vanilla"]
    df_with_slicing = pd.concat((df_1, df_2), ignore_index=True)
    save_plot(
        df_with_slicing, name, pdf_name, "_without_orig_tsan", plotter, mal_with_slicing
    )

    visc = VISC()

    # show only fast methods
    mal_no_slicing = visc.mode_apply_list_all.copy()
    mal_no_slicing.remove("vanilla")
    mal_no_slicing[:] = [s for s in mal_no_slicing if not s.startswith("slicing")]
    visc.set_mode_lists(mal_no_slicing)
    df_no_slicing = df[~df["mode"].str.contains("slicing")]
    save_plot(
        df_no_slicing, name, pdf_name, "_without_slicing", plotter, mal_no_slicing
    )

    visc = VISC()

    # compare static analysis with TSAN and slicing
    tsan_stan = "single+merge+loop"
    tsan_stan_text = "TSAN + static analysis"
    tsan_stan_old_text = visc.mode_mapping[tsan_stan]
    slicing_stan = "slicing+" + tsan_stan
    slicing_stan_text = "TSAN + slicing + static analysis"
    slicing_stan_old_text = visc.mode_mapping[slicing_stan]
    mal_plus_analysis = [
        "vanilla",
        "orig",
        tsan_stan,
        "slicing",
        slicing_stan,
    ]
    visc.mode_mapping[tsan_stan] = tsan_stan_text
    visc.mode_to_color_plot[tsan_stan_text] = visc.mode_to_color_plot[
        tsan_stan_old_text
    ]
    visc.mode_mapping[slicing_stan] = slicing_stan_text
    visc.mode_to_color_plot[slicing_stan_text] = visc.mode_to_color_plot[
        slicing_stan_old_text
    ]
    visc.set_mode_lists(mal_plus_analysis)
    df_plus_analysis = df[df["mode"].isin(mal_plus_analysis)]
    df_plus_analysis.loc[df_plus_analysis["mode"] == tsan_stan, "mode_readable"] = (
        tsan_stan_text
    )
    df_plus_analysis.loc[df_plus_analysis["mode"] == slicing_stan, "mode_readable"] = (
        slicing_stan_text
    )
    save_plot(
        df_plus_analysis, name, pdf_name, "_plus_analysis", plotter, mal_plus_analysis
    )


def create_plots(df, name):
    get_plot(df, name, "thread_count", get_plot_thread_number)
    get_plot(df, name, "problem_size", create_plot_problem_size)


def data_from_csv(name):
    DATAPATH = sys.argv[1]
    files = glob(DATAPATH + "/" + name + "/*.csv")
    df = pd.concat(
        (
            d[
                d["exit_code"].isna()
                | (d["exit_code"].astype(str) == "0")
                | ((d["name"] == "tealeaf") & (d["exit_code"].astype(str) == "1"))
            ]
            for d in (pd.read_csv(f) for f in files)
        ),
        ignore_index=True,
    )
    df["mode_readable"] = df["mode"].replace(VISC.mode_mapping)
    return df


def visualize_lulesh():
    df_lulesh = data_from_csv("lulesh")
    df_lulesh["cfg_size"] = df_lulesh["config"].str.extract(r"_s_(\d+)").astype(int)
    df_lulesh["cfg_iter"] = df_lulesh["config"].str.extract(r"_i_(\d+)").astype(int)
    name = "LULESH"

    df_lulesh["size"] = (
        df_lulesh["cfg_size"].astype(str)
        + "s+"
        + df_lulesh["cfg_iter"].astype(str)
        + "i"
    )
    get_plot(df_lulesh, name, "thread_count", get_plot_thread_number)

    df_lulesh["size"] = df_lulesh["cfg_size"]
    max_iter = get_largest_value_for_group(df_lulesh, "cfg_iter")
    df_lulesh = df_lulesh[df_lulesh["cfg_iter"] == max_iter]
    get_plot(df_lulesh, name, "problem_size", create_plot_problem_size)


def visualize_hpccg():
    df_hpccg = data_from_csv("hpccg")
    df_hpccg["size"] = df_hpccg["config"].str.extract(r"(\d+)").astype(int)
    create_plots(df_hpccg, "HPCCG")


def visualize_miniamr():
    df_miniamr = data_from_csv("miniamr")
    df_miniamr["size"] = df_miniamr["config"].str.extract(r"__nx_(\d+)_").astype(int)
    create_plots(df_miniamr, "miniAMR")


def visualize_tealeaf():
    df_tealeaf = data_from_csv("tealeaf")
    df_tealeaf["cfg_resolution"] = (
        df_tealeaf["config"].str.extract(r"(\d+)_").astype(int)
    )
    df_tealeaf["cfg_steps"] = df_tealeaf["config"].str.extract(r"_(\d+)").astype(int)
    name = "TeaLeaf"

    df_tealeaf["size"] = (
        df_tealeaf["cfg_resolution"].astype(str)
        + "r+"
        + df_tealeaf["cfg_steps"].astype(str)
        + "s"
    )
    get_plot(df_tealeaf, name, "thread_count", get_plot_thread_number)

    df_tealeaf["size"] = df_tealeaf["cfg_resolution"]
    max_iter = get_largest_value_for_group(df_tealeaf, "cfg_steps")
    df_tealeaf = df_tealeaf[df_tealeaf["cfg_steps"] == max_iter]
    get_plot(df_tealeaf, name, "problem_size", create_plot_problem_size)


def visualize_kripke():
    df_kripke = data_from_csv("kripke")
    df_kripke["size"] = df_kripke["config"].str.extract(r"__zones_(\d+)_").astype(int)
    create_plots(df_kripke, "Kripke")


if __name__ == "__main__":
    main()
