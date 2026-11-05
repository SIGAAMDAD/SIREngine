# Implementation roadmap — Console / CVars / Commands

**Base:** reconciled with `develop@1e799f4283c054b1a891ad23acc09c39d690b9c1` for P0.
**Branch:** `feature/console-cvars-commands`.
**Current delivery:** P0 closed ([#274](https://github.com/SIGAAMDAD/SIREngine/issues/274)); P1 closed after Linux/Windows native CI PASS (#276); P2 cooked registration completed and closed (#277) after native 99/99 Linux and 94/94 Windows CTests; P3 typed CVar registry completed and closed (#278) after native 100/100 Linux and 95/95 Windows CTests; P4 headless command service and its CTest implemented on the feature branch; exact-SHA native CI pending (#279). P5-P7 remain.
No artificial duration or performance claims; milestones are dependency-ordered and accepted by source/build/test evidence.

| ID | Scope and exclusive source owner | Depends | Deliverable / integration seam | Exit gate |
|---|---|---|---|---|
| P0 (complete) | Contracts, catalog identity; `Docs/Plans/ConsoleCVars/*` and future `Source/Core/Registration/SIRRegistrationTypes.h` | existing develop | Freeze names, flags, type set, precedence, callback ABI, event credit, error statuses and cooked schema | Contract review, failing negative catalog fixtures, source identity recorded |
| P1 (complete) | String identity: `Source/Core/Strings/*`, tests `Tests/Unit/Strings/*` | P0 | Reuse developed native SIRStringPool, bind canonical console names and SIRS ordinal lookup; no second NMD port | Clang AST for actual TUs, malformed NMDS, hash collisions, lease/epoch, no fallback allocation |
| P2 (complete) | Cooker/registration: `Source/Core/Registration/*`, `Tools/StringCatalogCooker/*`, `Tools/RegistrationCooker/*` | P1 | Deterministic cooked descriptors + versioned identity remap, dynamic overlay | Byte-identical recook, corrupt-section rejection, duplicate names/bindings rejected, CMake/GoogleTest |
| P3 (complete) | Typed CVar storage: `Source/Core/CVars/*`, `Tests/Unit/CVars/*` | P2 + event-credit contract | Compact value banks, checked handles, transactional writes, precedence history, latched states, event publication | Zero-alloc scalar cached get/set under reserved credits, atomic rollback, OOM/stale/priority tests |
| P4 (CI pending) | Native command backend: `Source/Core/Console/*`, `Tests/Unit/Console/*` | P2 + P3 | Cooked/dynamic command bindings, console ingress and pump, headless output/history/completion | Bounded grammar, permissions, callback unload, queue full, recursive exec, headless smoke |
| P5 | Config/archives: `Source/Core/CVars/SIRCVarConfigAdapter.*`, `Source/Core/Console/SIRConsoleConfigAdapter.*` | P3 + P4 + actual Config codec and VFS APIs | Typed startup user/project overrides, validated `exec`, ARCHIVE save/load via VFS | Parse failure no effects, batch atomicity, deterministic archive, failed rename recovery |
| P6 | Core/system + consumers: `Source/Core/SIRApplication.*`, `Source/Core/Events/SIREventRegistry.*` and targeted subsystem hook files | P1–P5 and owner review | Dependency-controlled system lifecycle, atom-named events, sinks/snapshots for render/physics/audio, scalability groups, script/editor opt-in | Config→CVar→event→subsystem single apply; module/shutdown ordering; no stale references |
| P7 | Verification: `Tests/Unit/{Strings,CVars,Console}/*`, `Tests/Benchmarks/ConsoleCVars/*`, CI target entries | P1–P6 | Windows/Linux CMake suites, sanitizer/fuzz/perf gates, coverage instrumentation | 100% line/branch for in-scope authored portable paths, integrated tests and controlled benchmark evidence |

### Dependency shape

```text
P0 contracts
  -> P1 Strings -> P2 cooker/registration
                    -> P3 typed CVars -> P4 commands/console
                                         -> P5 Config/VFS adapters
                                         -> P6 system + provider integration
                                         -> P7 full validation
```

P6's event-name registry migration may begin when P1's atom contracts are frozen but **must** be integrated by the Events owner; no competing edit to shared `SIREventRegistry` during P3/P4. Configuration codecs belong to existing `Source/Core/Config` ownership; P5 consumes only merged, real APIs and cannot invent a second tokenizer/parser family.

> P1 implementation details: [P1_DELIVERY.md](P1_DELIVERY.md). Existing `Source/Core/Strings/SIRStringPool` and 40 StringPool GoogleTests are present on current develop; the name binding adapter is in `Source/Core/Registration` to retain subsystem layering.

## Detailed work units and acceptance

### P0 — freeze shared contracts
- Establish all public structs/enums as SIR-style global-namespace C++17 declarations; max 4 function parameters; 140-column line width.
- Freeze effective name rules, slot generations, stable cooked schema IDs, reserved namespace, dynamic-module ownership, and status semantics.
- Choose one deterministic CVar precedence table with explicit override-removal behavior; specify CHEAT/READ_ONLY/REPLICATED capability matrix and priority independence.
- Freeze event-credit and sink revision semantics; decide whether an owner-thread staged queue guarantees reserved delivery capacity.
- Gate: round-trip catalog fixture spec, max sizes and corruption matrix approved for implementation.

### P1 — strings
- Import donor source only after byte-level identity/version verification; track original paths/digest and notices.
- Use one Core SIRMemory segment/allocator contract; preserve NMDS v2 decoding and 32-bit atoms, leases and dynamic bounded admissions.
- Reject stale epoch/invalid pool; test collisions, hash mismatch, full name table, decoded cache pinned during eviction, shutdown with lease and concurrent read/cold write.
- Gate: AST+unit tests under authentic build target; no per-lookup allocation.

### P2 — immutable registration
- Cook sorted, case-normalized command/CVar name records and distinct metadata groups. Dynamic names are cold overlays with explicit conflicts.
- Load validated little-endian versioned catalog, remap ordinals to same live pool, bind source callbacks by symbolic binding ID at activation.
- Gate: byte-identical output across source ordering, rejected unbound mandatory callbacks, bad spans, overflow and duplicate definitions.

### P3 — CVar registry
- Type-separated 4/8-byte banks, owned string storage, generation handles and revision tracking.
- Implement owner-thread typed get/set, stage/commit batch and origin-history remove/recalculate. Pending effective LATCHED state and sink grouping.
- Establish queue-capacity reservation rather than dropping events after commit. Profile only after correct semantics.
- Gate: all transactional/OOM/priority/latched/string reclamation tests; zero allocations for numeric cached get and reserved scalar set.

### P4 — command registry and service
- Use one normalized name index and explicit command/CVar collision rejection.
- Bind module-owned callback IDs; bounded MPSC ingress with separate input slab lifetime, owner-thread pump, async source diagnostics and output sinks.
- Implement help/list/set/get/reset/toggle, exec (only on real config grammar), completion, history and command budgets. Next-pump reentrancy.
- Gate: invalid/long/escaped/quoted command handling, permissions, stale callback, module drain, loop/recursion limits and headless test.

### P5 — configuration and archives
- Consume existing Core/Config typed/budget interfaces. Integrate Valve CFG when actually available; INI/JSON/TOML typed paths only after real format writer/reader exists.
- Validate entire override batch with source diagnostics before registry commit. Archive at a consistent revision through VFS user:/ and transactional Core/IO.
- Gate: invalid parse cannot execute; existing settings survive failure; deterministic round-trip; no direct parser callback performs mutation.

### P6 — consumers
- Modify SIRApplication to provide explicit dependency-order initialization and rollback; event registry string atom startup gap must be closed.
- Connect renderer/physics/audio subscriptions to revisioned snapshots at their own safe points; scalability profiles use same typed CVar batches; script/editor use same authority.
- Gate: one change batch results in one sink invocation per relevant subscriber, no half-updated provider state, clean shutdown with queued requests.

### P7 — coverage and benchmarks
- Tests: GTest per TU/branch including injected allocation/queue/full/cancel/epoch faults; ASan/UBSan, TSan where supported; CMake/CTest native Windows and Linux.
- Source-based coverage: instrument with LLVM, report per-file line/function/region/branch and zero uncovered in-scope authored paths; integration cases carry real engine entrypoints and shutdown.
- Benchmarks: cached numeric get, scalar commit, 100/1K/10K name registration, 1K command completion, parse/enqueue/pump, batch sink dispatch, string churn and archive I/O. Compare with donor and string-map baseline on same machine/build; report median/p95 and allocation bytes.
- Final gate: full scoped Clang AST from authentic compilation invocations, full coverage and integration evidence, reproducible results; absence of evidence is NOT_RUN, never PASS.

## Publication and ownership

Single integration authority: this feature branch's branch owner; each milestone merges sequentially after its owning tests, with explicit interface checks. Keep source edits scoped to listed owners and notify Events/Config/Core before modifying shared files. Conventional Commit messages; no force push. All changes must be formatted with repository-pinned clang-format 23.1.5 where C++ is touched. Retain source provenance; do not accept 'pass' from documentation alone.

## Historical open decisions (resolved for the P0 implementation baseline)

Full origin precedence order; included numeric/vector/path type set; namespace grammar and reserved built-ins; notification credit design; archive format and VFS readiness; latched phase; shipping/developer/editor permissions; exact donor source digest/latest revision.

Selected P0 policies, ABI v1 layout, status and acceptance evidence: [P0_FREEZE.md](P0_FREEZE.md). Remaining P1-P7 work retains its own verification gates.

P2 artifact and acceptance record: [P2_DELIVERY.md](P2_DELIVERY.md). P1 #276 closed after 97/97 Linux and 92/92 Windows native CTests; P2 [#277](https://github.com/SIGAAMDAD/SIREngine/issues/277) is closed after native acceptance on `82ef00bc7042b8e788c45324fb2df9a5b5dcfc6b`.

P3 implementation and test contract: [P3_DELIVERY.md](P3_DELIVERY.md), closed [#278](https://github.com/SIGAAMDAD/SIREngine/issues/278), accepted on `261e668f04eec44b14a5e22b117415e570ee9625`. P2 #277 remains closed.

P4 implementation and checks: [P4_DELIVERY.md](P4_DELIVERY.md), [issue #279](https://github.com/SIGAAMDAD/SIREngine/issues/279). P3 #278 remains closed. The validated `exec` command adapter is in P5 after the actual Config grammar integration; P4 reserves that spelling and rejects it until then.
