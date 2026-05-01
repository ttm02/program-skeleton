#!/usr/bin/env python3

import os
import re
import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from io import StringIO
from glob import glob

from visualize_common import visualize_common as VISC

visc = VISC()
df = None


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    global files_DRB
    DATAPATH = sys.argv[1]
    files_DRB = DATAPATH + "/DRB"
    visualize_accuracy()


def data_from_csv():
    files_combined = files_DRB + "_combined.csv"

    if os.path.isfile(files_combined):
        # if cache file exist, use it (speed up)
        df = pd.read_csv(files_combined)
    else:
        files_csv = glob(files_DRB + "/*.csv")
        df = pd.concat((pd.read_csv(f) for f in files_csv), ignore_index=True)
        df["mode_readable"] = df["mode"].replace(visc.mode_mapping)

        # categorize testcases
        df["tc_cat"] = [
            "yes" if re.match(".*-yes\\..*", tc) else "no" for tc in df["testcase"]
        ]

        # save to speed up later loads
        df.to_csv(files_combined)

    return df


def split_dataframe_internal(df, chunk_size):
    def df_chunk_filter(df, cn_list):
        return df[df["testcase"].isin(cn_list)]

    df_cn = []
    for tc_name in sorted(df["testcase"].unique(), reverse=True):
        if len(df_cn) == chunk_size:
            yield df_chunk_filter(df, df_cn)
            df_cn = []
        df_cn.append(str(tc_name))

    yield df_chunk_filter(df, df_cn)


def split_dataframe(df, chunk_size):
    stack = []
    for item in split_dataframe_internal(df, chunk_size):
        stack.append(item)
    while stack:
        yield stack.pop()


def create_boxplot(df, pdf, pdf_name):
    df_tc_count = df["testcase"].nunique()
    fig_factor = max(1 - df_tc_count * 0.05, 0)
    fig_height = df_tc_count * (1.125 + fig_factor * 0.04) + (0.03 + fig_factor * 0.05)
    fig, ax = plt.subplots(figsize=(7.6, fig_height))

    x_ticks = np.arange(0, 100 + 1, 10)
    x_levels = np.concatenate(([-5], x_ticks, [105]), axis=None)

    ax.set_xscale(VISC.get_level_scale(ax, x_levels))
    ax.set_xticks(x_levels)
    ax.set_xticklabels(x_levels)
    ax.set_xlim(-1, 101)

    ax.yaxis.set_label_position("right")
    ax.yaxis.tick_right()

    sns.set(style="whitegrid")
    sns.boxplot(
        ax=ax,
        data=df,
        x="df_value_count",
        y="testcase",
        hue="mode_readable",
        hue_order=visc.mode_order,
        palette=visc.mode_to_color_plot,
    )
    ax.set_title(f"DataRaceBench ({pdf_name}): Accuracy")
    ax.set_xlabel("Detection Percentage (in %)")
    ax.set_ylabel("Testcase\nNames" if fig_height < 2.63 else "Testcase Names")

    ax.legend(title="", bbox_to_anchor=(1.05, -0.01), loc="upper left", borderaxespad=0)

    if df_tc_count == 1:
        handles, labels = ax.get_legend_handles_labels()
        fig = plt.figure(figsize=(4, 2))
        fig.legend(handles, labels, loc="center")
    else:
        ax.legend_.remove()

    plt.tight_layout()
    pdf.savefig(fig, bbox_inches="tight", pad_inches=0.05)
    plt.close()


def create_heat(df, pdf, all_labels=True, y_labels=True, ax_title=""):
    df = df.copy()
    fig_height = 4.2 if all_labels else 2.2
    fig_width = 7.25 if y_labels else 4.7
    if not all_labels:
        fig_width *= 0.88
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))

    bin_mapping = {
        2: "2",
        3: "3",
        4: "4",
        5: "5",
        6: "6-7",
        8: "8-11",
        12: "12-15",
        16: "16-23",
        24: "24-35",
        36: "36-59",
        60: "60-96",
    }
    bin_keys = list(bin_mapping.keys()) + [np.inf]
    bin_labels = list(bin_mapping.values())
    df["threads_binned"] = pd.cut(
        df["threads"],
        bins=bin_keys,
        labels=bin_labels,
        include_lowest=True,
        right=False,  # [x_i, x_{i+1})
    )
    heatmap_data = df.pivot_table(
        index="mode_readable",
        columns="threads_binned",
        values="df_value_count",
        aggfunc="mean",
        observed=True,
    )

    annot = heatmap_data.copy()
    annot = annot.map(
        lambda x: f"{x:.1f}" if pd.notna(x) and x < 99.95 else f"{int(x)}"
    )

    sns.heatmap(
        heatmap_data,
        vmin=0.0,
        vmax=100.0,
        fmt="",
        annot=annot,
        annot_kws={"va": "center", "ha": "center"},
        cmap="viridis",  # cmap="RdYlGn"
        cbar=False,
    )

    if ax_title == "":
        tc_name = df["testcase"].iloc[0]
        ax.set_title(f"{tc_name}:")
    else:
        ax.set_title(ax_title)

    ax.set_xlabel("Thread Count")
    ax.set_ylabel("")

    ax.xaxis.tick_top()
    ax.xaxis.set_label_position("top")
    ax.tick_params(axis="x", length=0)
    for label in ax.get_xticklabels():
        if len(label.get_text()) > 3:
            label.set_rotation(25)
            label.set_ha("left")
            label.set_rotation_mode("anchor")

    ax.yaxis.tick_right()
    ax.yaxis.set_label_position("right")
    ax.tick_params(axis="y", right=False, labelright=True)

    if y_labels:
        ax.set_yticklabels(
            ax.get_yticklabels(),
            rotation=0,
            rotation_mode="anchor",
            # va="bottom",
            ha="left",
        )
    else:
        ax.set_yticklabels([])

    plt.tight_layout()
    pdf.savefig(fig, bbox_inches="tight", pad_inches=0.05)
    plt.close()


