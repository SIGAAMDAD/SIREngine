# Verification matrix

The applicable acceptance gate comes from the user request, current milestone or exact owning architecture contract. Always distinguish **required**, **performed**, and **passed**.

| Change class | Minimum verification | Additional qualification when scope requires |
| --- | --- | --- |
| Documentation/instructions | Markdown/YAML structural integrity, existing path links, contradiction audit | VS Code live harness smoke test |
| C++ source bug fix | Target compile; affected unit/negative tests; changed-file format | Multi-compiler, sanitizer, AST for risky C/C++ |
| ABI/layout/serialized data | Header consumer compile, layout/version tests and migration consumers | Compiler matrix, binary compatibility, cross-version reader tests |
| Concurrency/jobs | Deterministic lifecycle/shutdown tests; source-based ownership review | TSAN/ASAN, stress, fault injection |
| Rendering/driver | Shader/driver compile; resource lifetime and synchronization tests | Physical GPU runtime/device-loss validation |
| Container/allocator | Copy/move/lifetime/overflow tests, accounting and target compile | Source coverage, ASAN/UBSAN, benchmark distributions |
| Streaming/asset format | Valid/corrupt/versioned assets, eviction and lease tests | Memory/IO tracing and platform matrix |
| Performance optimization | Equivalent behavior plus matched baseline/candidate workload | CPU/GPU profile and repeatability statistics |

## Existing toolchain examples

The connected `main` presently provides FASTBuild `dev`, Ninja `tooling`, Linux sanitizer presets and `coverage-llvm23`. Check actual availability before invoking:

```
cmake --list-presets
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --preset tooling
```

Do not execute unavailable configurations as if they passed. `coverage-llvm23` requires its specified compilers and dependencies. The existing repository default C++ standard remains C++17 until an authorized migration.

## Explicit result language

- **PASS**: actual command/gate executed successfully, with receipt.
- **FAIL**: actual command/gate executed and failed, with relevant output.
- **NOT RUN**: absent hardware, tool, source or authorization; include reason.
- **UNVERIFIED**: analysis predicts a result, but execution evidence is missing.
