case "$SLURM_ARRAY_TASK_ID" in
0)
    MY_STAN_PASS_MODE="orig"
    MY_STAN_PASS_MODE_ARGS="--disable-precompute-pass"
    ;;
1)
    MY_STAN_PASS_MODE="slicing"
    MY_STAN_PASS_MODE_ARGS=""
    ;;
2)
    MY_STAN_PASS_MODE="slicing+single"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single"
    ;;
3)
    MY_STAN_PASS_MODE="slicing+single+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,loop"
    ;;
4)
    MY_STAN_PASS_MODE="slicing+single+merge"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge"
    ;;
5)
    MY_STAN_PASS_MODE="slicing+single+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge,loop"
    ;;
6)
    MY_STAN_PASS_MODE="slicing+merge"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=merge"
    ;;
7)
    MY_STAN_PASS_MODE="slicing+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=merge,loop"
    ;;
8)
    MY_STAN_PASS_MODE="slicing+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=loop"
    ;;
9)
    MY_STAN_PASS_MODE="passthrough"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing"
    ;;
10)
    MY_STAN_PASS_MODE="single"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single"
    ;;
11)
    MY_STAN_PASS_MODE="single+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,loop"
    ;;
12)
    MY_STAN_PASS_MODE="single+merge"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,merge"
    ;;
13)
    MY_STAN_PASS_MODE="single+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,merge,loop"
    ;;
14)
    MY_STAN_PASS_MODE="merge"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=merge"
    ;;
15)
    MY_STAN_PASS_MODE="merge+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=merge,loop"
    ;;
16)
    MY_STAN_PASS_MODE="loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=loop"
    ;;
17)
    MY_STAN_PASS_MODE="vanilla"
    MY_STAN_PASS_MODE_ARGS=""
    ;;
*) exit 1 ;;
esac

export MY_STAN_PASS_MODE
export MY_STAN_PASS_MODE_ARGS
