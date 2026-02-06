#!/usr/bin/env python3

import os
import re
import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.scale import FuncScale
from matplotlib.backends.backend_pdf import PdfPages
from io import StringIO
from glob import glob

DATAPATH = sys.argv[1]
files_DRB = DATAPATH + "/DRB"
df = None


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    visualize_accuracy()


colors = [
    "#AA0000",  # "TSAN"
    "#008800",  # "TSAN + slicing"
    "#0000AA",  # "TSAN + slicing + static analysis"
]
mode_mapping = {
    "orig": "TSAN",
    "pass": "TSAN + slicing",
    "stan": "TSAN + slicing + static analysis",
}
mode_order = list(mode_mapping.values())
mode_to_color = dict(zip(mode_mapping.keys(), colors))
mode_to_color_plot = dict(zip(mode_order, colors))


def data_from_csv():
    files_combined = files_DRB + "_combined.csv"

    if os.path.isfile(files_combined):
        # if cache file exist, use it (speed up)
        df = pd.read_csv(files_combined)
    else:
        files_csv = glob(files_DRB + "/*.csv")
        df = pd.concat((pd.read_csv(f) for f in files_csv), ignore_index=True)
        df["mode_readable"] = df["mode"].replace(mode_mapping)

        # categorize testcases
        df["tc_cat"] = [
            "yes" if re.match(".*-yes\\..*", tc) else "no" for tc in df["testcase"]
        ]

        # save to speed up later loads
        df.to_csv(files_combined)

    return df


def split_dataframe(df, chunk_size):
    def df_chunk_filter(df, cn_list):
        return df[df["testcase"].isin(cn_list)]

    df_cn = []
    for tc_name in sorted(df["testcase"].unique()):
        if len(df_cn) == chunk_size:
            yield df_chunk_filter(df, df_cn)
            df_cn = []
        df_cn.append(str(tc_name))

    yield df_chunk_filter(df, df_cn)


def create_boxplot(df, pdf, pdf_name):
    df_tc_count = df["testcase"].nunique()
    fig_factor = max(1 - df_tc_count * 0.04, 0)
    fig_height = df_tc_count * (0.32 + fig_factor * 0.06) + (1.0 + fig_factor * 0.2)
    fig, ax = plt.subplots(figsize=(7.6, fig_height))

    x_ticks = np.arange(0, 100 + 1, 10)
    x_levels = np.concatenate(([-5], x_ticks, [105]), axis=None)

    # Forward: value -> position
    def forward(x):
        return np.interp(x, x_levels, np.arange(len(x_levels)))

    # Inverse: position -> value
    def inverse(x):
        return np.interp(x, np.arange(len(x_levels)), x_levels)

    ax.set_xscale(FuncScale(ax, (forward, inverse)))
    ax.set_xticks(x_levels)
    ax.set_xticklabels(x_levels)

    ax.set_xlim(-1, 101)
    # ax.set_xbound(0, 100)
    # ax.autoscale(axis="x", enable=False)
    # ax.margins(x=5, tight=False)

    ax.yaxis.set_label_position("right")
    ax.yaxis.tick_right()

    sns.set(style="whitegrid")
    sns.boxplot(
        ax=ax,
        data=df,
        x="df_value_count",
        y="testcase",
        hue="mode_readable",
        hue_order=mode_order,
        palette=mode_to_color_plot,
    )
    ax.set_title(f"DataRaceBench ({pdf_name}): Accuracy")
    ax.set_xlabel("Detection Percentage (in %)")
    ax.set_ylabel("Testcase\nNames" if fig_height < 2.63 else "Testcase Names")

    ax.legend(
        title="", bbox_to_anchor=(1.05, -0.025), loc="upper left", borderaxespad=0
    )

    plt.tight_layout()
    pdf.savefig(fig)
    plt.close()


def create_heat(df, pdf, pdf_name):
    df = df.copy()
    fig, ax = plt.subplots(figsize=(33.3, 1.82))

    heatmap_data = df.pivot_table(
        index="mode_readable",
        columns="threads",
        values="df_value_count",
        aggfunc="first",
    )
    sns.heatmap(
        heatmap_data, annot=True, fmt=".0f", cmap="YlOrRd", cbar=False
    )  # cmap="viridis"

    tc_name = df["testcase"].iloc[0]
    tc_case = (
        "expecting at least one data race"
        if pdf_name == "yes"
        else "expecting no data races"
    )

    ax.set_title(f"{tc_name}: Detection Percentage ({tc_case})")
    ax.set_xlabel("Thread Count")
    ax.set_ylabel("")

    plt.tight_layout()
    pdf.savefig(fig)
    plt.close()


def get_plot(df, pdf_name):
    file_name = f"DRB_Accuracy_{pdf_name}_boxplot.pdf"
    with PdfPages(file_name) as pdf:
        # create dummy page as first page as it often renders not so nice
        create_boxplot(
            df[df["testcase"] == df["testcase"].iloc[0]], pdf, "to be ignored"
        )
        for df_chunk in split_dataframe(df, 18):
            create_boxplot(df_chunk, pdf, pdf_name)
        print(f"Saving {file_name}")

    file_name = f"DRB_Accuracy_{pdf_name}_heatmap.pdf"
    with PdfPages(file_name) as pdf:
        for df_chunk in split_dataframe(df, 1):
            create_heat(df_chunk, pdf, pdf_name)
        print(f"Saving {file_name}")


def get_df_tc_cat(cat):
    files_cat = files_DRB + "_" + cat + ".csv"
    if os.path.isfile(files_cat):
        # if cache file exist, use it (speed up)
        return pd.read_csv(files_cat)

    global df
    if df is None:
        df = data_from_csv()

    df_cat = (
        df[df["tc_cat"] == cat]
        .assign(is_cat=df["found"].eq(cat))
        .groupby(["testcase", "mode_readable", "threads"])["is_cat"]
        .mean()
        .mul(100)
        .reset_index(name="df_value_count")
    )
    # df_cat = df_cat[df_cat["found"] == cat]

    # only show the interesting cases
    df_cat = df_cat[
        df_cat.groupby("testcase")["df_value_count"].transform(
            lambda x: (x < 99.0).any()
        )
    ]

    # save to speed up later loads
    df_cat.to_csv(files_cat)
    return df_cat


def visualize_accuracy():
    df_no = get_df_tc_cat("no")
    get_plot(df_no, "no")

    df_yes = get_df_tc_cat("yes")
    get_plot(df_yes, "yes")


if __name__ == "__main__":
    main()
