# 19 - Platform OS services: implementation and integration roadmap

Source authority: `SIGAAMDAD/SIREngine`, `develop` at `b33693abb2c9e0317bb83d5e9824e90a51b65203`. First-party C++17 declarations stay in the global namespace. Platform APIs live in `Source/Core/Platform/`; Windows and Linux use separate translation units. The runtime and editor are consumers, not owners of OS state.

## Implemented in this changeset

| Contract | Owner | Core operations | Failure/lifetime tests |
| --- | --- | --- | --- |
| `SIRChildProcess` | `Core/Platform` | Spawn by UTF-8 executable and exact argv array; optional cwd; poll, wait, timed wait, terminate, exit code; close or destruction kills and reaps remaining child | Self-launch, arguments with spaces and quotes, invalid launch, timeout, termination, cwd, duplicate start |
| `SIRPlatformFileSystem` | `Core/Platform` | Stat with explicit entry type, recursive directory creation, empty-directory removal, same-filesystem replace rename, streaming directory enumeration | Missing paths, recursive create, replacement, early enumeration stop, nonempty delete rejection |
| `SIRPlatformThreadControl` | `Core/Platform` | Current thread name, group-aware/64-bit affinity, background/normal/high priority hints | Name roundtrip, affinity roundtrip or explicit unsupported, invalid mask, background scheduling |

The initial process API inherits the environment and standard streams. It does not duplicate `SIRPlatformPipe`, which remains in unmerged PR #11. Future process redirection will compose with that API, rather than creating another pipe owner. Linux uses `posix_spawnp` with glibc's working-directory file action; Windows uses Unicode `CreateProcessW` with command-line escaping. Process ownership is strict: destruction terminates/reaps outstanding children. The exposed Linux exit convention maps signal termination to `128 + signal`.

The filesystem API is not a global path cache: enumeration invokes a synchronous callback with a borrowed name; consumers copy names they need after the callback. Linux `rename` is atomic when both paths are on one filesystem; Windows uses `MoveFileExW` with replacement and write-through flags. Crash-durable publication additionally requires correct file and parent-directory flush on POSIX, so consumers must not infer transaction durability merely from rename success.

Thread controls apply to the **calling** thread to avoid ownership races. Linux exposes the first 64 CPU indices, reports `UNSUPPORTED` for larger affinity masks, and uses nice values; Windows takes an explicit processor-group ID and uses Win32 thread priorities. OS privilege policies can reject high priority changes.

## Planned future PRs, ordered by dependency

| Wave | Scope | Prerequisites | Acceptance gates |
| --- | --- | --- | --- |
| P2 | Filesystem watcher: `inotify` / `ReadDirectoryChangesW` | FileSystem API | Recursive path changes, rename pairing, queue overflow and forced rescan |
| P3 | Mapping range flush, advice, prefetch, sliding windows | PR #11 file mapping | Alignment, 64-bit sparse views, COW non-persistence, truncation behavior |
| P4 | Pipe nonblocking/readiness and child stream redirection | PR #11 pipes, P1 child process | EOF, backpressure, inherited-handle closure, redirected stderr, kill during I/O |
| P5 | Atomic address wait/wake, timers, scheduler controls | Existing thread/atomics/semaphore | Wake races, ABA, monotonic deadlines, oversubscribed workers |
| P6 | Asynchronous file I/O (IOCP, io_uring, bounded fallback) | P3-P5, memory routing | Cancellation/late completion, buffer ownership, partial reads, shutdown, quotas |
| P7 | Crash diagnostics, profiler adapters, OS memory pressure | Core telemetry and memory manager | Symbolized crash report, page-fault and pressure accounting, fault injection |
| P8 | Interprocess IPC and shared memory | P4/P6 | Handle passing, permissions, peer failure, cleanup |

## Ownership and acceptance

Do not introduce namespace-owned SIREngine types or runtime heap allocations in per-job hot paths. Each new API has independent Windows/Linux test targets, same-commit header/definition/build registration, explicit error results, and lifecycle contracts. Test using the pinned SIREngine CMake/format/CI pipeline. The branch is not merge-ready until both OS jobs and source hygiene pass; local Linux sanitizer tests are supplemental evidence. No GRDG or container implementation is included in this PR.
