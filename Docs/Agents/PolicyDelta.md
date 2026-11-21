# Checked-in standards versus possible future migration

Repository snapshot inspected via GitHub `SIGAAMDAD/SIREngine` `main` on 2026-11-20:

- `CMakeLists.txt` sets C++17; `.clang-format` says `Standard: c++17` and current `Docs/Standards/cpp.md` defines first-party source style.
- Some earlier agent instructions described C++23-first as an intended future policy and described alternative header-guard naming. **This optimized suite does not silently enforce unmerged migration rules.** Use the checkout's effective target standard and repository policy until the user explicitly requests that migration.
- FASTBuild `dev` is the common development path; Ninja `tooling` supplies a compilation database; inspect actual available presets/tools.
- The previous archive's language/guard target notes are historical proposals, not proof of applied source configuration.

When user-approved changes to standards happen, update code, formatter, compiler flags, and documentation as a deliberate separate migration with tests.
