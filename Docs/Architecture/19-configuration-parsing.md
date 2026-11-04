# 19 — Configuration parsing, serialization, and memory ownership

**Status:** PARTIAL IMPLEMENTATION — shared configuration types, the allocation budget adapter and CSV/TSV streaming reader/writer are implemented on `feature/config-parsing` at `ffbd58c2`; remaining format codecs are planned.  
**Source baseline:** `develop` commit `0a7d47de77130b63290ec4950bdf2e257dbfe326` (2026-11-03).  
**Owned area:** `Source/Core/Config/` (future); test integration under `Tests/Unit/Config/` and `Tests/Benchmarks/Config/`.

## Requirement and source ledger

Owner requirements (conversation 2026-11-03): implement Valve configuration files, JSON, CSV, TSV, INI, XML, YAML and TOML; provide an API for each; preserve precise memory usage controls; evaluate libucl as a candidate, not an imposed dependency; document a complete implementation plan and work on a separate branch.

Source facts at baseline:
- `Source/Core/CMakeLists.txt` defines `SIRCore` and adds `Platform`, `Memory`, `Events`; no `Config` directory or CMake target exists.
- `Source/Core/Containers/SIRContainerResult.h` defines `sirContainerAllocator_t` with `pContext`, `pfnAllocate`, `pfnDeallocate`, `nDomainId` and `nOwnerTag`.
- `Source/Core/Containers/SIRContainerHeapAdapter.h` binds that interface to `SIRGeneralHeap`, itself scoped to one `sirMemoryDomain_t`. The owner tag is metadata, not a current per-owner hard cap.
- `Source/Core/Memory/SIRMemoryManager.h` exposes per-domain commit limits, commit peaks and tracing; requested-byte accounting needs an additional budget wrapper.
- `Source/Core/Containers/SIRString.h` exposes `sirStringView_t` with a 32-bit byte count; the configuration source API therefore uses 64-bit byte lengths and does not coerce a whole file to that type.
- `Source/Core/Platform/` has no registered config-specific file source API. The configuration core is therefore memory-buffer/source-callback/sink-callback based, independently of future OS file integration.
- `Docs/Architecture/07-editor-cooking-runtime.md` preserves editable source data and deterministic cooked runtime records.
- `Docs/Architecture/10-build-and-dependencies.md` specifies CMake/FASTBuild, GoogleTest/CTest, Google Benchmark and Windows/Linux targets.
- `ThirdParty/README.md`, `.gitmodules` and `ThirdParty/IMPORT_LOCK.json` do not include libucl, yyjson, Expat, rapidyaml or toml11 at the source baseline.
- Local user-supplied reference archive `source-engine-master.zip` SHA-256 `05edc3526174dde936470c3a7f2810ef8f0e26b1a7277c35094943fab7725dca`; relevant files: `tier1/KeyValues.cpp`, `public/tier1/KeyValues.h`, `tier1/commandbuffer.cpp`, `public/tier1/CommandBuffer.h`, `datamodel/dmserializerkeyvalues2.cpp`. Reference material only; no direct Source code copy.

## Architectural decision

One shared configuration **service contract**, separate **format-specific public APIs and node representations**. The service shares input, output, limits, allocators, diagnostics, node-handle safety and source metadata. It does **not** coerce all formats into JSON objects: XML has mixed ordered content/namespaces, YAML has tagged mapping-key graphs and aliases, CSV is tabular, Valve CFG is an ordered command program, and TOML has distinct date/time and table rules.

No compatibility adapters are needed because this is a new subsystem and has no consumers in `develop`. No editor, scripting runtime, GRDG or full engine bootstrap is a dependency for the portable parser unit tests.

### Non-negotiable invariants

