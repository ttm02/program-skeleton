#!/usr/bin/env python3

import os
import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.scale import FuncScale
from matplotlib.backends.backend_pdf import PdfPages
from glob import glob

DATAPATH = sys.argv[1]
files_DRB = DATAPATH + "/DRB_time"
df = None


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    visualize_runtime()


colors = [
    "#FF0000",
    "#FF00FF",
    "#8B4513",
    "#FF7F50",
    "#FF69B4",
    "#FFA500",
    "#708090",
    "#FFD700",
    "#FFFF00",
    "#00BFFF",
    "#0000FF",
    "#00FF00",
    "#00FFFF",
    "#7FFF00",
    "#008080",
    "#800080",
    "#BA55D3",
    "#D3BA55",
]

mode_list = [
    "loop",
    "merge",
    "merge+loop",
    "single",
    "single+loop",
    "single+merge",
    "single+merge+loop",
    "slicing",
    "slicing+loop",
    "slicing+merge",
    "slicing+merge+loop",
    "slicing+single",
    "slicing+single+loop",
    "slicing+single+merge",
    "slicing+single+merge+loop",
]
mode_mapping = {
    "orig": "TSAN",
    "passthrough": "TSAN (pass, but all disabled)",
}
for ml in mode_list:
    mode_mapping[ml] = "TSAN + " + ml.replace("+", " + ")

mode_order = list(mode_mapping.values())
mode_to_color = dict(zip(mode_mapping.keys(), colors))
mode_to_color_plot = dict(zip(mode_order, colors))

# backup to reset after filtering
mode_to_color_plot_all = mode_to_color_plot.copy()
mode_apply_list_all = list(mode_mapping.keys())
mode_order_all = mode_order.copy()
mode_mapping_all = mode_mapping.copy()


def data_from_csv():
    files_combined = files_DRB + "_combined.csv"

    if os.path.isfile(files_combined):
        # if cache file exist, use it (speed up)
        df = pd.read_csv(files_combined)
    else:
        files_csv = glob(files_DRB + "/*.csv")
        df = pd.concat((pd.read_csv(f) for f in files_csv), ignore_index=True)
        df["mode_readable"] = df["mode"].replace(mode_mapping)
        df["time"] = df["time"].astype(float)
        df.to_csv(files_combined)

    return df


def get_level_scale(ax, x_levels):

    # Forward: value -> position
    def forward(x):
        return np.interp(x, x_levels, np.arange(len(x_levels)))

    # Inverse: position -> value
    def inverse(x):
        return np.interp(x, np.arange(len(x_levels)), x_levels)

    return FuncScale(ax, (forward, inverse))


def create_lineplot(df, pdf):
    fig, ax = plt.subplots(figsize=(6.5, 7))

    sns.lineplot(
        data=df,
        x="threads",
        y="time",
        hue="mode_readable",
        hue_order=mode_order,
        palette=mode_to_color_plot,
        style="mode_readable",
        marker=True,
        ax=ax,
    )

    ax.set_title("DataRaceBench: Runtime")
    ax.set_xlabel("Thread Count")
    ax.set_ylabel("Runtime (in seconds)")
    ax.legend(title="Compile Options", borderaxespad=0)

    plt.tight_layout()
    pdf.savefig(fig, bbox_inches="tight", pad_inches=0.05)
    plt.close()


def get_lineplot(df, pdf_name):
    file_name = f"DRB_Runtime_lineplot_{pdf_name}.pdf"
    with PdfPages(file_name) as pdf:
        create_lineplot(df, pdf)
        print(f"Saving {file_name}")


def get_plot(df):
    get_lineplot(df, "all")

    ### create better visibility what slicing or static analysis achieves
    ### TSAN without slicing has really very much overhead

    def reset_lists():
        global mode_mapping, mode_to_color_plot, mode_order, mode_to_color_plot_all
        mode_to_color_plot = mode_to_color_plot_all.copy()
        mode_mapping = mode_mapping_all.copy()
        mode_order = mode_order_all.copy()

    def set_mode_lists(mal_list):
        global mode_mapping, mode_to_color_plot, mode_order, mode_to_color_plot_all
        mode_to_color_plot = {}
        mode_order = []
        for v in mal_list:
            vra = mode_mapping[v]
            mode_to_color_plot[vra] = mode_to_color_plot_all[vra]
            mode_order.append(vra)

    # show only slow methods
    mal_with_slicing = mode_apply_list_all.copy()
    mal_with_slicing[:] = [s for s in mal_with_slicing if s.startswith("slicing")]
    set_mode_lists(mal_with_slicing)
    df_with_slicing = df[df["mode"].str.contains("slicing")]
    get_lineplot(df_with_slicing, "without_orig_tsan")

    # show only fast methods
    mal_no_slicing = mode_apply_list_all.copy()
    mal_no_slicing[:] = [s for s in mal_no_slicing if not s.startswith("slicing")]
    set_mode_lists(mal_no_slicing)
    df_no_slicing = df[~df["mode"].str.contains("slicing")]
    get_lineplot(df_no_slicing, "without_slicing")

    reset_lists()

    # compare static analysis with TSAN and slicing
    global mode_mapping, mode_to_color_plot, mode_to_color_plot_all
    mode_to_color_plot_backup = mode_to_color_plot_all.copy()
    tsan_stan = "single+merge+loop"
    tsan_stan_text = "TSAN + static analysis"
    tsan_stan_old_text = mode_mapping[tsan_stan]
    slicing_stan = "slicing+" + tsan_stan
    slicing_stan_text = "TSAN + slicing + static analysis"
    slicing_stan_old_text = mode_mapping[slicing_stan]
    mal_plus_analysis = [
        "orig",
        tsan_stan,
        "slicing",
        slicing_stan,
    ]
    mode_mapping[tsan_stan] = tsan_stan_text
    mode_to_color_plot[tsan_stan_text] = mode_to_color_plot[tsan_stan_old_text]
    mode_to_color_plot_all[tsan_stan_text] = mode_to_color_plot[tsan_stan_text]
    mode_mapping[slicing_stan] = slicing_stan_text
    mode_to_color_plot[slicing_stan_text] = mode_to_color_plot[slicing_stan_old_text]
    mode_to_color_plot_all[slicing_stan_text] = mode_to_color_plot[slicing_stan_text]
    set_mode_lists(mal_plus_analysis)
    df_plus_analysis = df[df["mode"].isin(mal_plus_analysis)]
    df_plus_analysis.loc[df_plus_analysis["mode"] == tsan_stan, "mode_readable"] = (
        tsan_stan_text
    )
    df_plus_analysis.loc[df_plus_analysis["mode"] == slicing_stan, "mode_readable"] = (
        slicing_stan_text
    )
    get_lineplot(df_plus_analysis, "plus_analysis")

    mode_to_color_plot_all = mode_to_color_plot_backup.copy()
    reset_lists()


def visualize_runtime():
    df = data_from_csv()
    get_plot(df)


if __name__ == "__main__":
    main()
