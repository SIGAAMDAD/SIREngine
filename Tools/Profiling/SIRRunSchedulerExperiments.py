#!/usr/bin/env python3
"""Execute manifest-defined scheduler experiments with exact source/host metadata."""
import argparse
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import sys


def sha256_file(path):
    checksum = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def run(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=pathlib.Path)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--output-dir", required=True, type=pathlib.Path)
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args(argv)
    if not args.binary.is_file() or args.timeout <= 0:
        parser.error("benchmark executable must exist; timeout must be positive")
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        experiments = manifest["experiments"]
        if not isinstance(experiments, list) or not experiments:
            raise ValueError("experiments must be a nonempty array")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        try:
            commit = subprocess.check_output(["git", "rev-parse", "HEAD"], timeout=5, text=True).strip()
        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            commit = "unavailable"
        metadata = {
            "commit": commit,
            "executable": str(args.binary.resolve()),
            "executable_sha256": sha256_file(args.binary),
            "host": platform.node(),
            "system": platform.platform(),
            "processor": platform.processor(),
            "logical_cpus": os.cpu_count(),
            "manifest_sha256": sha256_file(args.manifest),
        }
        seen = set()
        records = []
        failed = False
        for exp in experiments:
            ident = exp["id"]
            if not isinstance(ident, str) or not ident.replace("-", "").replace("_", "").isalnum() or ident in seen:
                raise ValueError(f"invalid or duplicate experiment id: {ident}")
            seen.add(ident)
            if args.only and ident not in args.only:
                continue
            filter_text = exp["filter"]
            if not isinstance(filter_text, str) or not filter_text:
                raise ValueError(f"invalid benchmark filter for {ident}")
            output = args.output_dir / f"{ident}.json"
            command = [str(args.binary.resolve()), f"--benchmark_filter={filter_text}", "--benchmark_min_time=0.2s",
                       "--benchmark_repetitions=7", "--benchmark_report_aggregates_only=true",
                       f"--benchmark_out={output}", "--benchmark_out_format=json"]
            try:
                proc = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout, check=False)
                status = "pass" if proc.returncode == 0 and output.is_file() and output.stat().st_size > 0 else "failed"
                records.append({"id": ident, "status": status, "exit_code": proc.returncode, "command": command,
                                "result": output.name if status == "pass" else None,
                                "stdout_tail": proc.stdout[-4096:], "stderr_tail": proc.stderr[-4096:]})
            except subprocess.TimeoutExpired:
                status = "timeout"
                records.append({"id": ident, "status": status, "command": command})
            if status != "pass":
                failed = True
        if args.only and len(records) != len(set(args.only)):
            raise ValueError("unknown experiment ID in --only")
        metadata["results"] = records
        (args.output_dir / "manifest-results.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n",
                                                                 encoding="utf-8")
        return 1 if failed else 0
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    return 2


if __name__ == "__main__":
    sys.exit(run())