1. All parser-managed dynamic bytes — vendor internals included — flow through the caller-supplied **document** or **scratch** allocator, both backed by SIR-managed memory. Null allocator is an error; never fall back to `malloc`, `new`, std default allocation, Godot `memalloc` or vendor defaults.
2. Track **live requested**, **peak requested**, **retained backing capacity**, **OS committed** and **borrowed** bytes separately. Manager commitment is not logical document size; attribution uses `sirMemoryDomain_t` and owner tags plus per-parse budget scope.
3. Limits are finite, explicit and measured: input, cumulative include and decoded bytes, document live bytes, scratch live bytes, output bytes, nodes, depth, scalar/row/column sizes, include/alias/entity expansions, and total callback invocations as applicable. `0` for a mandatory limit is invalid rather than unlimited.
4. Parse-and-replace is transactional: build a private candidate; on any error release candidate allocations, leave the previous output document unchanged, report a diagnostic, and publish the candidate only after successful validation.
5. Allocation failures never corrupt an already published document. Mutating editor operations either succeed or leave the previous representation unchanged; stale/cross-document handles are rejected.
6. Input ownership is explicit: copied/owned versus borrowed-and-pinned until document destruction; streaming readers expose views valid until the next advance. No dangling views after reload.
7. Distinguish structural syntax from semantics: preserve each format's duplicates, ordering, numeric precision, comments and extra metadata where that format and preservation mode support them. No silent lossy normalization across formats.
8. Parsing is data-only and has no command execution, filesystem access, implicit include resolution, environment expansion or network effects. Such operations are optional, strictly caller-provided callbacks with depth/byte limits.
9. Independent parser instances are reentrant across threads. Shared mutable globals in vendor parsers are disallowed; a scoped thread-local adapter is permissible only with exhaustive nested-call and callback reentrancy tests.
10. Writers enforce output budgets and propagate sink failures; callers requesting atomic persistent-file replacement must use a platform file layer that stages, flushes and atomically replaces the destination.
11. Externally sourced parser revisions are pinned Git submodules or reviewed source imports, with licenses and revisions in the existing dependency/notice registers. No unpinned build-time downloads.
12. All SIREngine-owned C++ follows current global-namespace SIR style, MPLv2 header/guards, fixed-width `sirName_t` typedefs, `SIR_` enum members, C++17, and the exact repository formatter.

## Components and data flow

```text
byte view / bounded callback source
  -> SIRConfigInput + format options + SIRConfigParseContext
  -> per-instance budget wrapper / parser scratch
  -> concrete parser backend (Valve/JSON/Delimited/INI/XML/YAML/TOML)
  -> format-specific staged document or streaming event/row
  -> validation and transactional publish
  -> editor access / explicit typed conversion / deterministic cook
  -> format-specific writer -> bounded sink or platform file adapter
```

**Ownership:** `SIRConfigBudget` owns allocation records and cap checks; a document owns nodes and retained storage; each streaming reader owns only its state and scratch; the caller owns borrowed inputs and callback source/sink state; an optional source resolver owns retrieved bytes until the parser copies them or holds a lease. The document and reader must be destroyed before their allocator descriptors, heap and manager. Each format's code owns its interpretation of its syntax and metadata.

**Error flow:** invalid arguments/encoding/limits/unsupported version/ambiguous type/unknown include/OOM/IO/cancellation return `sirConfigResult_t`, with byte offset and source ID where known. `SIR_CONFIG_END` is reserved for exhausted readers/iterators. `SIR_CONFIG_NOT_FOUND` is reserved for missing keys. Partial writes can have external effects; never claim sink rollback. No thrown exception escapes the public C++ API.

### Memory contract

- Use existing `sirContainerAllocator_t` interfaces as backing allocators, but do not treat them as per-document budgets. `SIRConfigBudget` wraps and accounts for every allocate/reallocate/free with checked overflow, alignment and reason-specific denial.
- A document budget and a scratch budget are separate, both explicitly sized; their **combined peak** is reported. Third-party callbacks that lack `free(size)` track sizes in SIR-owned allocation headers and free through the exact originating allocator.
- A borrowed input is counted as borrowed, never counted as SIR committed memory. Staging/unescaping/cross-chunk fields are charged to scratch. Strings retained by published documents are charged to document.
- A zero-copy parsed string can reference pinned input only if the parser stores an owned lease; otherwise materialize bytes in document storage. Mutating or releasing a borrowed source while the lease exists is invalid.
- A scratch epoch can be reset only after parser operations and synchronous callbacks no longer reference it. Reader state that persists across calls cannot live in a reset frame scratch arena.
- Peak requested bytes are measured by the budget wrapper; peak committed bytes by `SIRMemoryManager`. Validation measures both and denies a missing accounting path.
- Parser-specific `realloc` must account for old and new simultaneously where the vendor's operation cannot resize in place; limits must be applied *before* any backing allocation.
- A max depth protects bounded stack usage in recursive vendor functions. Testing includes depth 0, depth 1, maximal supported depth, and depth+1.

## Format fidelity matrix

