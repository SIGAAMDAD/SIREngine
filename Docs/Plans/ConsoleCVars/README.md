# Console / CVar / Command System — implementation packet

**Branch:** `feature/console-cvars-commands` created from `develop@51666fadadf8112b3b2d5bf919ba11dde55d931b` (2026-11-04).
**State:** P0 closed; P1 string identity adapter source and tests implemented, native full integration pending. P2-P7 remain.
**Design:** [Architecture](../../Architecture/20-console-cvars-commands.md).
**Contracts:** [Interfaces](INTERFACES.md).
**Milestones:** [Roadmap](ROADMAP.md).
**Verification:** [Acceptance](ACCEPTANCE.md). **P0 contract:** [P0 Freeze](P0_FREEZE.md).
**P1 implementation:** [P1 Delivery](P1_DELIVERY.md). **Machine task ledger:** [plan.json](plan.json).

## Purpose

Build one native, memory-accounted, cooked-and-dynamic command/CVar registry using Source/Quake interaction semantics and UE5-style override precedence, grouped sinks, scalability profiles and thread-safe reader publication. Adapt tested NomadFramework algorithms where useful. Maintain global-namespace C++17 conventions and current SIREngine lifecycle.

## Current baseline vs future work

Verified current develop has SIRMemory, typed SIREvent queues, SIREngineSystem/SIRApplication lifecycle and partial Core/Config (CSV/TSV). Strings is already present on current develop; P0 adds Registration contract types, validator and target. CVars and Console remain unimplemented. The legacy ConsoleManager remains on main only as a behavior reference. This packet adds plans and makes no product-code modifications.

## Source references

- `Source/Core/CMakeLists.txt`, `Source/Core/SIRApplication.h/.cpp`, `Source/Core/Systems/SIREngineSystem.h`
- `Source/Core/Memory/SIRMemoryManager.h`, `Source/Core/Containers/SIRContainerResult.h`
- `Source/Core/Events/SIREventRegistry.h`, `SIRMPSCEventQueue.h`, `SIRDeferredEventChannel.h`
- `Source/Core/Config/SIRConfigTypes.h`, `SIRConfigBudget.h`, `Source/Core/Config/CMakeLists.txt`
- `Docs/Architecture/19-configuration-parsing.md`, `Docs/Standards/cpp.md`
- Drive plans: SIR-CORE-PORT-002 and SIR-CONSOLE-ARCH-001 (the latter subordinate on conflicting details)

## Boundaries

P0/P1 can begin after interface review; the rest follow explicit dependency gates. No feature is complete without compiled engine evidence and tests. GitHub docs-only commit confirms only publication of design artifacts.
