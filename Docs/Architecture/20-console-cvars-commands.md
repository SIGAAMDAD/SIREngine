# 20 — Console, CVars, Commands and Cooked Registration

**Status:** P0/P1 completed; P2 cooked registration implementation native-verified on Windows/Linux; CVar typed storage and transactions are implemented through P3; native headless console dispatch is implemented through P4; archive/persistence and provider integration remain future P5–P7.
**Baseline:** `SIGAAMDAD/SIREngine develop@51666fadadf8112b3b2d5bf919ba11dde55d931b` (2026-11-04).
**Branch:** `feature/console-cvars-commands` from that exact commit.
**Execution packet:** [ConsoleCVars](../Plans/ConsoleCVars/README.md).
**Prior plans:** SIR-CORE-PORT-002 (primary); SIR-CONSOLE-ARCH-001 (historical details); SIR-CORE-MASTER-001 (lifecycle/VFS). This document refines their target design without representing owner ratification of unresolved options.

## 1. Scope and evidence

One native Core subsystem provides Quake/Source-style named commands/CVars and UE5-style typed access, setting origins, configuration precedence, render-safe snapshots, grouped change sinks and scalability profiles. It must serve editor, headless game, dedicated server, scripts and optional remote-admin consumers using the SAME dispatch/validation boundary. Registration is cooked by default, with bounded cold dynamic overlay. Preserve one shared name identity service, one type/event system, one configuration parser family and one allocator owner.

