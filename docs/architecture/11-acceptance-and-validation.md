# 11 — Engineering acceptance and continuous validation

**Status:** CI workflows and initial tests are established; the full engine validation matrix is a future gate.

## Distinct evidence classes

| Gate | Required proof |
| --- | --- |
| Provenance | Exact parent SHA, tree SHA and submodule commit identities |
| Source closure | Every intended source translation unit is compiled in a representative configuration |
| Formatting/style | Pinned formatter result plus codebase-patcher/Nomad semantic checks |
| Syntax/object/link | Real compiler and linker output for the source commit being published |
| Runtime | Boot and shutdown of editor/runtime on actual target platform |
| Asset cooking | Deterministic output IDs/hashes and loader failure tests |
| Memory | Domain-level committed/resident peaks, ownership and reclamation |
| Concurrency | ThreadSanitizer/stress for publication, epochs, queues and shutdown |
| Graphics/audio | Driver/device/provider initialization, loading, fault and recovery tests |
| Integration | The Nomad/Sigilbound sample scenes, import, save/load and game ABI |

Success in one column never substitutes for another. In particular, a SCons editor build does not prove that the new SIR runtime architecture exists.

## CI policy

On push/PR: Git hygiene, C++ formatting, dependency pin checks, foundation compilation and smoke tests for Windows and Linux. Nightly: complete editor builds using the pinned Godot fork for both OSs, with diagnostics/artifacts. macOS: manually triggered preview only until it meets the full acceptance matrix.

## Source-change policy

Before publication, run the source inventory, path/scope audit, affected C++ Clang AST audit, pinned formatting, exact-source compiler gates and available integration tests. Keep cache/artifacts out of Git history. For inherited Godot C++, run Godot's own pinned `prek` / clang-format checks rather than formatting it with SIR's policy.

For incomplete tests record `NOT_RUN` or `BLOCKED` with a concrete reason, not `PASS`. Each compiler error should identify the first actionable diagnostic and its source/config SHA. Reproduce and fix one dependency-closed source slice at a time.
