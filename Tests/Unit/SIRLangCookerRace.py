#!/usr/bin/env python3
"""Exercise two independent concurrent SIRLang cooker publishers."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import threading


def run_pair(command):
    ready = threading.Barrier(3)

    def invoke():
        ready.wait(timeout=10)
        return subprocess.run(command, capture_output=True, text=True, timeout=30)

    with ThreadPoolExecutor(max_workers=2) as workers:
        futures = [workers.submit(invoke) for _ in range(2)]
        ready.wait(timeout=10)
        return [future.result(timeout=40) for future in futures]


def unique_publisher(codes):
    return len(codes) == 2 and codes.count(0) == 1 and any(code > 0 for code in codes)


def main(argv):
    if len(argv) != 4:
        print("Expected cooker, input and output paths", file=sys.stderr)
        return 2
    command = [argv[1], argv[2], argv[3], "101", "201", "301", "401", "1", "1"]
    try:
        results = run_pair(command)
    except (OSError, subprocess.TimeoutExpired, threading.BrokenBarrierError, TimeoutError) as exc:
        print(f"Concurrent cooker execution failed: {exc}", file=sys.stderr)
        return 1
    codes = [result.returncode for result in results]
    if not unique_publisher(codes):
        print(f"Expected one successful publisher and one controlled rejection: {codes}", file=sys.stderr)
        for index, result in enumerate(results):
            print(f"Cooker {index} stderr: {result.stderr[-2000:]}", file=sys.stderr)
        return 1
    if not Path(argv[3]).is_file():
        print("Winning cooker did not produce a file", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
