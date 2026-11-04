# SIRConfig — complete implementation program

**Status:** IN PROGRESS — `ffbd58c2` implements parts of C01/C02/C05/C16/C17: shared types, a bounded callback-allocation adapter, buffer/stream CSV/TSV reader, CSV/TSV writer, SIRCore and CTest registrations. All other tasks remain planned.  
**Base:** SIGAAMDAD/SIREngine `develop@0a7d47de77130b63290ec4950bdf2e257dbfe326`.  
**Branch:** `feature/config-parsing`.  
**Scope:** Portable parsing, serialization, memory accounting, editor-preserving documents and explicit typed cooking for Valve KeyValues 1/VDF, Valve KV3 text/binary, Valve console CFG, JSON, CSV, TSV, INI, XML, YAML 1.2.2, TOML 1.0 and 1.1.

## A. Implementation boundaries

A portable `SIRCore` subsystem. No source dependency on an unfinished editor, GRDG, Godot parser or platform-specific filesystem. Keep first-party C++17 global namespace and repo-pinned formatting. Add CMake and unit/benchmark registration only when source actually exists. The new source path is `Source/Core/Config/`; the proposed headers and callable contracts are in [API.md](API.md) and the architecture is in [../../Architecture/19-configuration-parsing.md](../../Architecture/19-configuration-parsing.md). No backward adapters: there are currently no SIRConfig consumers.

The built-in config layer is reusable across SIREngine games. Budgets are passed by the caller or game profile. `SIR_MEMORY_DOMAIN_CORE`, `SIR_MEMORY_DOMAIN_EDITOR`, `SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT` and `SIR_MEMORY_DOMAIN_STREAMING` are existing possible backing domains, not hardcoded parser choices. No game-specific memory constants.

### Source ownership and cross-cutting inputs

| Boundary | Existing contract | Config implementation action |
| --- | --- | --- |
| Core CMake | `Source/Core/CMakeLists.txt` defines `SIRCore` and adds `Memory`, `Platform`, `Events` | Add `Config/CMakeLists.txt`; register implementation sources and include the directory |
| Allocator callback | `Source/Core/Containers/SIRContainerResult.h::sirContainerAllocator_t` | Use as backend; add per-parse/stream memory budget scope |
| Heap adapter | `Source/Core/Containers/SIRContainerHeapAdapter.h` | Preserve existing domain/allocator ownership; do not claim owner tags enforce hard caps |
| Memory authority | `Source/Core/Memory/SIRMemoryManager.h` | Attribute committed pages/peaks separately from requested bytes; use configured heap |
| Core containers | `SIRString`, vectors and hash maps | Use SIR allocator descriptors or owned fixed arrays, no default malloc fallback |
| Future editor/cooker | `Docs/Architecture/07-editor-cooking-runtime.md` | Expose format-preserving document and deterministic typed cook interfaces; integration when those consumers exist |
| Third-party | `ThirdParty/README.md`, `.gitmodules`, `IMPORT_LOCK.json`, `Docs/Dependencies.md`, `THIRD_PARTY_NOTICES.md` | Pin, license, audit, register and smoke-test exact backends; no runtime automatic fetch |
| Unit/performance | `Tests/Unit/CMakeLists.txt`, `Tests/Benchmarks/CMakeLists.txt` | Register per-format unit, format fixtures, fuzz harness and profiling |
| CI | `Docs/Architecture/10-build-and-dependencies.md` | CMake/FASTBuild, CTest, supported sanitizers, pinned SIR owned-source style checks |

**Vendor selection:** yyjson (JSON), rapidyaml (YAML), Expat (XML SAX) and toml11 (TOML) are research candidates, not accepted dependencies until full allocator closure and conformance pass. Native parsers for Valve, delimited text and INI. `libucl` is rejected as a universal parser: its formats/allocator design do not match the full requested contract. Before importing any dependency: inspect license (including transitive licenses); pin immutable source SHA; record in dependency lock/notice, and write a CMake toggle or unconditional target justified by actual need. No floating Git downloads at configure/build time.

## B. Frozen shared contracts before codecs

