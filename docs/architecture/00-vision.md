# 00 — Engine identity and design objectives

**Status:** normative design contract, implementation incomplete.

SIREngine is the reusable Godot-derived engine used for The Nomad, Sigilbound and future games. The editor, scene authoring, import tools, scripting workflow, inspection and platform-facing abstractions are retained to preserve authoring efficiency. Runtime systems are selectively replaced where resource ownership, memory accounting, streaming, renderer efficiency or determinism require a different model.

## Non-negotiable design principles

1. Preserve authoring capabilities while moving static, cooked runtime data away from persistent Node-object representations.
2. Conserve memory through explicit allocator domains, central accounting and one authoritative CPU-owned copy of each uniquely identified resource payload.
3. Adopt 7th-generation console-era engineering values: compact hot records, predictable lifetimes, low allocation rates, explicit jobs, good cache locality, offline cooking and bounded I/O.
4. Use C++17. Public SIR containers are EASTL aliases; modules should not invent parallel container and allocation conventions.
5. Treat Windows and Linux as the first acceptance platforms; macOS implementation follows stabilization of those targets.
6. Keep gameplay-specific data, networking packet schema and fixed memory limits in the game layer, not the engine.
7. Maintain buildable intermediate milestones; neither a repository skeleton nor a passing formatter means the engine backend replacement has shipped.

## Architectural shape

The authoring frontend is Godot-derived; the common system layer owns memory, job scheduling, handles, resource residency and diagnostics. Graphics and audio are providers behind stable interfaces. The cooked runtime path loads compact asset records and shared payloads; editor nodes are retained for authoring.

All subsystem boundaries must have an owner, an allocation domain, a failure policy and observable performance counters. Explicitly budget CPU, GPU and external SDK allocations. Record benchmark methodology alongside results and compare equivalent scenes.

## Acceptance for an engine release

A release needs the defined platform/build matrix, deterministic source and dependency hashes, reproducible shader/asset cooking, allocation-leak checks, GPU/device-reset behavior, editor load/save compatibility, and measured residency. Until these gates pass, an architecture entry represents a target rather than an implemented feature.
