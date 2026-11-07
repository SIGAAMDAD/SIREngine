#!/usr/bin/env bash
# One pinned formatting implementation for staged, committed, or full-tree SIREngine-owned C/C++.
set -euo pipefail

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [--pre-commit|--check|--apply|--ci|--full]" >&2
    exit 2
fi
mode="${1:---check}"
case "$mode" in --pre-commit|--check|--apply|--ci|--full) ;; *) echo "Unsupported mode: $mode" >&2; exit 2 ;; esac
root="$(git rev-parse --show-toplevel)"
cd "$root"

if command -v clang-format-23 >/dev/null 2>&1; then
    formatter="$(command -v clang-format-23)"
elif command -v clang-format >/dev/null 2>&1; then
    formatter="$(command -v clang-format)"
else
    echo "clang-format 23.1.5 is required" >&2
    exit 1
fi
version="$("$formatter" --version)"
if [[ ! "$version" =~ (^|[^0-9])23\.1\.5([^0-9]|$) ]]; then
    echo "Expected clang-format 23.1.5, got: $version" >&2
    exit 1
fi

files=()
if [[ "$mode" == "--ci" ]]; then
    base="${SIR_FORMAT_BASE:-}"
    if [[ -n "${GITHUB_BASE_REF:-}" ]]; then
        if [[ -z "$base" ]]; then base="origin/$GITHUB_BASE_REF"; fi
        diff_base="$(git merge-base "$base" HEAD)" || { echo "Cannot resolve PR format base: $base" >&2; exit 1; }
    elif [[ -n "$base" ]]; then
        if [[ "$base" =~ ^0{40}$ ]]; then
            diff_base="$(git hash-object -t tree /dev/null)"
        else
            git cat-file -e "${base}^{commit}" 2>/dev/null || { echo "Cannot resolve push format base: $base" >&2; exit 1; }
            diff_base="$base"
        fi
    else
        echo "CI format comparison requires SIR_FORMAT_BASE on pushes or GITHUB_BASE_REF on pull requests" >&2
        exit 1
    fi
    while IFS= read -r -d '' path; do files+=( "$path" ); done < <(git diff --name-only --diff-filter=ACMR -z "$diff_base" HEAD -- Source Tests Tools)
elif [[ "$mode" == "--full" ]]; then
    while IFS= read -r -d '' path; do files+=( "$path" ); done < <(git ls-files -z -- Source Tests Tools)
else
    while IFS= read -r -d '' path; do files+=( "$path" ); done < <(git diff --cached --name-only --diff-filter=ACMR -z -- Source Tests Tools)
fi

selected=()
for path in "${files[@]}"; do
    case "$path" in
        Source/*.cpp|Source/*.cc|Source/*.c|Source/*.h|Source/*.hpp|Tests/*.cpp|Tests/*.cc|Tests/*.c|Tests/*.h|Tests/*.hpp|Tools/*.cpp|Tools/*.cc|Tools/*.c|Tools/*.h|Tools/*.hpp)
            selected+=( "$path" ) ;;
    esac
done

if (( ${#selected[@]} == 0 )); then echo "Formatting PASS (no selected owned C/C++ files)"; exit 0; fi
source_file="$(mktemp)"
formatted_file="$(mktemp)"
trap 'rm -f "$source_file" "$formatted_file"' EXIT
for path in "${selected[@]}"; do
    if [[ "$mode" == "--ci" || "$mode" == "--full" ]]; then
        cp -- "$path" "$source_file"
    else
        git show ":$path" > "$source_file"
    fi
    "$formatter" --style=file --assume-filename="$root/$path" < "$source_file" > "$formatted_file"
    if [[ "$mode" == "--apply" ]]; then
        if ! git diff --quiet -- "$path"; then echo "Unstaged edits: $path" >&2; exit 1; fi
        cp -- "$formatted_file" "$path"
    elif ! cmp -s "$source_file" "$formatted_file"; then
        echo "Formatting violation: $path" >&2
        exit 1
    fi
done
echo "Formatting PASS (${#selected[@]} files, $version, $mode)"
