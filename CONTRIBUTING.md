# Contributing to SIREngine

SIREngine targets a reusable Godot-derived engine, not a one-game mod collection. Read [the architecture](docs/README.md), [C++ standard](docs/standards/cpp.md), and [migration gates](docs/architecture/13-migration-plan.md) before proposing changes. Preserve existing Godot editor/workflow behavior unless the change explicitly owns a replacement contract.

## Workflow

1. Open an issue or ADR for substantive cross-subsystem changes; identify affected ownership, memory domains, platform targets and acceptance tests.
2. Create a focused branch from `develop`. Keep one dependency-closed implementation change per reviewable commit. Use Conventional Commits (`feat(core): ...`, `fix(rendering): ...`, `build(ci): ...`), <=72-character header.
3. Run local CMake configure/build/CTest, pinned clang-format and the repository hygiene check. For Godot changes, run the source checkout's pinned formatting/pre-commit tools and its own SCons compile matrix.
4. Include actual compiler/link output and platform tests. A source audit or formatted patch without a real build is not a completed implementation.
5. Open a pull request against `develop` with a concise change contract, tested configs, performance/memory impact and unresolved gaps. Do not upload binary build outputs.

## Code review standards

Use the repository's C++17, EASTL alias and Hungarian naming rules for SIR-owned code; leave third-party code on its own formatter. Runtime allocations require a registered domain. Publishing changes to GRDG/job/streaming ownership requires concurrency/lifetime tests. Public ABI or cooked-format changes require compatibility and versioning notes.

## Licenses and third-party material

Do not commit proprietary FMOD or Wwise SDK files, private keys, generated application binaries or third-party code without attribution. Preserve the existing repository and dependency license notices.