| Public subsystem | Distinct behaviors | Backend strategy | Acceptance |
| --- | --- | --- | --- |
| `SIRValveKV1` | Ordered objects, repeated keys, conditionals, escaped strings, `#include`/`#base` | Native, source-informed tokenizer and tree | Source Engine-derived fixtures; include-cycle and precedence cases |
| `SIRValveKV3` | Typed values; text header GUIDs; resource strings; blobs; binary version/compression codecs | Native text parser; explicit versioned binary codecs; existing LZ4/Zstd only when enabled | Text and known binary-version fixtures; unknown versions rejected |
| `SIRValveCfg` | Command/argv sequence, quotes, separators and comments | Native tokenizer, **not** engine command execution | Source command buffer differential fixtures |
| `SIRJson` | RFC 8259 UTF-8, exact integer classification, duplicate policy, RFC 6901 pointers | yyjson with non-null SIR allocator and object wrapper | JSONTest/standard fixtures; pointers; allocation faults |
| `SIRCsv`/`SIRTsv` | Shared delimited row engine, quoting, line breaks, header choices | Native incremental state machine | Cross-chunk records; delimiter/quote/CRLF permutations |
| `SIRIni` | Nonstandard grammar selectable by strict profile: section/key duplicates, comments, whitespace | Native line-oriented scanner | Explicit dialect fixtures and round-trip |
| `SIRXml` | XML 1.0 namespaces, ordered mixed content, entities, SAX events and DOM | Expat SAX adapter after per-parser memory closure; SIR-owned DOM | W3C/XML fixtures; entity amplification, namespace, incremental input |
| `SIRYaml` | YAML 1.2.2 mapping keys, tags, sequence, anchors, aliases, documents | rapidyaml candidate only if conformance+allocator closure pass; otherwise complete native/approved alternate backend | Official yaml-test-suite + graph/alias/cycle and preservation tests |
| `SIRTOML` | TOML 1.0 and 1.1 typed tables, quoted keys, date/time, arrays-of-tables | toml11 candidate with tracked internal allocation; fallback native backend under unchanged API | Official TOML tests + invalid TOML cases |

The backend choices are **proposals**, subject to source/license/build/memory and conformance gates; public SIR APIs do not depend on a candidate vendor. A failed vendor gate requires an implementation that satisfies the same API, not a scope reduction.

libucl is **not** a universal backend: it lacks direct native support for the complete requested set and uses process-level allocator hooks in places. Do not import it unless a measured, separately justified use arises.

### Specific external references

- yyjson allocator API: https://ibireme.github.io/yyjson/doc/doxygen/html/api.html
- Expat memory suite: https://libexpat.github.io/doc/api/latest/
- YAML 1.2.2: https://yaml.org/spec/1.2.2/
- TOML 1.0 and 1.1: https://toml.io/en/
- Valve KV3 binary format survey/versions: https://github.com/ValveResourceFormat/ValveResourceFormat/blob/master/docs/guides/format-support.md

Expat's `XML_Memory_Handling_Suite` callbacks have no user context. The adapter must scope parser creation, parse, nested entity parsers, error handling and destruction under a reentrancy-safe per-thread context stack or use a reviewed vendor fork that carries instance context. Verify allocation ownership in `free` using a SIR-owned header. Reject the integration if nested parsing, callbacks or thread migration can route allocations to the wrong domain.

## Editor and cooked runtime

**Editor:** keep format-specific editable documents, source IDs/spans, comments/trivia where supported, insertion order, key casing, aliases/references and format header details. A preservation-mode writer must not silently discard relevant annotations; when unsupported it returns `SIR_CONFIG_UNSUPPORTED_FEATURE`. Generic read-only projections may exist for UI inspection, but editing still routes to the format-specific document API.

**Cooked runtime:** conversion is an explicit typed/schema-validated step, not a lossy automatic parser result. Emit versioned records with fixed widths, alignment, bounds, endianness, string encoding and hash/checksum. Input bytes, parser version/options, schema version and source dependencies determine a reproducible cook fingerprint. Runtime loader validates bounds and budget before publishing. Do not make the portable config parser depend on a live GRDG or editor.

## Integration and readiness

Registration path: `Source/Core/Config/CMakeLists.txt` contributes first-party sources to `SIRCore`; `Source/Core/CMakeLists.txt` adds `add_subdirectory(Config)` once the code exists. Vendor backends are private, pinned and optionally toggled in CMake; no Python/SCons build requirement. Tests join `Tests/Unit/CMakeLists.txt` and benchmark registration joins the actual `Tests/Benchmarks/CMakeLists.txt`. Windows/Linux CI/CTest, ASan/UBSan where available, Clang AST style checks and pinned formatter apply to implemented C++, not to this documentation-only commit.

Implementation dependency and acceptance plan: [IMPLEMENTATION.md](../Plans/ConfigParsing/IMPLEMENTATION.md). Callable API and lifetime contracts: [API.md](../Plans/ConfigParsing/API.md). Test matrix: [ACCEPTANCE.md](../Plans/ConfigParsing/ACCEPTANCE.md).

**Current acceptance:** focused native Clang C++17 tests using local source-contract fixtures passed (`-Werror`, ASan/UBSan and optimized release; `0 failures`). These are not an entire SIREngine CMake build. Windows/Linux CI, pinned clang-format 22.1.5, full conformance, fuzzing, memory-manager integration and editor/cooker acceptance remain NOT_RUN.
