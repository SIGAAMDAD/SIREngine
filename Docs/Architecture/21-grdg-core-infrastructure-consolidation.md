# GRDG/Core infrastructure consolidation — November 3, 2026

Canonical change review: **PR #23**, `integration/grdg-scheduler-platform-20261103`, targeting `develop`.

## Source ancestry and ownership

| Superseded PR | Source head | Canonical subsystem |
| --- | --- | --- |
| #11 native file, mapping, pipe and semaphore | `101f08a220bc2602f7ffcd141e629cc837cab0fa` | Core/Platform |
| #13 GRDG resource kernel and derived providers | `b9c442c49c9d5247c8a8dffe4675e08b1849542a` | Resources |
| #17 dedicated scheduler and task graph | `9bc5ae6d6094df65a8d3c4ce38dacdc8a279b881` | Core/Jobs |
| #15 child processes, filesystem, thread controls | `afb5de4aa1c9abf19b40da9ad4dedf153e450226` | Core/Platform |
| #20 managed page backing and telemetry | `d01d9b1febf9d2893c050c05435f8af0d2caabef` | Core/Memory |

The first three source heads are existing ancestors of the integration branch. #15 and #20 are incorporated via a multi-parent merge commit, with the branch-authored C++ blob hashes preserved exactly and only shared CMake/architecture-document registration conflicts reconciled.

## Canonical execution and memory contracts

- The **only** execution scheduler is Core `SIRTaskScheduler`, reached through `SIRTaskGraph`. The retired GRDG-local `SIRResourceTaskBatch` files and independent completion loop are deleted. No legacy scheduling adapters or compatibility entrypoints remain.
- GRDG is the single writer of canonical resource bytes; Core Job workers fill writer-allocated staging through native positional I/O or mapped file views, and publication validates resource revision.
- `Core/Platform` provides native Windows/Linux file operations, pipes, semaphores, filesystem metadata, child process management and current-thread controls.
- `Core/Memory` tracks page backing and retains deferred decommit accounting until verified reclamation. Its opt-in pending-page retirement does not discard live GRDG or worker memory.
- CMake registrations preserve all existing runtime, event, container, resource, scheduler, and platform targets; each platform adds source files only for its own operating system.
- New API callers use the native contracts directly. This engine is pre-alpha and no obsolete API facade is maintained.

## Acceptance

All five feature heads have passing repository CI at their recorded SHAs. PR #23's previous integration head `b9630e81d01755f30db5f69239555c7c8f0fdeb7` also passed native CI and scheduler sanitizers.

This **new combined tree** requires its own Windows/Linux compilation, CTest, pinned clang-format 22.1.5, and sanitizer/benchmark validation. Do not treat per-feature CI as cumulative acceptance. Original PRs are superseded once the combined tree is verified.

Separate renderer (Vulkan/D3D12), audio, and input PRs are outside this GRDG/Core consolidation and keep their own ownership until their integration points are source-complete.
