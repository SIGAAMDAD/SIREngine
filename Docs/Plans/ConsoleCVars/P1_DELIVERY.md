# P1 — Canonical console/CVar string identity

**Branch:** `feature/console-cvars-commands`; **status:** implementation submitted, native integration still awaiting execution.

## Implementation

SIREngine already has `Source/Core/Strings/SIRStringPool.h/.cpp` and 40 dedicated GoogleTest cases on current `develop`. Retain its SIRS v1 cooked catalog, 4-byte `SIRStringAtom`, bounded caller-owned/core domain storage, leases, adaptive cache and existing failure paths. Importing NMDStringPool a second time would create conflicting identities, so P1 adds only the missing name integration in `Source/Core/Registration/SIRRegistrationStringBridge.h/.cpp`.

The bridge exposes three bounded, allocation-free adapter operations:

1. `sirRegistrationInternName` normalizes ASCII to lower case, validates the P0 grammar, applies engine/module namespace admission and interns only cold registration keys.
2. `sirRegistrationFindName` normalizes and looks up without inserting or allocating. Missing names return `SIR_REGISTRATION_NOT_FOUND` and clear output atoms.
3. `sirRegistrationCookedName` validates the expected SIRS catalog hash and ordinal, decodes into a fixed stack buffer, rejects noncanonical/oversized names and returns the pool-owned atom.

`sirRegistrationNameRequest_t` includes the owner ID (zero = engine). Module registrations require qualified names and may not register under `sir.` or `console.`. Generation/epoch authority belongs to the existing pool; no new identity counters or heap-managed name copies are introduced.

## Verification

- **PASS:** Clang 23.1.5 and GCC 14 C++17 strict-warning syntax checks and linked adapter tests with a test-only pool stub.
- **PASS:** Repository-pinned clang-format 23.1.5 on all authored files.
- **Added native CTest:** `core.registration.string-bridge` links `SIR::Core` against the **real** pool and tests cold intern, repeated normalization, read-only miss, invalid names, module namespace denial, allocation-call stability, cooked valid/wrong hash/out-of-range/noncanonical name, pool shutdown and stale atom behavior.
- **Previously existing source:** 40 SIRStringPool GoogleTests cover cache/lease/UTF-8/corruption/retire/concurrency; they were source-inspected but **not rerun** for this feature-branch SHA.
- **NOT RUN:** Native whole-`SIRCore` compile/link or CTest, Windows CI, actual performance counters, full AST and coverage gate.

The P1 issue remains open until the real native integration test passes on the exact published source. P2 is a separate cook/registration stage and does not reimplement the string pool.
