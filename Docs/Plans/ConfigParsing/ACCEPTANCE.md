# SIRConfig — conformance, memory and performance acceptance

**Status:** PARTIAL — local Clang ASan/UBSan and optimized test binaries for the initial CSV/TSV and budget slice passed (`0 failures`). Full SIREngine CMake, upstream conformance suites, and Windows/Linux CI are NOT_RUN. Source baseline `develop@0a7d47de77130b63290ec4950bdf2e257dbfe326`. This matrix sets actual completion criteria and negative cases.

## I. Universal gates for every format

| Gate | Positive proof | Negative/hostile proof |
| --- | --- | --- |
| Input | Accept valid byte spans and legal stream EOF | Reject null pointer with positive size, invalid zero-progress success and truncated chunks |
| Syntax | Accept official valid grammar fixtures | Reject official invalid syntax with source ID and byte offset |
| Encoding | Correct UTF-8, escapes and embedded NUL when legal | Invalid sequences, truncated surrogate/escape and illegal codepoints |
| Ownership | Document remains valid after copied input released; borrowed view valid while lease held | Destroy source early, move/clear document, stale/cross-document handle rejected |
| Allocation | Reclaim all document/scratch allocations on success and destruction | Fail every Nth allocation and every limit; no leak, old document unchanged |
| Budget | Exact requested/scratch peak and manager commit accounting | Depth/node/row/column/inclusion/entity/alias amplification bounded, integer overflow rejected |
| Output | Bounded writer produces parsable equivalent bytes | Sink error and zero-cap writer; error surfaced and further writes rejected |
| Round trip | Parse->write->parse semantic equivalence | Unsupported preservation requested produces explicit error instead of dropping metadata |
| Concurrency | Two independent instances on separate threads | Shared document mutation without sync forbidden; nested callbacks do not cross domains |
| Compilation | Windows and Linux, C++17, SIR namespace/type/header style | Missing pinned dependency/formatter or unregistered implementation => fail |

Budget is **not** verified merely by observing `nMaxNodes`; instrument backing-allocator requests and SIRMemoryManager page commitment. Measure peak bytes, resident/committed bytes, retained heap capacity, owner tags, failed allocations, latency and throughput as separate metrics.

Common harness paths to implement:
- `Tests/Unit/Config/SIRConfigCoreTests.cpp`
- `Tests/Unit/Config/SIRConfigBudgetTests.cpp`
- `Tests/Unit/Config/SIRConfigIOTests.cpp`
- `Tests/Unit/Config/SIRConfigDocumentTests.cpp`
- `Tests/Unit/Config/<Format>Tests.cpp`
- `Tests/Unit/Config/Fixtures/<Format>/` with provenance/version metadata for imported corpora
- `Tests/Benchmarks/Config/SIRConfigBenchmarks.cpp`
- `Tests/Fuzz/Config/<Format>Fuzz.cpp` and persistent corpus seeds for CI/nightly
- CTest labels `config.core`, `config.format`, `config.memory`, `config.fuzz`, `config.sanitizer`.

## II. Required per-format suites

| Format | Conformance and positive tests | Explicit negative and lifecycle tests |
| --- | --- | --- |
| Valve KV1 / VDF | Source Engine `KeyValues.cpp` behavior fixtures, conditional/based/included files, ordered duplicate keys, source spans | Include cycles, resolver errors, escape damage, recursion/size caps, duplicate policy and base precedence |
| Valve KV3 text | Strict header, typed arrays/objects/resource/string/blob, writer output and header GUID handling | Invalid GUID/type/annotation, truncated literals, numeric overflow, malformed references |
| Valve KV3 binary | Golden files for each advertised binary version, codec, compression; encode-decode equivalence | Unknown header/version, invalid offsets, truncation, bomb decompression, unsupported compression rejection |
| Valve CFG | Source `CommandBuffer` fixtures for semicolons/comments/quotes, argv ordering and writing | Unterminated quotes, oversized command/arg, double separators, and prove parse never invokes console callbacks |
| JSON | JSONTest corpus, RFC 8259, RFC 6901, Unicode, numeric I64/U64/float ranges, writer | Duplicate keys default rejection, invalid UTF-8, NaN/infinity, overflow and unpaired surrogate |
| CSV / TSV | RFC 4180-style CSV with dialect options; quoted multiline fields; embedded delimiters; TSV table; chunk sizes 1,2,7,64,4096; header options | EOF in quote, CRLF split over chunks, column/row limit, enormous row, invalid byte policy, malformed quoting |
| INI | Explicit strict/permissive profiles; empty/global sections, repeated sections, ordering, comments, quoted/raw values | Duplicate key/profile conflict, malformed section header, leading whitespace ambiguity, invalid case fold |
| XML | W3C XML 1.0 namespace tests; SAX events equal DOM content; CDATA/text/PI/comment; roundtrip | External entities default disabled, exponential expansion, duplicate attribute, malformed entity, parser callback abort |
| XML allocator | Same-thread nested parser + concurrent threads; create/parse/error/free in correct domain | Allocator context mismatch, callback recursion, OOM in nested parser, destruction on wrong scope |
| YAML | yaml-test-suite valid/invalid cases; YAML 1.2.2 documents, scalar tags, complex keys, anchored/alias graphs, multi-doc streams | Custom tag constructor execution, aliases over limits, invalid merges under selected schema, cyclic graph destruction |
| TOML | TOML official tests, 1.0 and 1.1 grammar modes, exact date/time, dotted keys, arrays of tables, quoted keys | Duplicate tables/keys, invalid date/time/leap rules, overflow, redefinition of inline tables |
| Cooked config | Exact typed schema -> deterministic hash/bytes; version+endianness+offsets+bounds | Corrupted offset, wrong schema version, hash mismatch, partial payload, commit failure |
| Editor preserve | Parse/edit/write keeps supported comments/trivia, order, whitespace/quote policy and source location | Unsupported lossless edit returns explicit error, stale editor node handle rejected |

