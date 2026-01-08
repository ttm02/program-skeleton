#!/usr/bin/env bash

set -e

usage() {
    echo "$0 [image.tar.gz]"
    exit 1
}

INPUT_FILE="$1"
FILENAME_PREFIX="$HOME/myCont/precompute-devshell"
mkdir -p "$(dirname "$FILENAME_PREFIX")"

case "$INPUT_FILE" in
*.tar.gz)
    rm -f "${FILENAME_PREFIX}.tar"
    gunzip -c "$INPUT_FILE" >"${FILENAME_PREFIX}.tar"
    ;;
*.tar)
    if [ "$INPUT_FILE" != "${FILENAME_PREFIX}.tar" ]; then
        mv "$INPUT_FILE" "${FILENAME_PREFIX}.tar"
    fi
    ;;
*)
    usage
    ;;
esac

export TMPDIR=$HPC_SCRATCH
export APPTAINER_TMPDIR=${HPC_SCRATCH}
export NIX_BUILD_TOP="${FILENAME_PREFIX}-tmp"
mkdir -p "$NIX_BUILD_TOP"

singularity build --force "${FILENAME_PREFIX}.sif" docker-archive://"${FILENAME_PREFIX}.tar"

apptainer run \
    --scratch "$HPC_SCRATCH" \
    --mount "type=bind,source=${NIX_BUILD_TOP},destination=/build" \
    "${FILENAME_PREFIX}.sif" bash -c 'source $stdenv/setup; eval $shellHook; dumpVars'

mv "${NIX_BUILD_TOP}/env-vars" "${FILENAME_PREFIX}.env"
sed -i '/APPTAINER/d' "${FILENAME_PREFIX}.env"
sed -i 's|^declare -x ||g' "${FILENAME_PREFIX}.env"
sed -i '/^NIX_BUILD_TOP/s|^.*$|NIX_BUILD_TOP='"$HPC_SCRATCH"'|g' "${FILENAME_PREFIX}.env"
echo "noDumpEnvVars=1" >>"${FILENAME_PREFIX}.env"

rm -fr "$NIX_BUILD_TOP"
rm -f "${FILENAME_PREFIX}.tar"
rm -f "$INPUT_FILE"

echo "Successfully converted image and created env dump"
exit 0