1. **Input:** a bounded immutable byte view or streaming `sirConfigSource_t` with read callback; invalid zero-progress success is an error. Sources are explicitly borrowed/pinned or copied; include resolver callbacks receive source identity and return bounded owned/leased content. Never resolve files or environment variables implicitly.
2. **Allocator:** `SIRConfigBudget` owns document/scratch allocation records with original backend descriptor, alignment and exact requested bytes; source and sink wrappers account for bytes and overflow. Reallocation counts old/new simultaneously unless in-place; all allocations (including vendor error paths) pass through the relevant scope. Missing allocator is `INVALID_ARGUMENT`. This is above the manager's page accounting and supports separate hard caps.
3. **Result/diagnostics:** `sirConfigResult_t`; format-specific diagnostic subcode; absolute byte offset, source ID, line/column, and stable error reason. Diagnostic formatting must not allocate outside the passed budget. `NOT_FOUND` is lookup-only, `END` is iteration-only, `OUT_OF_MEMORY` and `LIMIT_EXCEEDED` are distinct.
4. **Document:** move-only owner of nodes, interning/index storage and referenced input leases. Node handles = document ID + slot + generation. Destroying/moving/clearing invalidates handles as declared; no pointer identities across reload. Readers can safely concurrently inspect an immutable published document. Mutators require exclusive access.
5. **Transaction:** parse candidate in private storage, validate, then exchange with output document. Failure leaves old document unchanged. Mutations use a rollback-safe replacement plan or stage-and-swap. Reader `open()` failure returns fully closed state; stream writer errors poison further writes until `close()`.
6. **Output:** synchronous `sirConfigSink_t` callback accepts full byte span or returns error; writer tracks cumulative output bytes and propagates failure; any persistent-file transaction is supplied externally by platform code.
7. **Error and cancellation:** C++17 public surface is `noexcept`; exceptions (including vendor `bad_alloc`) are caught internally and translated, with all partial storage reclaimed. Callback return codes propagate without resuming the parse. No format access to engine console/script execution.

### Shared type/location tasks

- `Source/Core/Config/SIRConfigTypes.h`: fixed-width result enum; source byte-span; ownership and mode enums.
- `Source/Core/Config/SIRConfigContext.h`: allocator descriptors, `sirConfigLimits_t`, explicit validated policies.
- `Source/Core/Config/SIRConfigDiagnostics.h`: source ID/location/error propagation; fixed-size message or budget-owned diagnostic text.
- `Source/Core/Config/SIRConfigIO.h/.cpp`: checked read/write adapters; leased buffer, bounded stream, callback resolver contract.
- `Source/Core/Config/SIRConfigBudget.h/.cpp`: tracked allocation headers, overflow checks, alignment, free/realloc, failure counters, peaks; backend wrappers.
- `Source/Core/Config/SIRConfigNodeHandle.h/.cpp`: document identity+slot+generation and lookup validity.
- `Source/Core/Config/SIRConfigDocument.h`: shared lifetime/query semantics only; do **not** use a common lossy AST.
- `Source/Core/Config/SIRConfigCook.h/.cpp`: optional explicit schema-guided conversion into deterministic, bounded endian-defined bytes, independently compilable from editor/GRDG.

Static type definitions in API.md are the authority for cross-format signatures. When implementation reveals a contract mismatch, update and review all consumers/tests before publishing code. C++ headers need exact MPLv2 copyright/guard/PCH and repo format policy; no namespaces.

## C. Format-specific implementation specification

Each table row is a dependency-complete unit: parser, writer, public format representation, tests and benchmark. All format classes live in `Source/Core/Config/Formats/<format>/`; registration is under `Source/Core/Config/CMakeLists.txt`.

