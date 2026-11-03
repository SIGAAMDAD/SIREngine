# Contributing to SIREngine

SIREngine targets a reusable Godot-derived engine, not a one-game mod collection. Read [the architecture](Docs/README.md), [C++ standard](Docs/Standards/cpp.md), and [migration gates](Docs/Architecture/13-migration-plan.md) before proposing changes. Preserve existing Godot editor/workflow behavior unless the change explicitly owns a replacement contract.

## Workflow

1. Open an issue or ADR for substantive cross-subsystem changes; identify affected ownership, memory domains, platform targets and acceptance tests.
2. Create a focused branch from `develop`. Keep one dependency-closed implementation change per reviewable commit. Use Conventional Commits (`feat(core): ...`, `fix(rendering): ...`, `build(ci): ...`), <=72-character header.
3. Run the `dev` CMake/FASTBuild preset, CTest, pinned clang-format and native repository hygiene checks. Validate changed engine targets with the appropriate CMake compilation profile.
4. Include actual compiler/link output and platform tests. A source audit or formatted patch without a real build is not a completed implementation.
5. Open a pull request against `develop` with a concise change contract, tested configs, performance/memory impact and unresolved gaps. Do not upload binary build outputs.

## Code review standards

Use C++17, the global namespace, canonical SIREngine header/record forms, and the repository's Hungarian naming rules for owned code. Use EASTL only through validated subsystem allocation policies; third-party code retains its own formatter. Runtime allocations require a registered domain. Publishing changes to GRDG/job/streaming ownership requires concurrency/lifetime tests. Public ABI or cooked-format changes require compatibility and versioning notes.

## Licenses and third-party material

Do not commit proprietary FMOD or Wwise SDK files, private keys, generated application binaries or third-party code without attribution. Preserve the existing repository and dependency license notices.
