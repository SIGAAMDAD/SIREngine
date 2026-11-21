# Coding standard enforcement matrix

The policy in [cpp.md](cpp.md) is authoritative. Formatting and warning configurations enforce only checks supported by their own tools. Absence of a native rule in `.clang-format` is not permission to violate an owner standard.

| Rule family | Enforcement owner |
| --- | --- |
| Tabs, braces, padding, 140-column limit, pointer/reference alignment, header sorting | `.clang-format` / `.editorconfig` |
| C++23 primary, C++17 fallback | CMake `SIR_CXX_STANDARD`, compiler and qualification builds |
| Compiler-specific flags, no exceptions, ABI and warnings | `Build/CMake/` owned-target rules and toolchain presets, including `SIR_STRICT_WARNINGS` |
| Language-level safety diagnostics, obvious missing `explicit`, override errors | `.clang-tidy` and compiler |
| Full `explicit`-on-every-constructor policy | Custom Clang AST policy check |
| Constructor/destructor grouping; class special members | Custom Clang AST policy check |
| Forbidden `auto`, lambdas, range-for, casts, namespaces, type aliases and free function definitions | Custom Clang AST policy check |
| Struct-only trivial utilities; no ordinary in-body class method definitions | Custom Clang AST policy check and owner review |
| Prohibited standard containers/algorithms and threading classes | Current AST/text policy gate; audited symbol-level resolution remains a qualification task |
| Header guard naming, PCH include, source path ownership | Header/path policy check |
| Allocator, smart-pointer, `new`/`delete` global override coverage | Link/symbol tests, allocator provenance and runtime tests |
| Concurrency memory order, bytewise object safety, SoA suitability | Compiled tests, sanitizers and owner review |
| Compatibility implementations | Separate C++23 and C++17 compilation plus behavioral tests |

## Current migration state

This revision updates policy and check configuration. Preexisting C++17 source remains a migration backlog until it is transformed and compiled. The historic `Build/CMake/SIRPCH.hpp` and preexisting `#include <SIRPCH.hpp>` references are not silently rewritten; the new `<Core/SIRPCH.h>` path must be implemented as part of the relevant source migration. The global replacement `new`/`delete` operators are a required implementation contract, not a claim that they are already present.

The LLVM 23.1.5 standalone AST policy checker implements the syntactic rule set described in `Tools/Standards/README.md` and is executed by the mandatory CI workflow. Header isolation and all-compilation-entry checks are additionally wired through the CMake targets. Allocator-routing, ABI stability, performance, and some semantic correctness requirements remain native-test or implementation obligations; they are **not** certified by these static checks.

## Executable Level 3 gates

See [Clang enforcement](ClangEnforcement.md) for the exact CMake targets, CI
workflow dependency, pinned tools, C++17 fallback mode, and local hook.