| Module / source paths | Implementation detail | Successful acceptance |
| --- | --- | --- |
| `Valve/SIRValveKV1.h/.cpp`, `SIRValveKV1Document.*` | Original KeyValues grammar, duplicate child keys, ordered tree, quotes/escapes, comments, explicit conditional evaluator and `#include`/`#base` callbacks; source ID + cycle detection; bound include bytes/depth | Differential Source Engine fixtures, repeated keys, base merge ordering, malformed brackets/strings, resolver denial |
| `Valve/SIRValveKV3.h/.cpp`, `SIRValveKV3Document.*`, `SIRValveKV3Binary.*` | Strict text header, typed scalars/arrays/objects, resource annotation, null/blob handling; binary codec registry keyed by proven header/version/encoding/compression; support multiple required versions only when golden fixtures and exact compressor dependencies are available | Round-trip typed text; binary reference fixtures for every *enabled* version/codec; reject unknown/unsupported versions; decompress budget |
| `Valve/SIRValveCfg.h/.cpp`, `SIRValveCfgProgram.*` | Tokenize ordered commands and argument arrays, quote/escape/semicolon/comment rules; no command execution or cvar evaluation | Compare source CommandBuffer tokenization; mixed quoting, escaped delimiters, empty commands; ensure no effect |
| `JSON/SIRJson.h/.cpp`, `SIRJsonDocument.*`, `SIRJsonYYJsonBackend.cpp` | yyjson explicit allocator and domain; RFC 8259 strict default, duplicate key rejection, U64/I64/float classification, Unicode escape and surrogate validation; RFC 6901 lookup; editor write | JSONTest corpus, UTF-8, integer boundaries, NUL strings, duplicate policy, deep input, fault injection; no hidden malloc |
| `Delimited/SIRDelimitedReader.h/.cpp`, `SIRDelimitedWriter.h/.cpp`, `SIRCsv.*`, `SIRTsv.*` | Shared incremental state machine with separator/quote/escape/newline policy; row views bounded to next advance; optional materialized table and header mode | Read/write CSV & TSV fixtures, CRLF and quotes across 1-byte chunks, column/row/field limits, oversized row, encoded NUL policy |
| `INI/SIRIni.h/.cpp`, `SIRIniDocument.*` | Explicit dialect profiles; ordered sections/entries, duplicate/repeated section policy, comments, whitespace and quoted values; raw values with typed getters | Strict and permissive profiles, case-fold rules, duplicated sections, root entries, comment losslessness/preservation |
| `XML/SIRXml.h/.cpp`, `SIRXmlEventReader.*`, `SIRXmlDocument.*`, `SIRXmlExpatBackend.cpp` | XML 1.0 and namespaces; incremental SAX feed, bounded entity policy, no external DTD by default; typed ordered mixed-content DOM built from events; special per-parser allocator routing | W3C tests, name/namespace/encoding, mixed content, CDATA, nested callbacks, concurrent parsing, entity exhaustion, early callback failure |
| `YAML/SIRYaml.h/.cpp`, `SIRYamlDocument.*`, `SIRYamlBackend.cpp` | YAML 1.2.2 docs/streams; arbitrary mapping keys, scalars and tags, anchors/alias graph (including cycles), schema resolution policy, preservation-mode writer | yaml-test-suite positive/negative cases, tag rules, nested mapping keys, aliases, cycle shutdown, expansion-limit enforcement |
| `TOML/SIRTOML.h/.cpp`, `SIRTomlDocument.*`, `SIRTomlBackend.cpp` | TOML 1.0 and 1.1 parser modes, exact data types, table vs table-array, dotted and quoted keys, offset/local datetime, invalid declaration rejection | Official TOML compliance tests for both versions, duplicate/invalid structure, number and datetime edge cases |

**Format compatibility:** names and versioned grammar modes reflect real external formats; these are not adapters for old SIR internals. Valve KV3 binary is a separate acceptance gate: supporting a version requires demonstrated reference fixtures and functioning decode+encode, not a claimed parser based on header recognition. For YAML, any missing standard-mandated syntax rejects the chosen backend until implemented; do not declare "complete" from a partial YAML subset.

## D. Dependency order — actionable stages

### Stage 0: freeze source, baseline and vendor audits

- Verify exact `develop` merge base and current tree; reconcile if upstream advances. Inventory Build, Containers, Memory, Platform, Tests, third-party locks and source format. Record SHA for each source reference.
- Create format conformance/negative fixtures and an allocator fault-injection harness before integrating vendors.
- Evaluate yyjson, rapidyaml, Expat, toml11 source revisions and licenses, total API coverage, memory allocation closure (including errors, writing, exception paths and nested callbacks), build time/size. If a candidate fails, write explicit backend replacement or fork plan; keep public contracts unchanged.
- Exit: candidate dependency register, pin choices, baseline tests and exact public API types; no unresolved default allocator paths.

### Stage 1: shared infrastructure

