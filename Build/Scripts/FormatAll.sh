#!/usr/bin/env bash
# Format every tracked, engine-owned C/C++ file before a source commit.
set -euo pipefail

if [[ $# -gt 1 || ( $# -eq 1 && "$1" != "--pre-commit" && "$1" != "--check" ) ]]; then
    printf 'Usage: %s [--pre-commit|--check]\n' "$0" >&2
    exit 2
fi

mode="${1:---apply}"
root="$(git rev-parse --show-toplevel)" || {
    printf 'FormatAll: not inside a Git worktree\n' >&2
    exit 1
}
cd "$root"

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

files=()
while IFS= read -r -d '' path; do
    case "$path" in
        *.cpp|*.cc|*.c|*.h|*.hpp)
            if [[ ! -f "$path" ]]; then
                printf 'FormatAll: tracked source missing: %s\n' "$path" >&2
                exit 1
            fi
            files+=( "$path" )
            ;;
    esac
done < <(git ls-files -z -- Source Tests Tools)

if (( ${#files[@]} == 0 )); then
    printf 'FormatAll: no tracked engine C/C++ source found\n' >&2
    exit 1
fi

# Bound argument size for Git Bash on Windows. Apply, then independently
# confirm the entire tree is formatted before Git may create a commit.
for (( first=0; first<${#files[@]}; first+=32 )); do
    batch=( "${files[@]:first:32}" )
    if [[ "$mode" == "--check" ]]; then
        "$formatter" --dry-run --Werror --style=file "${batch[@]}"
    else
        "$formatter" -i --style=file "${batch[@]}"
    fi
done

# Reuse the repository's canonical full-tree check, including its own
# pinned-version verification. Do not restrict this to changed files.
cmake -P Build/CMake/CIFormat.cmake

if [[ "$mode" == "--pre-commit" ]]; then
    # The formatted working copy must match the staged source exactly;
    # re-stage and rerun the hook after any formatter modification.
    while IFS= read -r -d '' path; do
        case "$path" in
            *.cpp|*.cc|*.c|*.h|*.hpp)
                if ! git diff --quiet -- "$path"; then
                    printf 'FormatAll: staged %s differs from formatted working copy; git add and commit again\n' "$path" >&2
                    exit 1
                fi
                ;;
        esac
    done < <(git diff --cached --name-only -z --diff-filter=ACMR -- Source Tests Tools)
fi
printf 'FormatAll: PASS (%s engine C/C++ files, %s)\n' "${#files[@]}" "$version"
