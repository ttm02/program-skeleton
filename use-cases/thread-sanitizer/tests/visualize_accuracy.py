#!/usr/bin/env python3

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
    DATAPATH = sys.argv[1]
    files = glob(DATAPATH + "/DRB/*.csv")
    df = pd.concat((pd.read_csv(f) for f in files), ignore_index=True)
    df["mode_readable"] = df["mode"].replace(mode_mapping)
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


def create_plot(df, pdf, pdf_name):
    fig, ax = plt.subplots(figsize=(7.6, 10.1))

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
    ax.set_ylabel("Testcase Names")
    ax.legend(title="")

    plt.tight_layout()
    pdf.savefig(fig)
    plt.close()


def get_plot(df, pdf_name):
    with PdfPages(f"DRB_Accuracy_{pdf_name}.pdf") as pdf:
        for df_chunk in split_dataframe(df, 26):
            create_plot(df_chunk, pdf, pdf_name)

    print(f"Saving DRB_Accuracy_{pdf_name}.pdf")


def visualize_accuracy():
    df = data_from_csv()

    # categorize testcases
    df["tc_cat"] = [
        "yes" if re.match(".*-yes\\..*", tc) else "no" for tc in df["testcase"]
    ]

    def get_df_tc_cat(df, cat):
        df_cat = (
            df[df["tc_cat"] == cat]
            .groupby(["testcase", "mode_readable", "threads"])["found"]
            .value_counts(cat)
            .reset_index(name="df_value_count")
        )
        df_cat["df_value_count"] = df_cat["df_value_count"].mul(100)
        return df_cat

    df_yes = get_df_tc_cat(df, "yes")
    df_no = get_df_tc_cat(df, "no")

    get_plot(df_yes, "yes")
    get_plot(df_no, "no")


if __name__ == "__main__":
    main()
