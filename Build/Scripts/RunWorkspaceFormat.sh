#!/usr/bin/env bash
set -euo pipefail

# For ChatGPT workspaces with mounted, non-executable clang-format 18.1.8.
# Both inputs are external tooling; neither binary is committed to SIREngine.
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
clang_source="${SIR_WORKSPACE_CLANG_FORMAT:-/mnt/data/clang-format}"
tinfo_source="${SIR_WORKSPACE_LIBTINFO:-/mnt/data/libtinfo.so.5}"

if [[ ! -f "${clang_source}" || ! -f "${tinfo_source}" ]]; then
    echo "Required workspace clang-format or libtinfo.so.5 is missing" >&2
    exit 1
fi

tool_dir="$(mktemp -d)"
trap 'rm -rf "${tool_dir}"' EXIT
cp "${clang_source}" "${tool_dir}/clang-format"
cp "${tinfo_source}" "${tool_dir}/libtinfo.so.5"
chmod 755 "${tool_dir}/clang-format"

export LD_LIBRARY_PATH="${tool_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
cmake "-DSIR_CLANG_FORMAT=${tool_dir}/clang-format" -P "${root}/Build/CMake/CIFormat.cmake"
