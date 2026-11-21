---
name: SIR C++ structure and formatting
description: Apply first-party C++ declaration, naming, and source formatting conventions.
applyTo: 'Source/**/*.{h,hpp,c,cpp,cc,cxx},Tests/**/*.{h,hpp,c,cpp,cc,cxx},Tools/**/*.{h,hpp,c,cpp,cc,cxx}'
---
# First-party C++ style

- Read the actual `.clang-format` and any path-local overrides. Pin clang-format 23.1.5; format **changed hunks/files only**, never `ThirdParty/`.
- Target policy C++23 with a C++17 fallback; before editing check the effective `-std` or `/std:` for that target. Current `main` CMake sets C++17.
- Use PascalCase paths and filenames, established `SIR` class/engine naming, global namespace, and fully qualified external symbols.
- `public` -> `protected` -> `private` access order; group constructors/destructors, operators, getters/setters, then ordinary methods. Unindent access specifiers per formatter.
- All constructors are `explicit` under the latest authorized standard, including zero-argument constructors when accepted by compiler configuration; check compilation of special member declarations.
- For new class bodies, allow in-class `= default` / `= delete` and constant-evaluated definitions where policy permits. Put other nontrivial definitions in source files. Do not add the `inline` keyword.
- Four-column tab indentation, 140 columns, structural opening braces on new lines, control braces always present; `T&` / `const T&`, and `T *p`; preserve source `.clang-format` as mechanical authority.
- No `auto`, lambdas, range-based loops, `std::function`, `decltype`, or `const_cast` in first-party new code. No aliases invented for convenience.
- Zero-parameter non-destructor declarations use `( void )`; destructors never use `( void )`. Preserve required ABI signatures in external interfaces.
- Keep cyclomatic complexity below 35, up to 3 conditions per conditional and 4 explicit function parameters; simplify through cohesive named functions rather than scattered macros.
- Member storage prefixes `m_`, static `s_`, external global `g_`; use semantic names consistently with the repository's existing naming contracts.
- Use `nullptr`, aggressive const correctness, appropriate `noexcept`, and explicit ownership. Do not add exceptions to first-party runtime error paths.
- Owned header guards use the most recently agreed `__SIR_[CLASS_NAME]_H__` convention with `SIR` removed from the class-name portion, respecting existing generated/header guards unless specifically migrating them. Source naming rules must not be silently changed wholesale.
