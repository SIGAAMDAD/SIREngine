# Configuration parsing implementation packet

**Branch:** `feature/config-parsing` from `develop@0a7d47de77130b63290ec4950bdf2e257dbfe326`.

This directory contains the implementation plan and a **partially implemented** subsystem. Commit `ffbd58c2` provides shared result/context types, a tracked allocation budget, CSV/TSV buffer/stream reader and writer, and registered native tests; the other codecs remain planned. See [architecture](../../Architecture/19-configuration-parsing.md), [public API](API.md), [complete implementation order](IMPLEMENTATION.md), [test/benchmark acceptance](ACCEPTANCE.md), and [machine-readable task ledger](plan.json).

Supported target formats: Valve KV1/VDF, KV3 text/binary, Valve CFG, JSON, CSV, TSV, INI, XML, YAML 1.2.2, TOML 1.0/1.1. Memory requirements: explicit SIR allocator contexts, enforced per-document and per-parser scratch budgets, no hidden allocations, transactional document publication, deterministic error reporting and separate committed/retained/logical accounting.

Implementation stages: source and vendor audit; common contracts; native lightweight codecs; hierarchical codecs; binary KV3 and explicit cooked configuration; editor preservation and platform acceptance. Each stage and code ownership area is in IMPLEMENTATION.md.

**Verification status:** local Clang 17-mode `-Werror` ASan/UBSan and optimized build/test executions passed (`0 failures`) against minimal source-contract fixtures. Full repository CMake/Windows/Linux builds, pinned clang-format 22.1.5, conformance, fuzzing, memory-manager integration, benchmarks and editor integration remain **NOT_RUN**. Exact backend pinning and binary KV3 version coverage remain implementation decisions.
