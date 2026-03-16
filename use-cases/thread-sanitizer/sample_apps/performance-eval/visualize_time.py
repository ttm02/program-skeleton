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


def data_from_csv():
    files_csv = glob(DATAPATH + "/compile_time" + "/*.csv")
    df = pd.concat(
        (
            d[d["exit_code"].isna() | (d["exit_code"].astype(str) == "0")]
            for d in (pd.read_csv(f) for f in files_csv)
        ),
        ignore_index=True,
    )
    df["mode_readable"] = df["mode"].replace(mode_mapping)
    df["app_readable"] = df["app"].replace(app_mapping)
    df["time"] = df["time"].astype(float)
    return df


def create_lineplot(df, pdf):
    height = 0.88 + 1.23 * len(df["app"].unique())
    fig, ax = plt.subplots(figsize=(6.5, height))

    sns.set(style="whitegrid")
    sns.boxplot(
        ax=ax,
        data=df,
        x="time",
        y="app_readable",
        hue="mode_readable",
        hue_order=mode_order,
        palette=mode_to_color_plot,
    )

    ax.set_title("Sample Apps: Compile Time")
    ax.set_xlabel("Compilation Time (in seconds)")
    ax.set_ylabel("App")

    ax.legend(title="Compile Options", borderaxespad=0)
    ax.legend_.remove()

    plt.tight_layout()
    pdf.savefig(fig, bbox_inches="tight", pad_inches=0.05)
    plt.close()


def visualize_runtime():
    df = data_from_csv()
    file_name = f"CompileTime_boxplot.pdf"
    with PdfPages(file_name) as pdf:
        create_lineplot(df, pdf)

        df_lulesh = df[df["app"] == "lulesh"]
        df_hpccg = df[df["app"] == "hpccg"]
        df_1 = pd.concat((df_lulesh, df_hpccg))
        create_lineplot(df_1, pdf)

        df_tealeaf = df[df["app"] == "miniamr"]
        df_miniamr = df[df["app"] == "tealeaf"]
        df_kripke = df[df["app"] == "kripke"]
        df_2 = pd.concat((df_tealeaf, df_miniamr, df_kripke))
        create_lineplot(df_2, pdf)

        print(f"Saving {file_name}")


if __name__ == "__main__":
    main()