For each imported upstream corpus: record license, origin URL, exact source commit/tag and documented expected outcomes. Do not cherry-pick only easy fixtures. Any unsupported required syntax fails the "complete format" gate.

## III. Allocator adversarial matrix

1. Set document budget to the smallest successful value, then reduce by one byte: parse must return LIMIT_EXCEEDED, leave previous document intact and not allocate over cap.
2. Set scratch budget just below required token growth: failure must leave no leaked parser state.
3. Loop fault injection at allocation index `N = 1..allocation_count` for each representative corpus; verify no leaks, double frees or stale publication at every index.
4. Use distinct document/scratch memory domains with unrelated owner tags, then parse nested XML callbacks and YAML aliases; verify exact ownership accounting.
5. Reallocate a string with both old and new live; check peak honors transient dual-storage and correct size/alignment on free.
6. Force denied domain page commit on the real `SIRMemoryManager`; no direct vendor malloc fallback and no false success.
7. Parse deep tiny nodes whose metadata greatly exceeds input bytes; node/byte/depth limits must all bind independently.
8. Validate output sinks fail at every byte boundary; old document unchanged and partial output is explicitly caller-owned.
9. Repeated create/parse/clear/destroy 10000 times under leak tracing; committed vs retained pages show no unexplained growth.
10. Borrowed mapped input is intentionally kept alive by an input lease until the document releases it; after release no view remains reachable.

Memory closure requires vendor source review of every allocation/free/realloc function, including read, parse, diagnostics, schema/validation, write, nested callbacks and failure paths. A source grep alone does not certify closure; dynamic fault injection and instrumentation must corroborate it.

## IV. Performance suite and acceptance

Provide representative document sizes measured at runtime, e.g. 256 bytes, 4 KiB, 64 KiB, 1 MiB and a game-authored large config; also format-appropriate workload shapes (deep, wide, string-heavy, many tiny entries, high duplicate rate, streaming, highly fragmented).

Report each benchmark with:
- Exact SIREngine source SHA, backend vendor SHA, compiler, platform, allocator settings and worker core;
- Mean/median plus p95 and p99 for parse/write/query/iteration where relevant;
- Parsed input MiB/s and wall/CPU times; cold and warm caches separated;
- Number/size of allocations; peak logical document/scratch bytes; SIR committed pages; retained free capacity;
- Peak performance of repeated hot parses, document mutation, streamer chunk sizes, and writer sink callbacks;
- Compiler baseline versus previous accepted revision and alternative vendor where measurable;
- A profile of large real-world config loads, not only microbench synthetic strings.

**Performance gates:** no new hidden allocations and no correctness regressions; compare current baseline medians and allocation peaks against the previous successful build. Do not claim specific speedups without real measurements. Proposed alert thresholds (subject to project profiling policy): >5% sustained median CPU regression or >10% peak-commit increase triggers investigation and explicit evidence-backed disposition; CI runner noise alone must not fail a release without reproduction.

## V. Build, tool and integration evidence

Minimum acceptance commands **after implementation** (run according to host availability):

```sh
cmake --preset tooling -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset tooling
ctest --preset tooling --output-on-failure
cmake --preset dev -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --preset profile -DSIR_WITH_GOOGLETEST=ON -DSIR_WITH_GOOGLE_BENCHMARK=ON -DSIR_BUILD_BENCHMARKS=ON
cmake --build --preset profile
ctest --preset profile --output-on-failure
bash Build/Scripts/RunGoogleBenchmarks.sh local-profile
```

Windows and Linux Debug/Release and available Clang/ASan/UBSan combinations need actual CMake/CTest logs. New C++ must pass pinned clang-format, headers, fixed-width enum/struct, global-namespace and AST checks. Add config fixture/fuzzer runs in regular and nightly CI. A test binary not linked from the real `SIRCore` sources is not integrated acceptance. Final source commit plus vendor pins must be included in evidence logs.

**Acceptance verdicts as of this plan:** all CONFIG UNIT, CONFORMANCE, FUZZ, MEMORY, PERFORMANCE, WINDOWS, LINUX, EDITOR and COOKED LOAD gates are **NOT_RUN**. `ffbd58c2` delivers a parser source implementation and tests. Full SIREngine integration and format-completeness acceptance remain NOT_RUN.
