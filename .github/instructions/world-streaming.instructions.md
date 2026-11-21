---
name: SIR world and streaming
description: Preserve terrain streaming, region state, HLOD and world simulation contracts.
applyTo: 'Source/Streaming/**,Source/World/**,Source/Terrain/**,Source/Mesh/**'
---
# World and streaming

- Separate simulation state, region/chunk state, rendering residency, visibility and cooked asset lifetime.
- Preserve deterministic world transitions and resource lease/refcount contracts; validate unloaded/evicted/in-flight corner cases.
- For streaming budgets, report actual memory, disk I/O, CPU budget and GPU-transfer cost instead of speculative asymptotics.
- Keep editor-specific capabilities isolated from stripped cooked runtime paths; do not reintroduce editor-time producers into shipping runtime by accident.
