#!/usr/bin/env python3

import os
import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from glob import glob

from visualize_common import visualize_common as VISC

visc = VISC()


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    visualize_runtime()


app_mapping = {
    "lulesh": "LULESH",
    "hpccg": "HPCCG",
    "tealeaf": "TeaLeaf",
    "kripke": "Kripke",
}


def data_from_csv():
    DATAPATH = sys.argv[1]
    files_csv = glob(DATAPATH + "/compile_time" + "/*.csv")
    df = pd.concat(
        (
            d[d["exit_code"].isna() | (d["exit_code"].astype(str) == "0")]
            for d in (pd.read_csv(f) for f in files_csv)
        ),
        ignore_index=True,
    )
    df["mode_readable"] = df["mode"].replace(visc.mode_mapping)
    df["app_readable"] = df["app"].replace(app_mapping)
    df["time"] = df["time"].astype(float)
    return df


def create_heatmap(df, pdf):
    df = df.copy()
    fig, ax = plt.subplots(figsize=(10.3, 3.45))

    heatmap_data = df.pivot_table(
        index="app_readable",
        columns="mode_readable",
        values="time",
        aggfunc="mean",
        observed=True,
    )
    heatmap_data = heatmap_data.reindex(
        columns=visc.mode_order, index=app_mapping.values()
    )

    annot = heatmap_data.copy()
    annot = annot.map(lambda x: f"{x:.1f}")

    heatmap_norm = heatmap_data.apply(
        lambda c: (c - c.min()) / (c.max() - c.min()), axis=1
    )

    sns.heatmap(
        heatmap_norm,
        fmt="",
        annot=annot,
        annot_kws={"va": "center", "ha": "center"},
        cmap="viridis_r",  # cmap="RdYlGn"
        cbar=False,
    )

    ax.set_title("")  # Mean Compilation Time (in seconds)
    ax.set_xlabel("")
    ax.set_ylabel("")

    ax.xaxis.tick_top()
    ax.xaxis.set_label_position("top")
    ax.tick_params(axis="x", length=0)
    for label in ax.get_xticklabels():
        label.set_rotation(50)
        label.set_ha("left")
        label.set_rotation_mode("anchor")

    ax.yaxis.tick_left()
    ax.yaxis.set_label_position("left")
    ax.tick_params(axis="y", left=False, labelleft=True)
    for label in ax.get_yticklabels():
        label.set_rotation(0)
        label.set_ha("right")
        label.set_rotation_mode("anchor")

    plt.tight_layout()
    pdf.savefig(fig, bbox_inches="tight", pad_inches=0.05)
    plt.close()


def create_lineplot(df, pdf, show_legend=False):
    height = 0.88 + 1.23 * len(df["app"].unique())
    fig, ax = plt.subplots(figsize=(6.5, height))

    sns.set(style="whitegrid")
    sns.boxplot(
        ax=ax,
        data=df,
        x="time",
        y="app_readable",
        hue="mode_readable",
        hue_order=visc.mode_order,
        palette=visc.mode_to_color_plot,
        linewidth=1,
        medianprops=dict(color="black"),
        showfliers=False,  # do not show circles for strong derivations
        boxprops=dict(edgecolor="none"),  # no outline -> better color visibility
    )

    ax.set_title("Sample Apps: Compile Time")
    ax.set_xlabel("Compilation Time (in seconds)")
    ax.set_ylabel("")  # App

    ax.legend(title="Compile Options", borderaxespad=0)
    if show_legend:
        handles, labels = ax.get_legend_handles_labels()
        fig = plt.figure(figsize=(4, 2))
        fig.legend(handles, labels, loc="center")
    else:
        ax.legend_.remove()

    plt.tight_layout()
    pdf.savefig(fig, bbox_inches="tight", pad_inches=0.05)
    plt.close()


def visualize_runtime():
    df = data_from_csv()

    file_name = "CompileTime_heatmap.pdf"
    with PdfPages(file_name) as pdf:
        create_heatmap(df, pdf)
        print(f"Saving {file_name}")

    file_name = "CompileTime_boxplot.pdf"
    with PdfPages(file_name) as pdf:
        create_lineplot(df, pdf, True)
        create_lineplot(df, pdf)

        df_lulesh = df[df["app"] == "lulesh"]
        df_hpccg = df[df["app"] == "hpccg"]
        df_1 = pd.concat((df_lulesh, df_hpccg))
        create_lineplot(df_1, pdf)

        df_tealeaf = df[df["app"] == "tealeaf"]
        create_lineplot(df_tealeaf, pdf)

        df_kripke = df[df["app"] == "kripke"]
        create_lineplot(df_kripke, pdf)

        print(f"Saving {file_name}")


if __name__ == "__main__":
    main()