- Implement budget manager and callback bridge, byte/stream sources, sinks and error codes.
- Implement transactional documents, source lease, handle generations, diagnostic locations and cancellation.
- Register `SIRConfig` in `SIRCore` only after compilation; supply GoogleTest and benchmark fixtures.
- Exit: standalone CMake compile + complete budget/transaction/failure-injection/handle/lifetime/concurrency tests pass in Debug/Release.

### Stage 2: native lightweight codecs

- Implement CSV/TSV shared incremental reader/writer, INI editable document and writers, Valve CFG pure tokenizer and writer.
- Differential and data-fidelity fixtures; benchmark chunk sizes 1, 8, 64, 4096 and 64 KiB.
- Exit: every supported dialect round-trips, complete negative tests, zero untracked allocations.

### Stage 3: hierarchical format codecs

- Implement strict JSON backend + typed document; Valve KV1 & KV3 text; TOML 1.0/1.1; then XML streaming and YAML document/stream.
- Preserve format-specific node semantics; audit backend allocators and error paths.
- Exit: standard fixtures and fuzz tests for all text formats; no unbounded expansions; memory+concurrency acceptance.

### Stage 4: binary KV3 and explicit cooking

- Add versioned binary KV3 encode/decode with exact header/compression fixtures and bounded decompression.
- Implement opt-in typed/schema cooking with deterministic fingerprint, endian/offset/bounds validation, source dependency IDs and schema versions.
- Exit: every advertised binary version golden fixture passes, cooked-byte hashes deterministic, malformed cooked chunks rejected without publication.

### Stage 5: editor/source preservation and production verification

- Preserve comments, order, quote style and source spans where required; editor receives rich format-specific document objects, not a JSON projection. Add file transaction integration once platform file APIs exist.
- Integrate actual scene/import consumers when their APIs exist; integration tests must be NOT_RUN until then.
- Run full Windows/Linux CMake/FASTBuild + CTest, ASan/UBSan where supported, source formatting, Clang AST, fuzz corpus and repeated Google Benchmark profiles, plus packaging and failure injection.
- Exit: required platforms, formats, vendor memory audits and performance/regression gates pass with actual logs. No runtime acceptance based on static docs.

**Stages are dependency gates**, not extra remote branches. All work is confined to `feature/config-parsing` until explicit publication/merge instructions. Implement Stage 0 and Stage 1 before parallelizing format modules; all format workers consume the exact same API contract. Only the integration owner edits shared headers or CMake registration. Format-specific modules may be independent after the contracts compile.

## E. Per-task implementation checklist with ownership

| ID | Owning path(s) | Prerequisite | Output and done condition |
| --- | --- | --- | --- |
| C00 | `Docs/Plans/ConfigParsing/`, vendored-source audit | Baseline source SHA | source/license/conformance matrix, documented chosen revisions |
| C01 | `Source/Core/Config/SIRConfigTypes.h`, `Context.h`, `Diagnostics.h` | C00 | compilable public types and strict defaults with zero-alloc diagnostic |
| C02 | `Source/Core/Config/SIRConfigBudget.*` | C01 | per-context counters, no escapes, failure injection and wrong-allocator rejection |
| C03 | `Source/Core/Config/SIRConfigIO.*` | C01+C02 | lease, stream, sink, include resolver, output cap, cancellation |
| C04 | `Source/Core/Config/SIRConfigNodeHandle.*`, `SIRConfigDocument.h` | C01+C02 | handle identity/generation, transactional staging and move-only ownership |
| C05 | `Source/Core/Config/Formats/Delimited/*` | C02+C03 | CSV/TSV reader/writer, table, chunk coverage |
| C06 | `Source/Core/Config/Formats/INI/*` | C03+C04 | dialect-complete INI document and writer |
| C07 | `Source/Core/Config/Formats/Valve/SIRValveCfg*` | C03+C04 | command parser + writer, no execution side effects |
| C08 | `Source/Core/Config/Formats/JSON/*` | C00+C02+C04 | yyjson + SIR document, JSON conformance |
| C09 | `Source/Core/Config/Formats/Valve/SIRValveKV1*` | C03+C04 | native KV1 document, include/base/conditional tests |
| C10 | `Source/Core/Config/Formats/Valve/SIRValveKV3*Text*` and `*Document*` | C03+C04 | KV3 text typed data and writer |
| C11 | `Source/Core/Config/Formats/TOML/*` | C00+C04 | TOML 1.0/1.1 document and writer |
| C12 | `Source/Core/Config/Formats/XML/*` | C00+C02+C03+C04 | SAX+DOM, tested reentrant allocator routing |
| C13 | `Source/Core/Config/Formats/YAML/*` | C00+C04 | YAML graph, aliases, streams, writer, full test suite |
| C14 | `Source/Core/Config/Formats/Valve/SIRValveKV3Binary.*` | C10+C00 | every enabled KV3 binary version/compression backed by golden tests |
| C15 | `Source/Core/Config/SIRConfigCook.*` | C04+C08+C09+C10+C11+C12+C13 | explicit schema-typed deterministic cooking and validation |
| C16 | `Source/Core/Config/CMakeLists.txt`, `Source/Core/CMakeLists.txt`, `Build/CMake/SIRDependencies.cmake`, `ThirdParty/*`, docs/notice | C00+C01; progress per codec | actual library targets, pinned deps and CMake registration; first-party code no platform coupling |
| C17 | `Tests/Unit/Config/*`, `Tests/Benchmarks/Config/*`, relevant test CMake | C01 onward | runnable conformance/fuzz/fault/benchmark suite per codec |
| C18 | `Source/Editor/` and actual cooker/GRDG consumer paths when present | C15 + actual consumer APIs | source-preserving editing and cooked loading on real engine (must remain NOT_RUN before consumers exist) |
| C19 | CI registration and release acceptance docs | C05–C18 | source/ABI/format/memory/performance acceptance with actual logs and comparison baseline |

