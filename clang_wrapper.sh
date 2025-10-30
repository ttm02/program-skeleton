#!/bin/bash
# wrapper to invoke clang for compilation with the as using multiple object files
compiler=clang++
#compiler= variable needs to be set on line 3 as it will be replaced with clang or clang++ depending if c or cpp wrapper is generated

 LD_PRELOAD_PREV="$LD_PRELOAD"
if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
    echo "INVOKE CLANG_WRAPPER"
    echo "clang_wrapper $@"
    export LD_PRELOAD="$(clang -print-file-name=libclang_rt.asan.so):$LD_PRELOAD_PREV"
fi

if [ "$USE_COMPILER_PASS" == 1 ]; then
USE_COMPILER_PASS=true
fi

USE_COMPILER_PASS=${USE_COMPILER_PASS:false}

is_to_obj=false
has_o_option=false
has_o_files=false
has_flto=false
has_fwhole_program_vtables=false
has_opt_lvl=false
has_src_file=false
has_multiple_src_file=false
for arg in "$@"; do
    # Check if the current argument is "-c"
    if [ "$arg" == "-c" ]; then
        is_to_obj=true
    elif [ "$arg" == "-o" ]; then
        has_o_option=true
    elif [[ "$arg" == *.o ]]; then
        has_o_files=true
    elif [ "$arg" == "-flto" ]; then
        has_flto=true
    elif [ "$arg" == "-fwhole-program-vtables" ]; then
        has_fwhole_program_vtables=true
    elif [ "$arg" == "-O1" ] || [ "$arg" == "-O2" ] || [ "$arg" == "-O3" ]; then
        has_opt_lvl=true
    elif [[ "$arg" == *.c ]] || [[ "$arg" == *.cpp ]]  || [[ "$arg" == *.cc ]] || [[ "$arg" == *.cxx ]]; then
        if [ "$has_src_file" == true ]; then
            has_multiple_src_file=true
        fi
        has_src_file=true
    fi
done

# check if necessary flags are given
if [ "$USE_COMPILER_PASS" == true ] &&
    ( [ "$has_flto" == false ] ||
   [ "$has_fwhole_program_vtables" == false ] || [ "$has_opt_lvl" == false ] ); then
    echo "Error, need -flto and -fwhole-program-vtables and at least -O1 for pass to work correctly"
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit 1
fi

if [ "$USE_COMPILER_PASS" == true ] && ( ! [[ -v COMPILER_PASS ]] ); then
    echo "The COMPILER_PASS environment variable is not set"
    export LD_PRELOAD="$LD_PRELOAD_PREV"
    exit 1
fi

COMPILER_INVOCATION="$compiler"
if [[ "$USE_COMPILER_PASS" == true ]]; then
    COMPILER_INVOCATION="$COMPILER_INVOCATION -Wl,--load-pass-plugin=$COMPILER_PASS -Wl,-mllvm=-load=$COMPILER_PASS -lprecompute"
fi
for arg in "$@"; do
    COMPILER_INVOCATION="$COMPILER_INVOCATION $arg"
done
if [ "$DEBUG_CLANG_WRAPPER" == true ]; then
    echo "$COMPILER_INVOCATION"
fi
$COMPILER_INVOCATION
export LD_PRELOAD="$LD_PRELOAD_PREV"
exit

