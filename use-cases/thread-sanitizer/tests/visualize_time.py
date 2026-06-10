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

from visualize_common import visualize_common as VISC

visc = VISC()


def usage():
    print(f"Usage: {sys.argv[0]} [results dir]", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        usage()

    visualize_runtime()


def data_from_csv():
    DATAPATH = sys.argv[1]
    files_DRB = DATAPATH + "/DRB_time"
    files_combined = files_DRB + "_combined.csv"

    if os.path.isfile(files_combined):
        # if cache file exist, use it (speed up)
        df = pd.read_csv(files_combined)
    else:
        files_csv = glob(files_DRB + "/*.csv")
        df = pd.concat((pd.read_csv(f) for f in files_csv), ignore_index=True)
        df["mode_readable"] = df["mode"].replace(visc.mode_mapping)
        df["time"] = df["time"].astype(float)
        df.to_csv(files_combined)

    return df


def create_lineplot(df, pdf):
    fig, ax = plt.subplots(figsize=(6.5, 7))

    sns.lineplot(
        data=df,
        x="threads",
        y="time",
        hue="mode_readable",
        hue_order=visc.mode_order,
        palette=visc.mode_to_color_plot,
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
    global visc
    visc = VISC(False)
    get_lineplot(df, "all")

    ### create better visibility what slicing or static analysis achieves
    ### TSAN without slicing has really very much overhead

    # show only slow methods
    mal_with_slicing = visc.mode_apply_list_all.copy()
    mal_with_slicing[:] = [s for s in mal_with_slicing if s.startswith("slicing")]
    visc.set_mode_lists(mal_with_slicing)
    df_with_slicing = df[df["mode"].str.contains("slicing")]
    get_lineplot(df_with_slicing, "without_orig_tsan")

    visc = VISC(False)

    # show only fast methods
    mal_no_slicing = visc.mode_apply_list_all.copy()
    mal_no_slicing[:] = [s for s in mal_no_slicing if not s.startswith("slicing")]
    visc.set_mode_lists(mal_no_slicing)
    df_no_slicing = df[~df["mode"].str.contains("slicing")]
    get_lineplot(df_no_slicing, "without_slicing")

    visc = VISC(False)

    # compare static analysis with TSAN and slicing
    tsan_stan = "single+merge+loop"
    tsan_stan_text = "TSAN + static analysis"
    tsan_stan_old_text = visc.mode_mapping[tsan_stan]
    slicing_stan = "slicing+" + tsan_stan
    slicing_stan_text = "TSAN + slicing + static analysis"
    slicing_stan_old_text = visc.mode_mapping[slicing_stan]
    mal_plus_analysis = [
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
    get_lineplot(df_plus_analysis, "plus_analysis")


def visualize_runtime():
    df = data_from_csv()
    get_plot(df)


if __name__ == "__main__":
    main()