For each task, stop on an allocator fallback, undecoded required grammar feature, unexpected cross-domain ownership, source provenance failure, or missing dependency; record the precise blocker. Avoid touching another subsystem's owned implementation except explicit CMake test/source registration. An implementation that compiles but skips mandatory negative tests is **PARTIAL**, not complete.

## F. Error handling and recovery contract

- **Invalid input/encoding:** produce stable byte offset and source ID; preserve old output document; do not continue parsing after a fatal error.
- **Allocator failure/budget limit:** reject before backend allocation, free any candidate state, produce OOM or LIMIT_EXCEEDED separately, leave previous document and allocator counters consistent.
- **Stream cancellation/source error:** stop immediately, do not retain source buffer pointers, close/release stream state. Reader EOF is `SIR_CONFIG_END` only at a legal grammar boundary.
- **Sink output failure:** immediately poison writer and propagate IO error; a partial external destination is allowed and explicitly reported; platform atomic-file wrapper handles rollback.
- **Include/alias/entity cycles:** track graph or visit state with bounded depth/count; YAML alias cycles may remain in an owned graph, whereas recursive expansion must be bounded. Multiple source documents contribute to cumulative budget.
- **Unknown KV3 binary version/codec:** fail explicitly with `SIR_CONFIG_UNSUPPORTED_VERSION`; no speculative decode.
- **Thread contention:** independent instances are safe; a single mutable document is not shared without caller synchronization. Expat TLS allocation-stack handling must survive nested callbacks and parser destruction.

## G. Build, test and publication commands

```sh
git switch feature/config-parsing
git fetch origin develop
git merge-base --is-ancestor 0a7d47de77130b63290ec4950bdf2e257dbfe326 HEAD
git submodule update --init ThirdParty/GoogleTest ThirdParty/GoogleBenchmark
# Once vendor choices have been pinned, initialize their actual paths as well.
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

The current SIREngine CI/style gates and pinned formatter are mandatory for new C++; match actual CMake preset availability on each OS. Use documented `asan` and `ubsan` presets where supported. Vendor linking, SIR API compile, unit/conformance, sanitizer, memory attribution and benchmark gates require actual logs bound to one exact source SHA. Do not infer passing tests from proposed commands. No source/API implementation is included in this planning commit.

## H. Acceptance and dispatch

All mandatory formats, writer paths, memory behavior and tests are defined in [ACCEPTANCE.md](ACCEPTANCE.md). The plan is **READY FOR OWNER TECHNICAL REVIEW**, with implementation **NOT_RUN**. Before claiming full implementation, verify the actual branch source matches the documented contracts, the chosen third-party revisions/allocators pass their negative tests, and every required format/version has conformance evidence. Dependencies and editor consumer integration may be temporarily deferred only with explicit PARTIAL status, never labeled complete.
