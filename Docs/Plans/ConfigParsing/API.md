# SIRConfig API contract — proposed C++17 public surface

**Status:** contract proposal (not existing headers). **Source baseline:** SIREngine `develop@0a7d47de`. **Owners:** `Source/Core/Config/`; test consumers `Tests/Unit/Config/`. The code blocks specify types and operations to implement; they are not yet compile-verified.

## Shared value/ownership types

New headers: `SIRConfigTypes.h`, `SIRConfigContext.h`, `SIRConfigBudget.h`, `SIRConfigIO.h`, `SIRConfigDiagnostics.h`, `SIRConfigNodeHandle.h`.

All new SIR-owned declarations are at C++ global scope. Public methods that can fail return `sirConfigResult_t` and use out parameters. All methods document pointer nullability; do not use throwing exceptions across this boundary.

```cpp
typedef enum sirConfigResult_e : uint8_t
{
    SIR_CONFIG_OK = 0,
    SIR_CONFIG_END,
    SIR_CONFIG_NOT_FOUND,
    SIR_CONFIG_INVALID_ARGUMENT,
    SIR_CONFIG_INVALID_ENCODING,
    SIR_CONFIG_SYNTAX_ERROR,
    SIR_CONFIG_TYPE_MISMATCH,
    SIR_CONFIG_DUPLICATE_KEY,
    SIR_CONFIG_INVALID_HANDLE,
    SIR_CONFIG_LIMIT_EXCEEDED,
    SIR_CONFIG_OUT_OF_MEMORY,
    SIR_CONFIG_IO_ERROR,
    SIR_CONFIG_RESOLUTION_ERROR,
    SIR_CONFIG_CANCELLED,
    SIR_CONFIG_UNSUPPORTED_FEATURE,
    SIR_CONFIG_UNSUPPORTED_VERSION,
    SIR_CONFIG_INTERNAL_ERROR
} sirConfigResult_t;

typedef struct sirConfigInput_s
{
    const uint8_t *pBytes;
    uint64_t nBytes;
    sirStringView_t sourceName;
} sirConfigInput_t;

typedef struct sirConfigSource_s
{
    void *pContext;
    sirConfigResult_t ( *pfnRead )(
        void *pContext, uint8_t *pOutput, uint64_t nCapacity,
        uint64_t *pBytesRead );
} sirConfigSource_t;

typedef struct sirConfigSink_s
{
    void *pContext;
    sirConfigResult_t ( *pfnWrite )(
        void *pContext, const uint8_t *pBytes, uint64_t nBytes );
} sirConfigSink_t;

typedef struct sirConfigLimits_s
{
    uint64_t nMaxInputBytes;
    uint64_t nMaxDocumentLiveBytes;
    uint64_t nMaxScratchLiveBytes;
    uint64_t nMaxOutputBytes;
    uint64_t nMaxIncludeBytes;
    uint64_t nMaxExpandedBytes;
    uint32_t nMaxNodes;
    uint32_t nMaxDepth;
    uint32_t nMaxScalarBytes;
    uint32_t nMaxIncludeDepth;
    uint32_t nMaxAliases;
    uint32_t nMaxRowBytes;
    uint32_t nMaxColumns;
    uint32_t nMaxRows;
} sirConfigLimits_t;

typedef struct sirConfigParseContext_s
{
    const sirContainerAllocator_t *pDocumentAllocator;
    const sirContainerAllocator_t *pScratchAllocator;
    sirConfigLimits_t limits;
} sirConfigParseContext_t;

typedef struct sirConfigDiagnostic_s
{
    sirConfigResult_t eResult;
    uint64_t nByteOffset;
    uint32_t nLine;
    uint32_t nColumn;
    uint32_t nSourceId;
    uint32_t nFormatErrorCode;
} sirConfigDiagnostic_t;

typedef struct sirConfigNodeHandle_s
{
    uint64_t nDocumentIdentity;
    uint32_t nSlot;
    uint32_t nGeneration;
} sirConfigNodeHandle_t;

typedef struct sirConfigMemoryStats_s
{
    uint64_t nDocumentLiveBytes;
    uint64_t nDocumentPeakBytes;
    uint64_t nScratchLiveBytes;
    uint64_t nScratchPeakBytes;
    uint64_t nCombinedPeakBytes;
    uint64_t nRetainedBackingBytes;
    uint64_t nBorrowedInputBytes;
    uint64_t nOutputBytes;
    uint64_t nAllocationCount;
    uint64_t nRejectedAllocations;
} sirConfigMemoryStats_t;
```

Required initialization: all maximums are finite, nonzero and checked, except format-irrelevant limits whose documented values are ignored. `nMaxInputBytes` applies to all input bytes (including chunked reads); `nMaxIncludeBytes` is cumulative for externally resolved inputs; `nMaxExpandedBytes` bounds decompression, XML expansion and YAML alias materialization. Limits are enforced without integer overflow, with document+scratch combined peak reported, not only each independent peak.