Source evidence as of baseline:
- `Source/Core/CMakeLists.txt` registers Platform, Memory, Events and Config, **not** Strings, Registration, CVars or Console.
- `Source/Core/SIRApplication.h/.cpp`: application borrows `SIREngineSystem` instances, owns an `SIREventRegistry` by value, registers up to 16 systems and ticks in registration order.
- `Source/Core/Systems/SIREngineSystem.h`: `initialize/update/fixedUpdate/shutdown` lifecycle exists.
- `Source/Core/Events/SIREventRegistry.h`: `MAX_EVENTS=64`, `char szName[48]`; event names are not atoms yet.
- `Source/Core/Events/SIRMPSCEventQueue.h`: bounded trivially-copyable MPSC payload, one consumer; full and stopped are explicit outcomes.
- `Source/Core/Events/SIRDeferredEventChannel.h`: bounded owner-thread publication; `enqueue` can fail, with no reservation API.
- `Source/Core/Containers/SIRContainerResult.h`: `sirContainerAllocator_t` with callback, domain and owner tag.
- `Source/Core/Memory/SIRMemoryManager.h`: shared virtual reservation, `SIR_MEMORY_DOMAIN_CORE`, domain budgets and tracing.
- `Source/Core/Config/SIRConfigTypes.h`: typed input/source/sink, allocator-backed parsing context, budget limits and source diagnostics.
- `Source/Core/Config/CMakeLists.txt`: `SIRConfigBudget.cpp` and delimited CSV/TSV implementation only. Valve CFG, INI, JSON, TOML and remaining codecs are *not* registered here.
- `Docs/Standards/cpp.md`: global namespace, C++17, no-throw ABI, 4-param maximum (per owner's latest rule), 140-column style, fixed-width SIR types and pinned clang-format 23.1.5. Follow latest owner class-group ordering when actual headers are authored.
- Source donor: NomadFramework `NMDStrings`, `NMDCVarSystem`, `NMDConsole`, `NMDRegistrationCatalog`. Earlier archive inspection documented it; the actual current uploaded donor revision must be re-hashed and verified before copying code. Legacy `main:Engine/Core/ConsoleManager` is behavior inventory, not a runtime transplant.

### Non-goals
No second logging backend, general scripting language, configuration parser framework, allocator, event bus, networking protocol, standalone GUI console or independent global service singleton. This branch begins with documentation; source code is a later milestone.

## 2. Ownership and dependencies

```text
SIRApplication / SIREngineSystem dependency-ordered lifetime
  SIRMemoryManager
    SIRStringPool (name identity; cooked NMDS catalog + dynamic admission)
      SIRRegistrationCatalog (immutable metadata + validated dynamic overlay)
        SIRCVarRegistry (single writer of effective/pending values and revisions)
        SIRCommandRegistry (single writer of binding slots and callback lifetimes)
          SIRConsoleService (bounded ingress, parser adapter, history, output, execution)
  SIREvent / SIRDeferredEventChannel / SIRMPSCEventQueue (existing delivery)
  Core/Config (format parser / serializer, no execution)
  VFS/Core IO (future archive and exec sources; no native filesystem calls in registries)
```

- Core/Strings: 32-bit `sirStringAtom_t`, pool epoch, cooked ordinal remap, lease lifetime. ASCII-casefold **only** admitted console/CVar name syntax, not all UTF-8 strings. Never persist atom IDs.
- Core/Registration: validate versioned metadata, identity, uniqueness and binding IDs; registry publishes only complete validated descriptors. Cooked tables are read-only after attach.
- Core/CVars: all effective/pending value writes, history/priority resolution, batch transactions and change revision.
- Core/Console: command callbacks and dispatch, input queue and output; **no** ownership of variable storage or disk parsing.
- Core/Config: bytes-to-typed-document or Valve CFG ordered statements. Parse never executes.
- Core/Events: delivery mechanisms. One typed CVar-change event class + optional revision-batch event; never one event type for each CVar.
- Subsystem consumers: renderer/physics/audio/terrain/network each own their *applied* provider state, synchronized via owner-specific safe points.
- Memory: all persistent/temporary allocations from declared `sirContainerAllocator_t`, backed by SIRMemory and owner tags. Strings that change repeatedly use reclaimable allocations, not NMD's append-only name arena.

No registry gets independent process-wide lifetime; the application owns the system wrapper, which borrows or owns its dependencies explicitly. Startup order matters and teardown reverses it after all producers, callbacks and leases drain.

## 3. Canonical identity and registry

Canonical command and CVar name = validated ASCII identifier converted once to lowercase + optional explicitly delimited module namespace; command and CVar names share an effective key space and cannot collide. `cl_Foo` and `cl_foo` are one name. A hash collision must compare canonical bytes, not only a digest. Cooked name references are `{catalogHash,ordinal}` remapped to process-local atoms at initialization; optional module catalog uses `{moduleStableId,catalogHash,localOrdinal}`.

Immutable descriptor groups:
- **CVar descriptor:** name/help/group ordinals, stable schema ID and version, type, flags, default bytes, min/max/enum/string constraints, override policy, change-sink group, module owner ID.
- **Command descriptor:** name/help ordinals, typed argument schema, flags/capabilities, stable binding ID, module owner ID.
- **Scalability descriptor:** group/profile name, members with canonical CVar IDs and validated typed preset values, default device-profile relations.
- **Registration header:** magic, version, endian, lengths, offsets, alignment, source revision, string-catalog hash, descriptor hash, integrity field.

A binary catalog never contains process pointers, callback addresses, CVar handles, process atom IDs, C++ padding or platform-native serialized structs. Runtime callbacks are rebound using a stable binding ID; an unresolved mandatory binding rejects activation. Cold dynamic overlays are bounded, versioned, and removed only after callbacks/arguments in flight finish or cancel.

## 4. CVar data model and hot storage

A public `sirCVarHandle_t` is `{uint32_t nSlot,uint32_t nGeneration}` (invalid slot 0). Slot metadata maps to a type-specific value bank. Suggested banks: 4-byte bool/int32/uint32/float32/enum/flags; 8-byte int64/uint64/float64; separately allocated reclaimable UTF-8 strings with generation-checked lease/handle; vector/path types require layout and ownership validation before acceptance.

Cold descriptors retain name atom, flags, range, grouping, default, source/priority and sink metadata; hot reads fetch validated typed current values without interning, parsing, string hashing, vtables or dynamic allocation. Cached reader access requires a valid registration epoch. Module unload bumps generations. A borrowed string view is valid only with a documented read lease.

Suggested flag model: `ARCHIVE, READ_ONLY, INIT_ONLY, LATCHED, DEVELOPER, CHEAT, REPLICATED, SENSITIVE, MODULE_OWNED, TRANSIENT`; `HIDDEN` and demo/notify flags remain candidate extensions. Define flag combinations in the contract before use. Error results distinguish stale handle, wrong type, denied origin, bad range, queue full, no change, OOM and invalid catalog.

### Overrides, sources and precedence (UE5-derived)

A CVar has *one effective hot value*, plus cold origin records and optionally a pending latched value. Candidate origin classes: `ENGINE_DEFAULT, DEVICE_PROFILE, PROJECT_PROFILE, USER_ARCHIVE, COMMAND_LINE, SESSION_CONSOLE, EDITOR_PREVIEW, SERVER_AUTHORITY`. These are proposed semantics, NOT an approved total ranking. A deterministic precedence table must be ratified before implementation; support explicit priority numbers and stable equal-priority tie-breaking by admitted revision. Removing a temporary override must recalculate the effective value from retained lower-priority sources, not reset unconditionally to the cooked default.

An origin permission is separate from override priority: a high-priority source cannot bypass READ_ONLY/CHEAT/SENSITIVE restrictions. Network authority applies only to CVars whose schema permits replication; wire IDs must be stable schema IDs/canonical paths, not atom/handle identities.

### Mutation transaction and notification

```text
resolve handle + typed origin
 -> verify registry/module epoch, type, capability, range and priority
 -> stage replacement / pending-latch / batch override source
 -> reserve all required change-notification capacity and string leases
 -> atomically publish effective value(s) and revision on owner lane
 -> enqueue ordered typed CVarChanged / batch summary
 -> retire old string snapshots after readers/events drain
```

No-change writes do not emit changes. A change that loses precedence may update source history without changing the effective value; emit effective-change events only if the visible value changed. A failed batch leaves effective values, origin histories, revision and required events unchanged. `LATCHED` changes remain pending until a declared apply point; effective-value publication occurs there.

**Required dependency fix:** current `SIRDeferredEventChannel::enqueue` has no reservation mechanism. Introduce a credit/reservation contract in Core/Events or a CVar-owned bounded staging channel with guaranteed post-commit publication. Never commit values then silently drop mandatory events. Keep callback-triggered writes deferred to the next mutation batch.

Sinks subscribe by affected group or key and receive a coherent batch revision once per commit, rather than triggering renderer resource rebuild on each related scalar mutation. Deliver typed `sirCVarChangedEvent_t` with handle/name/type/revision/origin/tick, and stable old/new snapshots; string payloads require owned leases or copies.

## 5. Command registration and execution

One logical name index resolves a key to exactly one command or CVar descriptor. CVar operations use typed registry methods; executable commands invoke explicitly bound native/script callbacks. Callback context and argument ownership are valid for the documented invocation lifetime. Module unload revokes registrations and drains/cancels in-flight dispatch.

```text
console/editor/script/server submits text + explicit capability origin
 -> bounded MPSC input descriptor + owned byte slab/ring
 -> owner-thread pump with commands-per-tick/time budget
 -> Core/Config Valve CFG parse (once implemented) or bounded interim tokenizer
 -> normalize command key / atom-find; type/schema/capability validation
 -> CVar mutation or command callback
 -> output record + bounded history + typed result
```

Nested `exec`, aliases and callbacks cannot execute recursively on the caller's C++ stack. They schedule additional bounded work for subsequent pump phases; per-request depth, command count, input bytes, output bytes and time budgets are enforced. A queued request stores its origin/capabilities and source revision, not transient parser memory. A module unload invalidates command handles and waits for executing callbacks to finish before releasing code.

Ordinary cooked built-ins: `help, cmd_list, cvar_list, get, set, reset, toggle, exec, writeconfig, console_clear, cvar_stats, stringpool_stats, event_stats`; `network_stats` only when networking is installed. Completion combines sorted cooked entries with dynamic registrations and returns typed/help metadata under a bounded output limit. History and output sinks work headless; any GUI is an optional consumer.

## 6. Configuration, persistence and scalability

- At bootstrap, Core/Config parses minimal settings before console startup. SIRMemory and the name catalog initialize before registries; registries stage overrides and activate console dispatch later.
- Existing `sirConfigInput_t`, `sirConfigParseContext_t`, `sirConfigDiagnostic_t` and `sirConfigSink_t` are the format integration seam. The current source only has CSV/TSV; Valve CFG and additional formats are blocked until implemented/merged. Do not introduce a competing parser.
- `exec` is authorized command execution **after** a complete bounded parse. INI/JSON/TOML project/user configuration maps to validated typed override batches, not arbitrary command execution.
- ARCHIVE writes snapshot a registry revision, deterministic sorted names and typed values; use VFS `user:/` through Core/IO staged + atomic-replace semantics once those interfaces exist. Failures leave prior archive intact and report error. No raw OS writes from console.
- Cooked scalability profiles are typed batches of overrides with origin `DEVICE_PROFILE` or another explicitly authorized source; batch sink notification applies renderer/terrain settings coherently.
- Remote admin, replication, scripting and editor consumers must use the same typed mutation/capability policy, never a bypass handler.

## 7. Threading and lifecycle

Writer: declared engine owner thread (single mutable registry authority). Readers: cached value reads only under frozen registration epoch or published immutable snapshot; use explicit acquire/release or proven atomic value semantics, not unsynchronized non-atomic reads. Producer threads enqueue bounded POD work descriptors; text/arguments are owned by separate leased buffers because `SIRMPSCEventQueue` requires trivially copyable payloads. Subsystems apply revisioned snapshots at their own safe points; a provider must not observe a half-applied batch.

Initialize: memory -> string pool -> registration catalog -> event-name attachment -> CVar registry -> command registry -> console service -> config and optional consumers. Adapt `SIRApplication` composition carefully: its current event registry is constructed by value before external SIRStringPool init, so add explicit late binding or dependency-controlled initialization. Shutdown: stop ingress -> drain/cancel queued commands -> detach modules/consumers and sinks -> flush optional archive -> drain events/leases -> destroy registry bindings -> catalog -> string pool -> SIRMemory. On any failed initialization phase, unwind only successfully activated resources in reverse order.

## 8. Source influence and exclusions

**Quake III / Source:** named ConVar + ConCommand registry, ConVarRef-style cached reads, archive/cheat/replication flags, command buffer, exec/aliases, help/completion, namespace conventions. Do not import Source per-variable allocations, global-locale mutations, legacy command objects or its full memory manager.

**UE5:** typed console manager, explicit value-set source, priority override history, scalability group assignment, render-thread-safe published values, grouped CVar sinks. Implement semantics with SIREngine's own handle+bank layout and typed event channels, not UE5's TAutoConsoleVariable object storage.

**NomadFramework:** NMD string pool, generation-checked CVar handles, owner-thread mutation, bounded console queue and runtime registrations are algorithmic donors. Port against actual source and tests, preserving provenance. Avoid duplicate parallel registries/parsers.

## 9. Dispatch verdict

Target system: **READY FOR OWNER TECHNICAL REVIEW**. This is a documented architectural proposal, not frozen ABI or a source patch. Open decisions: exact supported CVar types, complete override precedence, command-name/module namespace syntax, event-credit implementation, archive serialization format, latched apply phases and per-build origin permissions. All source implementation and runtime tests: **NOT RUN**. See [roadmap](../Plans/ConsoleCVars/ROADMAP.md) and [acceptance gates](../Plans/ConsoleCVars/ACCEPTANCE.md).
