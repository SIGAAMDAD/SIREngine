---
name: sir-profile
description: Investigate CPU, GPU, memory, allocation, cache, threading or I/O performance of SIREngine changes with measured comparisons.
argument-hint: '[system, benchmark and workload]'
context: fork
---
# Performance measurement

1. Record baseline and candidate source hashes, hardware, compiler, optimization level and workload.
2. Inspect the actual hot path and choose an instrumented metric; distinguish throughput, latency, allocations, bandwidth, memory footprint and GPU time.
3. Use representative warmups/repetitions and compare distributions, not one sample. Validate correctness while benchmarking.
4. Report raw measurements, computed deltas, confidence/variance, confounders and any missing counters.
5. Recommend changes only when supported by source and measurements; label untested ideas hypotheses.

Do not modify source during measurement.
