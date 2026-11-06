#!/usr/bin/env bash
set -uo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: NightlyBuild.sh <CMake preset> [extra CMake configuration arguments]" >&2
    exit 2
fi
preset="$1"
shift
label="${SIR_CI_LABEL:-${RUNNER_OS:-Local}-$preset}"
if [[ ! "$label" =~ ^[A-Za-z0-9._-]+$ ]]; then
    echo "Invalid nightly label: $label" >&2
    exit 2
fi
# The workflow invokes this script from the repository root. On Git Bash,
# GITHUB_WORKSPACE is a Windows path; PWD is a usable POSIX path.
root="$(pwd -P)"
report="$root/out/Nightly"
mkdir -p "$report/logs" "$report/tests" "$report/metrics"

run_logged() {
    local path="$1"
    shift
    "$@" 2>&1 | tee "$path"
    return "${PIPESTATUS[0]}"
}
timed_step() {
    local name="$1"
    shift
    local started ended status
    started="$(date +%s)"
    if run_logged "$report/logs/$label-$name.log" "$@"; then
        status=0
    else
        status=$?
    fi
    ended="$(date +%s)"
    printf -v "${name}_exit" '%d' "$status"
    printf -v "${name}_seconds" '%d' "$((ended - started))"
}

config_exit=-1
build_exit=-1
test_exit=-1
package_exit=-1
config_seconds=0
build_seconds=0
test_seconds=0
package_seconds=0

timed_step config cmake --preset "$preset" "$@"
if (( config_exit == 0 )); then
    if [[ "$preset" == "tooling" ]]; then
        timed_step build cmake --build --preset "$preset" --parallel 4
    else
        timed_step build cmake --build --preset "$preset"
    fi
fi
if (( build_exit == 0 )); then
    python_cmd=python3
    if [[ "${RUNNER_OS:-}" == Windows ]]; then python_cmd=python; fi
    timed_step test "$python_cmd" Build/CI/RunCTestGate.py --preset "$preset" \
        --report "$report/tests/$label.xml"
fi
if [[ "$preset" == "release" && "${SIR_NIGHTLY_PACKAGE:-OFF}" == "ON" ]] && (( test_exit == 0 )); then
    timed_step package cmake --build --preset release --target package
fi

if ! cmake -DSIR_REPO_ROOT="$root" -DSIR_REPORT_ROOT="$report" \
    -DSIR_LABEL="$label" -DSIR_PRESET="$preset" \
    -DSIR_CONFIG_EXIT="$config_exit" -DSIR_BUILD_EXIT="$build_exit" \
    -DSIR_TEST_EXIT="$test_exit" -DSIR_PACKAGE_EXIT="$package_exit" \
    -DSIR_CONFIG_SECONDS="$config_seconds" -DSIR_BUILD_SECONDS="$build_seconds" \
    -DSIR_TEST_SECONDS="$test_seconds" -DSIR_PACKAGE_SECONDS="$package_seconds" \
    -DSIR_PACKAGE_REQUIRED="${SIR_NIGHTLY_PACKAGE:-OFF}" \
    -P "$root/Build/CMake/NightlyMetrics.cmake"; then
    echo "Nightly metrics generation failed" >&2
    exit 1
fi

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    {
        echo "### $label"
        echo
        echo "| Stage | Exit code | Seconds |"
        echo "| --- | ---: | ---: |"
        echo "| Configure | $config_exit | $config_seconds |"
        echo "| Compile/link | $build_exit | $build_seconds |"
        echo "| CTest | $test_exit | $test_seconds |"
        echo "| Package | $package_exit | $package_seconds |"
    } >> "$GITHUB_STEP_SUMMARY"
fi

if (( config_exit != 0 || build_exit != 0 || test_exit != 0 )); then
    exit 1
fi
if [[ "$preset" == "release" && "${SIR_NIGHTLY_PACKAGE:-OFF}" == "ON" ]] && (( package_exit != 0 )); then
    exit 1
fi
