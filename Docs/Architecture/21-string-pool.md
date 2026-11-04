# 21 — SIREngine String Pool v1

**Status:** Native source implementation candidate. Exact CMake/GoogleTest integration requires the SIREngine develop checkout and its pinned submodules; see coverage report.

## Provenance

This port was derived from user-uploaded `nframework.zip` (SHA-256 `b648a15d2420bd5d13d8fd70343c1430a16c85aafed117130602a4350fe042f6`) at `Framework/Core/Containers/NMDStrings.h` and `NMDStrings.cpp`.

Donor file digests:
- Header: `e2bf82757731d79b7f4a5d292939a619c0dc9fc0c15f50488c0430f302d2f148`
- Source: `907f772feebdc107db547b411bae4b78ef6a7636a3cb5a1f1e65051138f7cb63`

## API and ownership

- `SIRStringAtom` is a four-byte process-local identity. Atom values are never persisted and are invalid after pool retirement/reinitialization.
- `SIRStringPool` interns UTF-8 names, maintains one hash index, and resolves cooked ordinal-to-atom identities.
- `SIRStringLease` owns a cache pin or direct-string lease. The pool must outlive every lease and `SIRStringTextRef`; failed `shutdown` is a hard lifetime violation in destructors.
- Memory comes from a supplied SIRMemory-backed `sirContainerAllocator_t` or a caller-owned span. No global allocation fallback.
- Cooked entries are immutable after successful catalog validation. Dynamic names are bounded and append-only until explicit scope retirement.
- SIMD/grouped index and adaptive index are compile-time experimental switches, not a runtime ABI.
- The optional XXH3 hash lookup fast path requires a future explicit pinned `xxHash` dependency; the current default is FNV-1a with full bytewise collision resolution.

## Catalog version reset

The new `SIRS` catalog has magic `SIRS`, version **1**, an explicit 48-byte little-endian header and 8-byte records, with raw and backreference-compressed UTF-8 payloads. It preserves the donor's structural grammar but does **not** accept the prior `NMDS` v2 magic. A SIREngine-native cooked catalog generator must output `SIRS` v1, recalculate payload and corpus hashes, and produce deterministic sorted unique names. Existing NMDS v2 artifacts must be recooked.

## Concurrency

A spinlock serializes string admission; a separate cache lock serializes lazy cooked decoding and eviction. Atom publication is release ordered; readers use acquire semantics. Caller establishes an exclusive boundary for pool shutdown and scope retirement.

## Integration notes

`Source/Core/Strings` is linked into SIRCore through `add_subdirectory(Strings)`. Optional GoogleTest target `core.strings.pool.googletest` integrates in `Tests/Unit/StringPoolTests.cmake` when `SIR_WITH_GOOGLETEST` is enabled. Tests include malformed cooked catalogs, valid/invalid UTF-8, dynamic storage exhaustion, atom/lease lifetimes, scope retirement, concurrent interning, cache eviction, allocation faults and mutated backing-catalog detection.

## Verification state

The donor-port was built and exercised in a bounded **local source fixture** using Clang/clang-format 23.1.5 and a GoogleTest-compatible assertion shim. This is not the actual pinned GoogleTest library or full SIREngine build. LLVM coverage is source-derived and counts compiled scope only, not noncompiled experimental variants. Results are documented separately in the task handoff. A full repository AST and upstream CMake/CTest integration are still required before claiming develop merge readiness.
