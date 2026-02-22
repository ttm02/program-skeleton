case "$SLURM_ARRAY_TASK_ID" in
0)
    MY_STAN_PASS_MODE="orig"
    MY_STAN_PASS_MODE_ARGS=""
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
    MY_STAN_PASS_MODE="slicing+single+merge"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge"
    ;;
4)
    MY_STAN_PASS_MODE="slicing+single+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge,loop"
    ;;
5)
    MY_STAN_PASS_MODE="slicing+merge"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=merge"
    ;;
6)
    MY_STAN_PASS_MODE="slicing+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=merge,loop"
    ;;
7)
    MY_STAN_PASS_MODE="slicing+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=loop"
    ;;
8)
    MY_STAN_PASS_MODE="passthrough"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing"
    ;;
9)
    MY_STAN_PASS_MODE="single"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single"
    ;;
10)
    MY_STAN_PASS_MODE="single+merge"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,merge"
    ;;
11)
    MY_STAN_PASS_MODE="single+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,merge,loop"
    ;;
12)
    MY_STAN_PASS_MODE="merge"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=merge"
    ;;
13)
    MY_STAN_PASS_MODE="merge+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=merge,loop"
    ;;
14)
    MY_STAN_PASS_MODE="loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=loop"
    ;;
*) exit 1 ;;
esac

export MY_STAN_PASS_MODE
export MY_STAN_PASS_MODE_ARGS
