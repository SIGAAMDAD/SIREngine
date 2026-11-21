---
name: SIR verification and testing
description: Use real compiler, test, sanitizer and performance evidence for engine tests.
applyTo: 'Tests/**,Verification/**,Source/**/*Test*,.github/workflows/**'
---
# Verification

- Start with focused tests for touched classes and negative cases, then run the matching CTest suite and relevant integration gates.
- Use actual CMake presets/available tools and include full commands, exit status, counts, and failure logs in the final report.
- The historical targets include at least 90% subsystem coverage and more stringent 95% branch / 100% line in selected critical interfaces. Use the **current applicable milestone acceptance contract**, not an invented universal numeric threshold.
- For public ABI or concurrency-critical C/C++, compile the actual project and use a real Clang AST/source coverage where the qualification scope requires it; regex lint alone cannot establish safety.
- Run sanitizer, GPU, multi-compiler and benchmark checks when applicable and available. Mark unavailable hardware or configurations NOT RUN with a concrete reason, rather than blocking independent work or inventing evidence.
- Validate test failures and regressions against a pre-change baseline when feasible. Record compiler version and source SHA.
