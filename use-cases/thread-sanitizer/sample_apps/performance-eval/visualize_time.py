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
    "miniamr": "miniAMR",
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
    file_name = f"CompileTime_boxplot.pdf"
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