`pfnRead`: returns `SIR_CONFIG_OK` with `0 < *pBytesRead <= nCapacity` or `SIR_CONFIG_END` with zero bytes. Callback errors terminate the parse; repeated zero-byte success is invalid. This is a **synchronous** operation and cannot retain `pOutput` after returning.

`pfnWrite`: consumes exactly `nBytes` on `SIR_CONFIG_OK` or signals an error; cannot report a partial successful write. A failure may leave a partial external destination, so callers needing atomic files use an OS file transaction layer.

`sirConfigNodeHandle_t`: document identity, slot and generation must all match. Mutation invalidates removed/replaced nodes, not unrelated handles unless the owning document declares a global generation reset. No pointer-based persistent node identifiers.

`SIRConfigBudget` is an implementation class with `init( context )`, `tryAllocate( scope, bytes, align, out )`, `tryReallocate( scope, ptr, bytes, align, out )`, `free( scope, ptr )`, `getStats()`, and `shutdown()`, backing the two callback descriptors. Its allocation header stores origin allocator, requested bytes, alignment and scope; no hidden malloc fallback.

## Stable parser/writer behavior

For every nonstreaming format `X`:
- `X::parse(input, context, options, XDocument*, diagnostic*) noexcept`: parse into a private candidate and replace the output **only** on success. `pDocument` and context allocator pointers are mandatory; diagnostics optional. Nonzero size requires a nonnull byte buffer.
- `X::write(const XDocument&, options, sink, diagnostic*) noexcept`: validates data before writing, applies `nMaxOutputBytes`, reports partial external sink effects on failure, and never caches sink context.
- `XDocument::clear() noexcept`: releases all retained storage and invalidates all handles; `getMemoryStats(out*) const noexcept` returns data up to the last operation without allocation.
- `XDocument::getRoot(out*)`, `getNode(handle, view*)`, `find...()`, and `tryGet<Type>()`: return missing/type mismatch as results, do not coerce silently.
- `XDocument` is move-only, bound to a stable allocator descriptor; it is not concurrently mutable. Concurrent read-only access to an immutable document is allowed after publish; mutations require caller synchronization and never mutate an in-use reader view.
- `options` structs are specified by each format header and versioned only through source/API review. Editor preservation is an explicit option; byte-identical output is not promised unless a format test proves it.

All input APIs are synchronous and do not own OS files or launch jobs. No callbacks execute engine commands, network calls or loads except explicitly supplied include resolver callbacks.

## Valve: three distinct public codecs

```cpp
class SIRValveKV1
{
public:
    static sirConfigResult_t parseText(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirValveKV1ParseOptions_t &, SIRValveKV1Document *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t writeText(
        const SIRValveKV1Document &, const sirValveKV1WriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};

class SIRValveKV3
{
public:
    static sirConfigResult_t parseText(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirValveKV3ParseOptions_t &, SIRValveKV3Document *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t parseBinary(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirValveKV3BinaryOptions_t &, SIRValveKV3Document *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t writeText(
        const SIRValveKV3Document &, const sirValveKV3WriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t writeBinary(
        const SIRValveKV3Document &, const sirValveKV3BinaryOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};

class SIRValveCfg
{
public:
    static sirConfigResult_t parseText(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirValveCfgParseOptions_t &, SIRValveCfgProgram *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t writeText(
        const SIRValveCfgProgram &, const sirValveCfgWriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};
```

`sirValveKV1ParseOptions_t`: escape mode, case comparison policy, repeated-key policy (preserve by default), conditional evaluator callback, optional resolver descriptor for `#include` and `#base`. Resolver receives source ID and requested path and returns bounded bytes + a release callback; parser forbids implicit filesystem/network access. Include cycle detection uses canonical resolver IDs, and `#base` precedence is fixture-defined.

`SIRValveKV1Document`: `getRoot()`, `findFirst(parent,key,out)`, `findNextDuplicate(previous,key,out)`, `firstChild()`, `nextSibling()`, `getRawString()`, `tryGetInt32()`, `tryGetFloat()`, ordered iteration and transactional `setString()`/`remove()`.

`SIRValveKV3Document`: typed scalars and arrays, a single typed root, header encoding and format GUIDs, annotated/resource strings and binary byte strings. Binary options declare target variant (VKV3 legacy, KV3 versions 1–5), compression (none, LZ4, Zstd), and failure on unknown enum/version. Actual supported binary variants require individual differential fixtures before enabling their build flag. Decompressed bytes count against `nMaxExpandedBytes`.

