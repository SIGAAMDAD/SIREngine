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

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "Build" / "CI"))
from CompareBenchmarks import median_samples

# Source-defined benchmark catalog; execution results remain ignored CI artifacts.
DEFAULT_MANIFEST = {
    "schema": "sir.scheduler.experiments.v1",
    "baseline_branch": "feat/grdg-task-scheduler-20261103",
    "description": "Controlled, synthetic scheduler measurements. Subsystem replay is not native renderer/physics/audio execution.",
    "experiments": [
        {
            "id": "E02-callback-contention",
            "filter": "^SIRBenchmarkExperimentContention/",
            "scope": "scheduler",
            "independent_variables": [
                "workers",
                "shared_atomic"
            ],
            "hypothesis": "Shared atomic result updates reduce scaling"
        },
        {
            "id": "G01-task-granularity",
            "filter": "^SIRBenchmarkExperimentGranularity/",
            "scope": "scheduler",
            "independent_variables": [
                "workers",
                "chunk_items",
                "compute_iterations"
            ],
            "hypothesis": "The optimum batch size depends on useful task cost"
        },
        {
            "id": "Q06-local-stealing",
            "filter": "^SIRBenchmarkExperimentLocalSteal/",
            "scope": "scheduler",
            "independent_variables": [
                "workers",
                "spawned_children"
            ],
            "hypothesis": "Worker-created tasks activate local queues and stealing"
        },
        {
            "id": "D06-dag-shapes",
            "filter": "^SIRBenchmarkExperimentDagShape/",
            "scope": "scheduler",
            "independent_variables": [
                "workers",
                "task_count",
                "graph_shape"
            ],
            "hypothesis": "Dependency width changes scheduling efficiency"
        },
        {
            "id": "D08-graph-compilation",
            "filter": "^SIRBenchmarkExperimentGraphCompile/",
            "scope": "grdg",
            "independent_variables": [
                "pass_count",
                "resource_access_mode"
            ],
            "hypothesis": "Overlapping writes increase graph compilation cost"
        },
        {
            "id": "SYS-synthetic-replay",
            "filter": "^SIRBenchmarkExperimentReplay/",
            "scope": "subsystems",
            "independent_variables": [
                "kernel_profile",
                "workers",
                "chunk_items"
            ],
            "hypothesis": "Different workload classes favor different task granularities and worker counts"
        },
        {
            "id": "S10-Q01-Q03-queue-candidates",
            "filter": "^SIRBenchmarkQueueCandidate/",
            "scope": "queue-alternatives",
            "independent_variables": [
                "queue_kind",
                "workers",
                "tasks_per_owner",
                "work_iterations",
                "workload_imbalance"
            ],
            "hypothesis": "Independent per-worker locks and bounded Chase-Lev remove global queue contention on owner-local tasks; compare matched workloads only"
        },
        {
            "id": "S8-existing-worker",
            "filter": "^SIRBenchmarkWorkerBatch/",
            "scope": "baseline",
            "independent_variables": [
                "workers",
                "tasks",
                "work"
            ],
            "hypothesis": "Existing shared-atomic benchmark is a mixed scheduler/payload measurement"
        },
        {
            "id": "S8-existing-providers",
            "filter": "^SIRBenchmarkFenceBurst/",
            "scope": "baseline",
            "independent_variables": [
                "ticket_count"
            ],
            "hypothesis": "Fence burst processing has a batching crossover"
        }
    ]
}


def sha256_file(path):
    checksum = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def validate_benchmark_json(path):
    """Reject skipped, malformed or empty benchmark experiments."""
    try:
        with open(path, "r", encoding="utf-8") as stream:
            data = json.load(stream)
        if not isinstance(data, dict) or not isinstance(data.get("benchmarks"), list):
            return False
        if any(not isinstance(row, dict) or row.get("error_occurred")
               for row in data["benchmarks"]):
            return False
        return bool(median_samples(data))
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError, TypeError):
        return False

def run(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=pathlib.Path)
    parser.add_argument("--manifest", type=pathlib.Path)
    parser.add_argument("--output-dir", required=True, type=pathlib.Path)
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args(argv)
    if not args.binary.is_file() or args.timeout <= 0:
        parser.error("benchmark executable must exist; timeout must be positive")
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8")) if args.manifest else DEFAULT_MANIFEST
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
            "manifest_sha256": hashlib.sha256(json.dumps(manifest, sort_keys=True).encode("utf-8")).hexdigest(),
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
                status = "pass" if proc.returncode == 0 and validate_benchmark_json(output) else "failed"
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
