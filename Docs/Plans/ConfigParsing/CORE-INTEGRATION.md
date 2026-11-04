# Configuration core integration: allocator, stream and native file boundaries

**Baseline:** `SIGAAMDAD/SIREngine` branch `feature/config-parsing`, SHA `3af09b49cf435035cf68ce0e3f9c35495996060a`.
**Status:** Source slice implemented locally; remote publication awaiting repository-pinned clang-format 22.1.5 and full native CMake validation.

## Dependency findings from current source

- `Source/Core/Memory/SIRMemoryManager.h` is the page-commit authority and exposes per-domain `getDomainStats()` and `getStats()`.
- `Source/Core/Memory/SIRGeneralHeap.h` is an arbitrary-lifetime heap with domain-specific backend segments; `SIRContainerHeapAdapter.h` already exposes `sirContainerAllocator_t` with owner tags.
- `Source/Core/Memory/SIRAllocationRouter.h` is bound to an owner thread; job/frame routes have epoch-controlled reclamation and cannot individually free document allocations.
- `Source/Core/Platform/CMakeLists.txt` already splits Windows/Linux implementations, but there was no portable native sequential file-reader contract in the verified branch.
- `Source/Core/Config/SIRConfigTypes.h` defined callback sources/sinks and parse limits, but reusable IO budgets and platform file adapters were absent.

## Implemented interface slice

1. `SIRConfigMemoryScope::init(documentBacking, scratchBacking, limits)` creates independent `SIRConfigBudget` wrappers, accepts existing heap-adapter allocator descriptors, and exposes a `sirConfigParseContext_t` with the two bound descriptors. `shutdown` fails while either scope has live allocations. No heap fallback.
2. `SIRConfigBoundedSource::init(source, maxBytes)` and `getSource()` provide a reusable checked byte-source callback. On the exact limit boundary, a single-byte probe distinguishes EOF from overflow. Zero-progress reads and invalid counts fail. The source is borrowed and its callback owner must outlive the wrapper.
3. `SIRConfigBoundedSink::init(sink, maxBytes)` and `getSink()` reject overflow before the backing writer is called, and poison the writer after the first callback error. Partial external destination contents are not rolled back by this layer.
4. `SIRPlatformFileReader` provides a native UTF-8-path, read-only sequential file handle. Linux uses `open/read/close` with `EINTR` retry; Windows converts UTF-8 to UTF-16 and uses `CreateFileW/ReadFile/CloseHandle`. No parser heap allocations in the OS read path. Windows path conversion currently enforces a 4095 UTF-16-unit bound.
5. `SIRConfigFileSource::init(fileReader)` exposes an existing open file handle as `sirConfigSource_t` and maps native OS read results to config result codes. It deliberately does not own or close the file.
6. Register new implementation sources in `Source/Core/Config/CMakeLists.txt`, native OS implementation files in `Source/Core/Platform/CMakeLists.txt`, and focused unit tests in `Tests/Unit/Config/CMakeLists.txt`.

## Ownership and dataflow

```text
SIRMemoryManager (committed pages, per-domain caps)
    -> SIRGeneralHeap (arbitrary lifetime)
    -> SIRContainerHeapAdapter (domain ID, owner tag, callbacks)
    -> SIRConfigMemoryScope (document & scratch requested-byte caps)
    -> sirConfigParseContext_t
    -> format-specific parser / retained typed document

SIRPlatformFileReader [Linux / Windows, file handle owned by caller]
    -> SIRConfigFileSource [borrowed file reader]
    -> SIRConfigBoundedSource [bounded callback wrapper]
    -> SIRDelimitedReader / future JSON, XML, Valve, YAML, TOML streams

format-specific writer
    -> SIRConfigBoundedSink [bounded callback wrapper]
    -> caller-chosen destination and transaction authority
```

The reader/sink callback layers are synchronous and do not take ownership of the backing source/sink. A parsed document may outlive the source only if it owns copied data or an explicit leased/mapped input; those ownership semantics are still to be implemented for hierarchical formats. A parser worker may use a job scratch arena only if its data does not survive the job fence. Long-lived row-reader state must use arbitrary-lifetime storage or another explicitly retained owner lifetime.

## Implementation dependency gates remaining

1. **Real manager-backed integration test:** construct a functioning `SIRMemoryManager`, backing segments, `SIRGeneralHeap` and two `SIRContainerHeapAdapter` instances. Verify domain commit accounting, owner tags, denied page commits, failure unwinding and destruction order. Mock-allocator tests do not establish this gate.
2. **Platform writer:** add safe file output with staging, flush/fsync and atomic rename/replace semantics on Linux and Windows. Do not implement config saving by truncating the target before its replacement has been validated.
3. **File-system integration:** link path normalization, sandbox/virtual path policy, filesystem watches and package/archive streaming through a common platform file service. Do not make individual format parsers open paths independently.
4. **Streaming robustness:** add cancellable/deadline-aware jobs through the existing engine job system once its interface is available; asynchronous reads should not call document-mutating parser methods across threads without explicit ownership.
5. **Mapped and leased input:** support zero-copy views only through an owned, ref-counted or explicit lifetime lease tied to mapped-file or resource residency. Validate unmapping does not outlive borrowed document strings.
6. **Codec plumbing:** route JSON, INI, Valve formats, XML, YAML and TOML through these allocation and I/O contracts; preserve format-specific semantics and transactional publication.
7. **Validation:** use the complete native SIREngine checkout on Windows/Linux with the repository-pinned 22.1.5 formatter and CTest. Run ASan/UBSan and failure injection, then compare peak requested bytes against actual manager page commitments. Windows native reader still requires its Windows compile and runtime test.

## Validation of current local slice

Focused C++17 compile and runtime tests use the current first-party config sources with the existing SIR callback type shapes, a local no-op PCH and fake backing allocator callbacks. Linux native file read, bounded source/sink, parser streaming, out-of-memory budgets, error paths and reclamation are exercised. These tests do not claim a full engine build or production memory-manager integration. The formatter mounted locally is version 23.1.5; the repository requires 22.1.5. A remote push should await the repository's exact pinned formatting gate, rather than extrapolating from version 23.