`sirValveCfgParseOptions_t`: quote/escape/comment/semicolon policy and max command/argument lengths; `SIRValveCfgProgram`: `getCommandCount()`, `getCommand(index,out)`, `getArgument(command,index,out)`, `appendCommand()`, `eraseCommand()`. Execution and alias/cvar evaluation are **not** part of this parser.

## JSON

```cpp
class SIRJson
{
public:
    static sirConfigResult_t parse(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirJsonParseOptions_t &, SIRJsonDocument *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t write(
        const SIRJsonDocument &, const sirJsonWriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};
```

Strict RFC 8259 default; optional deliberately selected JSON-with-comments is a separate dialect value, not silent acceptance. `sirJsonParseOptions_t`: duplicate-key policy (reject default), integer-overflow policy (reject default), strict UTF-8 and input ownership mode. `SIRJsonDocument`: `getRoot()`, `findMember()`, `getArrayElement()`, `findPointer()` (RFC 6901), `tryGetInt64()`, `tryGetUInt64()`, `tryGetDouble()`, `tryGetBool()`, `getString()`, `setValue()`, `removeValue()`. Strings preserve embedded NUL with explicit byte length. Conversion to numeric types is lossless or returns `TYPE_MISMATCH`.

Backend: yyjson with nonnull `yyjson_alc` and SIR-controlled context. Verify yyjson's read, mutable-edit, write and error paths all use this allocator. The public document does not expose yyjson node pointers or malloc-managed output.

## CSV and TSV (one row engine, two default dialects)

```cpp
class SIRDelimitedReader
{
public:
    sirConfigResult_t openBuffer(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirDelimitedOptions_t & ) noexcept;
    sirConfigResult_t openStream(
        const sirConfigSource_t &, const sirConfigParseContext_t &,
        const sirDelimitedOptions_t & ) noexcept;
    sirConfigResult_t nextRow(
        sirDelimitedRowView_t *, sirConfigDiagnostic_t * ) noexcept;
    void close( void ) noexcept;
};

class SIRDelimitedWriter
{
public:
    sirConfigResult_t open(
        const sirConfigSink_t &, const sirConfigParseContext_t &,
        const sirDelimitedOptions_t & ) noexcept;
    sirConfigResult_t writeRow(
        const sirStringView_t *pFields, uint32_t nFields ) noexcept;
    sirConfigResult_t finish( void ) noexcept;
    void close( void ) noexcept;
};

class SIRCsv
{
public:
    static sirConfigResult_t parseTable(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirDelimitedOptions_t &, SIRDelimitedTable *,
        sirConfigDiagnostic_t * ) noexcept;
};

class SIRTsv
{
public:
    static sirConfigResult_t parseTable(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirDelimitedOptions_t &, SIRDelimitedTable *,
        sirConfigDiagnostic_t * ) noexcept;
};
```

`SIRCsv` defaults: comma, double quote, quote-doubling, CRLF/LF accepted, no header inference. `SIRTsv` defaults: tab delimiter with explicitly configurable quoting. `sirDelimitedOptions_t`: delimiter, quote, escape policy, newline policy, header policy, blank-row policy, column policy, max fields and strictness. `sirDelimitedRowView_t` is valid until the next `nextRow()` or `close()`; mutable table retains copies. `SIR_CONFIG_END` ends iteration. No implicit typed columns: explicit schema conversion only.

## INI

```cpp
class SIRIni
{
public:
    static sirConfigResult_t parse(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirIniParseOptions_t &, SIRIniDocument *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t write(
        const SIRIniDocument &, const sirIniWriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};
```

`sirIniParseOptions_t`: case comparison, duplicate-key policy (reject/first/last/preserve), repeated-section policy (reject/merge/preserve), root-level keys, comment chars, whitespace trim and quoted/unquoted values. `SIRIniDocument`: `findSection()`, `getValue()`, `getAllValues()`, `firstEntry()`, `nextEntry()`, `setValue()`, `removeValue()`, section ordered iteration. Store raw textual scalars; typed conversion only via explicit getters. No include/interpolation unless added as a named opt-in grammar extension.

## XML

```cpp
class SIRXml
{
public:
    static sirConfigResult_t parse(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirXmlParseOptions_t &, SIRXmlDocument *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t write(
        const SIRXmlDocument &, const sirXmlWriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};

class SIRXmlEventReader
{
public:
    sirConfigResult_t open(
        const sirConfigParseContext_t &, const sirXmlParseOptions_t &,
        const sirXmlEventHandler_t & ) noexcept;
    sirConfigResult_t feed(
        const uint8_t *pBytes, uint64_t nBytes,
        sirConfigDiagnostic_t * ) noexcept;
    sirConfigResult_t finish( sirConfigDiagnostic_t * ) noexcept;
    void close( void ) noexcept;
};
```

