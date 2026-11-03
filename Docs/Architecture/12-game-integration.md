# 12 — Engine-to-game contracts

**Status:** architecture contract.

SIREngine is shared infrastructure; The Nomad and Sigilbound are separate consumers. They may have different resource budgets, combat/quest systems, rendering quality tiers, audio banks, shaders, asset cook parameters and network protocols. Their game data and behavior must not become hard dependencies of SIR core.

## Engine exports

The engine will expose stable interfaces for resource handles and loads, memory domains and budgets, logging/profiling, scene/cooked-asset import, streaming, graphics provider options, audio event scheduling, jobs, physics dispatch and multiplayer transport hooks. Common handles are versioned and typed.

## Game-owned configuration

Game packages provide startup memory budgets and allocator-domain caps, resource residency priorities, content pack schemas, audio event/bank manifests, gameplay state and network packet definitions. The framework may provide generic compressed packet infrastructure but must not bake The Nomad's packet contents into the engine.

## Compatibility

The editor and game project share an asset schema/version declaration. SIR API/ABI and cooked resource formats are versioned separately. Breaking public interfaces require a migration document and validated adapters for both games. Game project integration tests should load representative scenes and execute save/load/streaming paths under each budget profile.
