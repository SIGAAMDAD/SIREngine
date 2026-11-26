/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

# SIREngine Clang 23 standards enforcement (levels 1, 2 and 3)

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

## Level 3 — Required build/CI gates

`Build/CMake/SIRStandards.cmake` registers `sir-format-check`,
`sir-tidy-check`, `sir-policy-check`, `sir-header-check`,
`sir-policy-regression-check`, `sir-standards-check`, and the separate
`sir-compatibility-check`. The standards workflow runs these from the
`standards-clang23` CMake preset and qualifies the C++17 fallback from
`standards-cxx17`. The reusable standards workflow is a required dependency of
`ci.yml`'s `sir/required` job. `Docs/Standards/ClangEnforcement.md` contains
the complete commands and qualification boundaries.

Only configuration and standards tooling are delivered; this package does not
rewrite legacy engine source to satisfy the stricter policy.

## Cross-platform development (LLVM 23.1.5)

Use `cmake --build --preset tooling --target sir-clangd-db` to expose an
**authentic** CMake compile database at the repo root. The same `.clangd` file
works with Linux Clang/GCC and Windows clang-cl/MSVC commands. Do not add
`-std`, Windows SDK paths, or Linux system include paths to `.clangd`.

On Windows, use a Developer PowerShell with Visual Studio/Windows SDK enabled;
configure Ninja with `-DCMAKE_CXX_COMPILER=clang-cl`. The custom AST and header
checks recognize MSVC-style compile-command arguments and select clang-cl
driver parsing. The Git pre-commit hook delegates to a Python implementation
and does not call Linux-only shell scripts.

See `Docs/Standards/ClangEnforcement.md` for commands and CI validation scope.