`sirXmlEventHandler_t` carries `pContext` and synchronous `pfnOnEvent`, returning `sirConfigResult_t` for cancellation/error. XML event types: start/end element, ordered text, CDATA, comment, processing instruction, declaration and namespace information. Callback event views expire when handler returns; no hidden retained copies.

`sirXmlParseOptions_t`: XML 1.0 conformance profile, namespace mode, entity policy with strictly bounded expansion, DTD prohibition by default, comments/preservation and source handling. `SIRXmlDocument` supports ordered mixed content: `firstChild()`, `nextSibling()`, `getNodeKind()`, `getQualifiedName()`, `getAttribute()`, `getTextSegment()`, and transactional edits. Tree and SAX share the tokenizer/engine, not separate incompatible XML grammars.

Backend Expat `XML_ParserCreate_MM`: its callbacks do not receive context, so use an audited scoped TLS allocation-context stack and allocation-owner headers (or an explicitly reviewed source change). Wrap **creation, feed, nested handlers, free**; test concurrent parser instances, same-thread nested parsing and allocator-domain isolation before considering backend accepted. No global unsynchronized memory-suite toggling.

## YAML

```cpp
class SIRYaml
{
public:
    static sirConfigResult_t parse(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirYamlParseOptions_t &, SIRYamlDocument *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t parseStream(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirYamlParseOptions_t &, SIRYamlStream *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t write(
        const SIRYamlDocument &, const sirYamlWriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};
```

YAML 1.2.2: sequences, mappings with non-string keys, scalar style, explicit tags, anchors, aliases and multi-document streams. `sirYamlParseOptions_t`: schema (failsafe/JSON/core), tag allowlist/policy, alias-cycle policy, preservation mode, maximum resolver expansion; defaults do not execute custom constructors. `SIRYamlDocument`: `getRoot()`, `getTag()`, `getScalar()`, `getMappingPair()`, `getSequenceElement()`, `getAliasTarget()`, `iterateDocuments()`. The graph uses non-owning edges for aliases; deletion visits owning nodes once; cycles do not produce recursive unbounded walks. Preserve original alias/anchor representation when writing.

rapidyaml can be used only after coverage and allocation review; any unsupported required YAML syntax is an explicit gate failure requiring backend replacement or augmentation behind the same API. Do not convert YAML to JSON to fake support.

## TOML

```cpp
class SIRTOML
{
public:
    static sirConfigResult_t parse(
        const sirConfigInput_t &, const sirConfigParseContext_t &,
        const sirTomlParseOptions_t &, SIRTomlDocument *,
        sirConfigDiagnostic_t * ) noexcept;
    static sirConfigResult_t write(
        const SIRTomlDocument &, const sirTomlWriteOptions_t &,
        const sirConfigSink_t &, sirConfigDiagnostic_t * ) noexcept;
};
```

`sirTomlParseOptions_t`: grammar version TOML 1.0 or 1.1, preservation mode, no implicit type coercion. `SIRTomlDocument`: `findTable()`, `findKey()`, `getArrayElement()`, `tryGetInt64()`, `tryGetDouble()`, `tryGetBool()`, `tryGetDate()`, `tryGetTime()`, `tryGetLocalDateTime()`, `tryGetOffsetDateTime()`, `setValue()`, `removeValue()`. Date/time records retain precision and offset (minutes), and do not silently apply a local timezone. Preserve table/array-of-table identity, quoted keys, dotted-key rules and declaration order. Duplicate key/table cases reject precisely.

Backend candidate toml11 must pass a complete allocation-closure and grammar test; otherwise use a native parser under this public API, preserving identical externally observable behavior.

## Contract tests to implement before large integration

- Parse-and-replace leaves preexisting document byte-for-byte semantically intact on OOM, invalid encoding, syntax error and callback failure.
- All allocators called with original alignment and owner; deep and highly fragmented documents cannot exceed set budgets or escape accounting.
- Null callback, zero-byte success read, out-of-range view, invalid/stale node, cross-document node, double free and destroyed source lease produce deterministic errors/assertions, never UB.
- Same-thread reentrant XML parsing and multi-thread XML parsing correctly isolate budgets.
- No format parser implicitly opens files, executes Valve CFG, invokes arbitrary YAML constructors or follows external XML entities.
- JSON integer overflow, CSV multiline quoted field across chunk, KV include cycle, XML entity amplification, YAML alias graph cycle and TOML duplicate table are all rejected or represented under their stated policies.
- Read-after-publish is thread-safe while immutable; writer/reader concurrency on mutable documents requires external serialization.
- Round-trip semantics are verified per format; editor preservation-mode behavior is separately tested; serialization output limit/failed sink error is fully surfaced.

Source paths and work tasks: [IMPLEMENTATION.md](IMPLEMENTATION.md). Fixture/test matrix: [ACCEPTANCE.md](ACCEPTANCE.md).
