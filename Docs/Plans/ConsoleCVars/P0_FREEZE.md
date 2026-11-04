# P0: Console/CVar shared contract freeze (ABI v1)

**Status:** P0 implementation baseline. **Source authority:** current SIREngine develop reconciled into feature/console-cvars-commands.
**Code:** Source/Core/Registration/SIRRegistrationTypes.h/.cpp. **Tests:** Tests/Unit/SIRRegistrationContractTests.cpp.
**Milestone boundary:** Shared types, policy, binary header validator, malformed catalog tests; P1-P7 own cooking, value banks, dispatch, persistence and sinks.

## Name and handle rules

Console/CVar keys use ASCII grammar [A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*, with 1-96 bytes and ASCII lowercase canonicalization. Dot-separated keys carry a module ownership ID. Unqualified built-ins and the prefixes sir. and console. are engine-reserved. Commands and CVars share one name collision domain. Parser arguments must never be interned by default.

Current develop already implements Source/Core/Strings/SIRStringPool with the four-byte class SIRStringAtom and SIRS v1 cooked catalogs. P1 must reuse that service rather than add a second atom API. Process-local atom IDs never persist. Cooked references use {catalog hash, ordinal}; archives and wire messages use stable schema IDs or canonical names. Command/CVar handles are two uint32 values {slot,generation}, with slot zero invalid and registration-epoch invalidation on reuse.

## Types, flags and override resolution

v1 scalar/value types: BOOL, I32, U32, F32, I64, U64, F64, ENUM_U32, UTF8_STRING. Vectors/colors/paths are later schema work. Name atoms do not store repeatedly changed string values; those need reclaimable owned leases.

Priority from lowest to highest: ENGINE_DEFAULT=0; DEVICE_PROFILE=10; PROJECT_PROFILE=20; USER_ARCHIVE=30; COMMAND_LINE=40; SESSION_CONSOLE=50; EDITOR_PREVIEW=60; SERVER_AUTHORITY=70. Equal-priority writes from one source identity use latest admitted revision; distinct identities tie-break by stable issuer ID then admitted revision. RemoveOverride recomputes the winner from remaining history. Lower-priority writes may update source history without generating an effective-value event.

Flags: ARCHIVE, READ_ONLY, INIT_ONLY, LATCHED, DEVELOPMENT, CHEAT, REPLICATED, HIDDEN, SENSITIVE, MODULE_OWNED, TRANSIENT. Reject unknown flags, ARCHIVE+TRANSIENT, and REPLICATED+SENSITIVE. Capabilities are READ, EXECUTE, WRITE, DEVELOPMENT, CHEAT, INIT, SERVER, SENSITIVE, ADMIN. Permission and source priority are independent.

| Origin | Default capabilities | Restrictions |
| --- | --- | --- |
| LOCAL_USER | READ, EXECUTE, WRITE | CHEAT/DEVELOPMENT/SENSITIVE require explicit grants and build policy |
| ENGINE_INTERNAL | READ, EXECUTE, WRITE, INIT | Extra privileges must be explicitly granted by the engine |
| EDITOR_TOOL | READ, EXECUTE, WRITE, DEVELOPMENT | INIT allowed only during bootstrap |
| SCRIPT / MOD | READ, EXECUTE | Mutations require scoped grants |
| DEDICATED_SERVER | READ, EXECUTE, WRITE, SERVER | SERVER only for REPLICATED schemas |
| REMOTE_ADMIN | READ, EXECUTE | Privileged rights require authenticated ADMIN grant |

READ_ONLY denies console/script mutation. INIT_ONLY accepts authorized writes during initialized bootstrap only; LATCHED stages changes until a named next safe point, or RESTART where explicitly declared. SENSITIVE readings, CHEAT writes, DEVELOPMENT writes and replicated server writes all require their matching capabilities.

## Event and transaction atomicity

CVar registry owns a bounded, preallocated publication channel with event-credit reservations. prepareBatch validates every write, reserves all publication capacity and string snapshots, and produces a single-use owner-thread token. Failed preparation changes nothing. commitBatch atomically publishes coherent effective values and one revision, then consumes already reserved event slots; post-commit notifications cannot fail due to allocation or queue-full. Cancel releases reserved credit. Callback mutations are deferred to the next phase. The existing SIRDeferredEventChannel does not offer reservation, so P3 uses CVar-owned staging rather than silently dropping events. Group sinks observe each revision once at their own safe points.

## SIRC v1 cooked catalog

Version remains 1. Header size: 120 bytes. All integer fields are little endian, never serialized native structs. Header layout:

| Bytes | Field |
| --- | --- |
| 0-3 | ASCII magic SIRC |
| 4-5 | Version 1 |
| 6-7 | Header bytes 120 |
| 8-11 | Endian marker 0x01020304 |
| 12-15 | Generator revision |
| 16-23 | Total bytes |
| 24-39 | Names section {offset U64, count U32, stride U32} |
| 40-55 | Commands section (same) |
| 56-71 | CVars section (same) |
| 72-87 | Scalability section (same) |
| 88-95 | SIRS string catalog content hash |
| 96-103 | Descriptor hash, FNV-1a64 over command/CVar/scalability record bytes concatenated |
| 104-111 | Payload checksum, FNV-1a64 over bytes after the 120-byte header including padding |
| 112-119 | Reserved zero |

FNV-1a64 offset basis 14695981039346656037, multiplier 1099511628211. Section offsets are 8-byte aligned; each has <=65536 records, exact strides 8/32/32/16, fits within totalBytes, and sections cannot overlap. Empty sections have offset=0,count=0,stride=0.

Name record stride 8: cooked ordinal U32, stable schema-name ID U32.
Command record stride 32: name index U32, help index U32, binding ID U64, flags U32, capabilities U32, module ID U32, argument schema ID U32.
CVar record stride 32: name index U32, help index U32, stable schema ID U32, type U32, flags U32, default offset U32, module ID U32, sink group ID U32.
Scalability record stride 16: name index U32, values offset U32, value count U32, flags U32.
Optional help index = UINT32_MAX. P2 will validate record-level semantics, name ordinals and duplicate/binding/schema conflicts before activating a catalog. P0 validator deliberately does not execute descriptors.

## Configuration and lifecycle

INI is the selected typed project/user archive target when the existing Core/Config reader/writer and VFS atomic-replace path are available. Valve CFG is an ordered command script only and requires the actual merged parser. Do not create a second parser family. Startup: memory, strings, registration, events, typed CVars, commands, console, config consumers. Teardown reverses after quiescence.

## P0 verification

PASS: authored C++ formatted with pinned clang-format 23.1.5; isolated contract source/test compiled and run with Clang 23.1.5 and GCC 14, C++17 -Wall -Wextra -Wpedantic -Werror.
PASS negative fixtures: malformed magic/version/byte order/length, section misalignment/overlap/overflow/stride, invalid expected string hash, corrupted payload/descriptor hash, nonzero reserved bytes, invalid names, unsupported and conflicting flags.
NOT RUN: complete native engine CMake/link; Windows/MSVC; all-target AST closure and entire CI suite. P2/P7 own integrated acceptance.
The historical NMD donor archive is not required by P0; present develop's SIRStringPool is authoritative for the dependency.
