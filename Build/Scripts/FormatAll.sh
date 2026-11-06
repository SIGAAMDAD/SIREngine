#!/usr/bin/env bash
# Validate only staged, SIREngine-owned C/C++ paths. Full-tree audit is explicit.
set -euo pipefail

if [[ $# -gt 1 || ( $# -eq 1 && "$1" != "--pre-commit" && "$1" != "--check" && "$1" != "--apply" && "$1" != "--full" ) ]]; then
    printf 'Usage: %s [--pre-commit|--check|--apply|--full]\n' "$0" >&2
    exit 2
fi
mode="${1:---check}"
root="$(git rev-parse --show-toplevel)" || exit 1
cd "$root"

if [[ "$mode" == "--full" ]]; then
    exec cmake -DSIR_FORMAT_FULL=ON -P Build/CMake/CIFormat.cmake
fi

files=()
while IFS= read -r -d '' path; do
    case "$path" in
        Source/*.cpp|Source/*.cc|Source/*.c|Source/*.h|Source/*.hpp|Tests/*.cpp|Tests/*.cc|Tests/*.c|Tests/*.h|Tests/*.hpp|Tools/*.cpp|Tools/*.cc|Tools/*.c|Tools/*.h|Tools/*.hpp)
            files+=( "$path" )
            ;;
    esac
done < <(git diff --cached --name-only --diff-filter=ACMR -z -- Source Tests Tools)

if (( ${#files[@]} == 0 )); then
    printf 'FormatAll: PASS (no staged owned C/C++ files)\n'
    exit 0
fi

if command -v clang-format-23 >/dev/null 2>&1; then
    formatter="$(command -v clang-format-23)"
elif command -v clang-format >/dev/null 2>&1; then
    formatter="$(command -v clang-format)"
else
    printf 'FormatAll: clang-format 23.1.5 is required\n' >&2
    exit 1
fi
version="$("$formatter" --version)"
if [[ ! "$version" =~ (^|[^0-9])23\.1\.5([^0-9]|$) ]]; then
    printf 'FormatAll: expected clang-format 23.1.5, got: %s\n' "$version" >&2
    exit 1
fi

source_file="$(mktemp)"
formatted_file="$(mktemp)"
trap 'rm -f "$source_file" "$formatted_file"' EXIT

for path in "${files[@]}"; do
    if [[ "$mode" == "--apply" ]]; then
        if ! git diff --quiet -- "$path"; then
            printf 'FormatAll: unstaged edits in %s; stage or revert first\n' "$path" >&2
            exit 1
        fi
        "$formatter" -i --style=file -- "$root/$path"
        continue
    fi
    git show ":$path" > "$source_file"
    "$formatter" --style=file --assume-filename="$root/$path" < "$source_file" > "$formatted_file"
    if ! cmp -s "$source_file" "$formatted_file"; then
        printf 'FormatAll: staged source requires formatting: %s\n' "$path" >&2
        exit 1
    fi
done
if [[ "$mode" == "--apply" ]]; then
    printf 'FormatAll: formatted %s staged owned C/C++ paths; re-stage changed files\n' "${#files[@]}"
else
    printf 'FormatAll: PASS (%s staged owned C/C++ paths, %s)\n' "${#files[@]}" "$version"
fi