def get_boxplot(df, cat_name, pdf_name):
    file_name = f"DRB_Accuracy_{cat_name}_{pdf_name}_boxplot.pdf"
    with PdfPages(file_name) as pdf:
        # create dummy page as first page as it often renders not so nice
        create_boxplot(
            df[df["testcase"] == df["testcase"].iloc[0]], pdf, "to be ignored"
        )
        chunk_size = 8 if pdf_name == "all" else 5
        for df_chunk in split_dataframe(df, chunk_size):
            create_boxplot(df_chunk, pdf, cat_name)
        print(f"Saving {file_name}")


def get_heatmap_files(df, file_name, all_labels=True):
    iter_tc = file_name.startswith("DRB_Accuracy_all_all_heatmap")
    mean_tfi = " "
    if "_yes_" in file_name:
        mean_tfi = " yes "
    elif "_no_" in file_name:
        mean_tfi = " no "
    mean_text = f"Mean over all{mean_tfi}Testcases"

    if iter_tc:
        with PdfPages(file_name + ".pdf") as pdf:
            for df_chunk in split_dataframe(df, 1):
                create_heat(df_chunk, pdf, all_labels, True)
            print(f"Saving {file_name}.pdf")

    with PdfPages(file_name + "_mean" + ".pdf") as pdf:
        create_heat(df, pdf, all_labels, True, mean_text)
        print(f"Saving {file_name}_mean.pdf")

    if iter_tc:
        with PdfPages(file_name + "_no_desc" + ".pdf") as pdf:
            for df_chunk in split_dataframe(df, 1):
                create_heat(df_chunk, pdf, all_labels, False)
            print(f"Saving {file_name}_no_desc.pdf")

    with PdfPages(file_name + "_mean" + "_no_desc" + ".pdf") as pdf:
        create_heat(df, pdf, all_labels, False, mean_text)
        print(f"Saving {file_name}_mean_no_desc.pdf")


def get_heatmap(df, name):
    file_name = f"DRB_Accuracy_{name}_all_heatmap"
    get_heatmap_files(df, file_name)

    stan_mapping_text = {}
    for mm in visc.mode_mapping:
        text = visc.mode_mapping[mm]
        if mm.startswith("slicing+"):
            stan_mapping_text[text] = "TSAN + slicing + static analysis"
        elif "+" in mm or mm == "loop" or mm == "merge" or mm == "single":
            stan_mapping_text[text] = "TSAN + static analysis"
        else:
            stan_mapping_text[text] = text

    so = sorted(set(stan_mapping_text.values()))
    so_1 = [x for x in so if not " + " in x]
    so_2 = [x for x in so if x.startswith("TSAN + static")]
    so_3 = [x for x in so if x.startswith("TSAN + slicing")]
    stan_order = so_1 + so_2 + so_3

    df["mode_readable"] = pd.Categorical(
        df["mode_readable"].replace(stan_mapping_text),
        categories=stan_order,
        ordered=True,
    )

    get_heatmap_files(df, file_name + "_only_stan", False)


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

    # save to speed up later loads
    df_cat.to_csv(files_cat)
    return df_cat


def focus_on_interesting_testcases(df):
    df_focus = df.copy()
    # only show the interesting cases
    df_filtered = df_focus[
        df.groupby("testcase")["df_value_count"].transform(
            lambda x: (1.0 < x).any() and (x < 99.0).any()
        )
    ]
    return df_filtered


def visualize_accuracy():
    df_no = get_df_tc_cat("no")
    df_no_filtered = focus_on_interesting_testcases(df_no)
    get_boxplot(df_no, "no", "all")
    get_boxplot(df_no_filtered, "no", "filtered")

    df_yes = get_df_tc_cat("yes")
    df_yes_filtered = focus_on_interesting_testcases(df_yes)
    get_boxplot(df_yes, "yes", "all")
    get_boxplot(df_yes_filtered, "yes", "filtered")

    df_yes_specific = df_yes[
        df_yes["testcase"].str.startswith(("DRB114", "DRB185", "DRB187"))
    ]
    get_boxplot(df_yes_specific, "yes", "specific")

    df_all = pd.concat([df_yes, df_no], ignore_index=True)
    df_all = df_all.sort_values(by="testcase")
    get_heatmap(df_all, "all")
    get_heatmap(df_yes, "yes")
    get_heatmap(df_no, "no")


if __name__ == "__main__":
    main()
