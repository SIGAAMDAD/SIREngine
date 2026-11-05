#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: RunGoogleBenchmarks.sh <output-label>" >&2
    exit 2
fi

label="$1"
if [[ ! "$label" =~ ^[A-Za-z0-9._-]+$ ]]; then
    echo "Invalid benchmark output label: $label" >&2
    exit 2
fi

benchmark_executable="$(find out -type f \( -name SIRCoreBenchmarks -o -name SIRCoreBenchmarks.exe \) \
    -path '*/profile/*' -print -quit)"
if [[ -z "$benchmark_executable" ]]; then
    echo "Profile build does not contain SIRCoreBenchmarks" >&2
    exit 1
fi

mkdir -p out/Nightly/Benchmarks
"$benchmark_executable" \
    --benchmark_min_time=0.1s \
    --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true \
    --benchmark_out="out/Nightly/Benchmarks/$label.json" \
    --benchmark_out_format=json

test -s "out/Nightly/Benchmarks/$label.json"
python3 Tests/Profiling/ValidateSIRGRDGBenchmarks.py "out/Nightly/Benchmarks/$label.json"
