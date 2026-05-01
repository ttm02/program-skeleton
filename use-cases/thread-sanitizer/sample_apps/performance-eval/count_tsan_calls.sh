#!/usr/bin/env bash

compile_options=(
    "orig"
    "passthrough"
    "loop"
    "merge"
    "merge+loop"
    "single"
    "single+loop"
    "single+merge"
    "single+merge+loop"
    "slicing"
    "slicing+loop"
    "slicing+merge"
    "slicing+merge+loop"
    "slicing+single"
    "slicing+single+loop"
    "slicing+single+merge"
    "slicing+single+merge+loop"
)

apps=()
for ef in $(realpath build-perf-tests/use-cases/thread-sanitizer/sample_apps/*_orig.exe); do
    echo -en "\t&\t"
    name=$(echo "$ef" | awk -F'/' '{print $NF}' | cut -d'_' -f1 | tr -d '\n')
    echo -n "$name"
    apps+=("$name")
done
echo ""

for co in "${compile_options[@]}"; do
    for a in "${apps[@]}"; do
        echo -en "\t&\t"
        exe_file=$(realpath build-perf-tests/use-cases/thread-sanitizer/sample_apps/"${a}"_"${co}".exe)
        # collect all jumps to tsan function labels, but exclude the jump targets itself
        objdump -d "$exe_file" 2>/dev/null | grep -vE ':$' | grep -c '__tsan_' | tr -d '\n'
    done
    echo -en "\t\t"
    echo "${co}"
done
