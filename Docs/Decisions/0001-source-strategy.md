# ADR-0001 - First-party engine source organization

- **Date:** 2026-11-02
- **Status:** accepted; revised with the built-in Terrain classification

## Decision

SIREngine is developed in a single first-party `Engine/Source/` hierarchy. Portable services live in `Core/`, built-in game-engine systems (including Terrain) in `Runtime/`, authoring tools in `Editor/`, and platform-dependent code in `Platform/`. External libraries can be pinned individually under `ThirdParty/`.

The original engine and framework implementations are reference material for source-level ports. Authenticated source revisions are reconciled and licensed correctly before importing code directly into its owning first-party subsystem. The repository does not track separate engine-source checkouts.

## Consequences

The minimal Core/Launcher builds without the future engine ports. Editor and terrain support become native engine-owned build targets only when their real implementation and tests exist. Source history and attribution remain explicit during each migration.
