# Contributing to SIREngine

SIREngine targets a reusable Godot-derived engine, not a one-game mod collection. Read [the architecture](Docs/README.md), [C++ standard](Docs/Standards/cpp.md), and [migration gates](Docs/Architecture/13-migration-plan.md) before proposing changes. Preserve existing Godot editor/workflow behavior unless the change explicitly owns a replacement contract.

## Workflow

1. Open an issue or ADR for substantive cross-subsystem changes; identify affected ownership, memory domains, platform targets and acceptance tests.
2. Continue the existing canonical system branch from its verified remote HEAD, or create a focused branch from `develop` only if none exists. One writer owns each shared source boundary. Keep declarations, implementations, consumers, registrations, tests and formatting in the same semantic commit; do not publish per-file updates or avoidable CI/format fixup commits. Use Conventional Commits (`feat(core): ...`, `fix(rendering): ...`, `build(ci): ...`), <=72-character header; run `git config --local core.hooksPath Build/Hooks` to enable staged-source formatting and commit-message hooks.
3. Run the `dev` CMake/FASTBuild preset, CTest, pinned clang-format and native repository hygiene checks. Validate changed engine targets with the appropriate CMake compilation profile.
4. Include actual compiler/link output and platform tests. A source audit or formatted patch without a real build is not a completed implementation.
5. Open a pull request against `develop` with a Conventional Commit title and an explicit GitHub issue reference in its description. Record the exact source SHA, tests, performance/memory impact and unresolved gaps. Prefer squash integration for ordinary coherent features; preserve meaningful donor ancestry and never rewrite protected/shared history without owner authorization. Only the authorized integrator promotes a qualified combined tree. Do not upload binary build outputs.

## Code review standards

Use C++17, the global namespace, canonical SIREngine header/record forms, and the repository's Hungarian naming rules for owned code. Use EASTL only through validated subsystem allocation policies; third-party code retains its own formatter. Runtime allocations require a registered domain. Publishing changes to GRDG/job/streaming ownership requires concurrency/lifetime tests. Public ABI or cooked-format changes require compatibility and versioning notes.

## Licenses and third-party material

Do not commit proprietary FMOD or Wwise SDK files, private keys, generated application binaries or third-party code without attribution. Preserve the existing repository and dependency license notices.
