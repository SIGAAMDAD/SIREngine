# SIREngine Clang 23 enforcement (levels 1 and 2)

## Files

- Root `.clang-format`: LLVM 23.1.5 formatting decisions, preserving SIREngine's existing tabs, 140 columns, braces, spacing, and pointer/reference style.
- Root `.clang-tidy`: standard analyzer checks and enforceable naming/casting/constness rules.
- Root `.clangd`: reads the same `.clang-tidy`; compile database is resolved from the repository root.
- `Tools/Standards/sir_clang_policy.py`: Clang 23 JSON AST and first-party source policy checker, required for language rules not expressible through stock clang-tidy.
- `Tools/Standards/run_checks.py`: local gate runner for all three tools.
- `Tests/Standards/`: positive and negative fixture tests.

This package does **not** include a loadable clang-tidy C++ plugin. The custom checks are an executable standalone Clang AST gate, suitable for the same local qualification workflow. It depends only on Python 3 and the Clang 23 binary, not on unavailable Clang-Tidy C++ development headers. The separate gate is mandatory for the rules it implements.

## Compile database and clangd

Generate an authentic database with the existing CMake `tooling` preset (or the `tooling-cxx17` fallback preset). Then publish it to the root for clangd to discover:

```sh
python3 Tools/Standards/run_checks.py --repo . \
  --compdb out/Linux/tooling/compile_commands.json \
  --publish-compdb --clang clang++-23 --clang-tidy clang-tidy --clang-format clang-format
```

Use `out/Windows/tooling/compile_commands.json` when the CMake tooling preset on that platform produces it. The runner does not invent compilation flags, rewrite source, or mark a failed/partial run as passing. On Windows/MSVC compilation databases, use Clang tooling with matching frontend/driver support for the recorded commands.

## Standalone fixture tests

```sh
python3 Tests/Standards/test_policy.py /path/to/clang-23
clang-tidy --config-file=.clang-tidy Tests/Standards/positive.cpp -- -std=c++23
```

The AST test mode `--file` is for fixtures only. Repository qualification requires `--compdb`.

## Allowing documented special cases

Place a source comment immediately preceding the declaration, with a concrete reason:

```cpp
// SIR-POLICY-ALLOW: free-function required OS callback entrypoint
// SIR-POLICY-ALLOW: integer-abi externally defined C signature
// SIR-POLICY-ALLOW: double world-coordinate precision contract
```

The tool checks exact rule-specific markers. Unannotated code is rejected.

## Coverage boundaries

Clang AST checks enforce syntax, declaration layout, constructor/member policies, forbidden APIs, and header conventions. They do not prove that operator new/delete really routes to SIREngine's allocation manager, ownership semantics, smart-pointer correctness, performance preferences (SoA/specialized containers), proper fallback feature semantics, or cross-module ABI compatibility. Those require source/link/runtime evidence. The `readability-function-cognitive-complexity` check is an approximation, not exact cyclomatic complexity; exact cyclomatic complexity requires a separate CFG metric. This tooling does not claim those unimplemented gates.

First-party `Source/`, `Tests/`, and `Tools/` are in scope; vendor and generated directories are excluded. An existing codebase with violations will fail until remediated. No source rewriting is bundled.
