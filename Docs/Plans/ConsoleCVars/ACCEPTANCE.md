# Acceptance matrix and adversarial cases

**Observed:** this branch's deliverables are documentation. None of the proposed engine tests, Clang AST builds, coverage runs, sanitizers, or microbenchmarks have been executed for the Console/CVar implementation.

| Gate | Valid case | Deliberate negative | Required evidence |
|---|---|---|---|
| Catalog identity | Cook/load/remap canonical name | wrong hash, out-of-bounds section, duplicate command/CVar key, case alias | byte-level roundtrip and failure result |
| Atom lifecycle | pinned cooked name and dynamic insert | same hash different bytes; retire with active lease; stale pool epoch | no corruption/UAF; memory trace |
| CVar typed read | cached i32/f32/string lease | wrong type, generation reused, simultaneous unsafe writer | type error, no alloc hot read |
| Override precedence | device/user/session stack, removal restores prior | lower-priority source clobbers higher; forbidden origin bypass | effective and history revision trace |
| Transaction | two valid CVar values, one coherent revision and sink | second invalid, allocation denied, queue full before commit | previous effective values/revision/events unchanged |
| Latched | pending then designated apply phase | application outside phase, duplicate effective notification | only apply-phase value visible |
| Notifications | ordered old/new with stable leased strings | queue full, subscriber reentrancy, string release before pump | all mandatory notifications preserved |
| Command registry | cooked binding + dynamic module callback | duplicate built-in, stale/unloaded module, callback still executing | rejected or safely drained invocation |
| Console parser | quoted/escaped/semicolon input within limits | malformed encoding, oversized token, recursive exec loop | bounded diagnostic; no partial unintended execution |
| Permissions | local internal authorized setter | remote/script/mod attempts restricted setter | DENIED without state mutation |
| Config | fully valid typed archive, deterministic sorted output | bad parser diagnostic, key collision, fail on write/rename | old archive retained; no command execution on parse |
| Scalability/sinks | coherent multiple CVar profile applies once | half-applied profile, render rebuild per member | one sink per batch with same revision |
| Lifecycle | startup, command run, clean teardown | init failure rollback; producer writing after shutdown; module unload while queued | all leases and callbacks retired |
| Replication | stable schema ID typed value | process atom/handle on wire; stale schema version | reject malformed packet |
| Memory | repeat mutable string changes without growth | OOM at each allocation boundary | live bytes reclaimed and accounted |
| Performance | baseline vs optimized same workload | different semantic outputs or hidden allocations | functional parity + reproducible comparison |

### Verification commands (future; adapt exact existing CMake presets)

- `cmake -S . -B build/console -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- `cmake --build build/console --target SIRCore`
- `ctest --test-dir build/console --output-on-failure`
- `llvm-profdata merge ...` and `llvm-cov report ...` for per-source branch/line/region coverage once instrumented targets exist.
- Native Clang AST gate for every in-scope TU from authentic `compile_commands.json` before static code acceptance.
- Repeat with Windows/MSVC, Linux/Clang, sanitizer and release benchmark configurations where toolchain is available.

The commands above are instructions, not claims of execution. Exact target names/presets must be verified when the actual implementation enters CMake. 100% line/branch for authored in-scope code is the acceptance target, not a present result. External vendor source is evaluated separately by its integration contracts.

### Design review checklist

1. Instruction fidelity: one registry family and existing parser/event/allocator owners; no hidden fallback allocations or new second framework.
2. Contract/ownership: module unload, reader lease, transaction and event reservation are closed before first implementation wave.
3. Falsification: stale handles, malformed cooked bytes, denied origins, partial transaction, full queue, init failure and live string-reader teardown all have executable negative cases.
4. No invented performance results, build success, coverage percentage or approved decisions.

**Verdict:** architecture READY FOR OWNER TECHNICAL REVIEW; runtime acceptance NOT_RUN.
