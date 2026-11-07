# SIREngine-owned C++17 standard

**Scope:** first-party `Source/`, `Tests/` and future `Tools/` C++ sources. Externally licensed libraries under `ThirdParty/` retain their own API and formatting conventions.

## Source and directory naming

Use **PascalCase** for first-party repository directories, subsystem directories and C++ filenames. Preserve the existing `SIR` naming convention for engine-owned source and types. The established flat source root is `Source/`, with `Source/Core/`, `Source/Renderer/`, `Source/Terrain/`, `Build/CMake/`, `Tests/` and `Docs/`. Keep actual vendor directory names and fixed metadata names such as `.github/` unchanged. Do not introduce lowercase first-party folders or snake_case C++ source filenames in future layout proposals.

## Source ownership and declaration contracts

All SIREngine-owned C++ classes, structs, enumerations, templates, and free functions are in the **global namespace**. Do not introduce `namespace sir`, other named/anonymous namespaces, or `using namespace` directives. External `std::` and `eastl::` symbols keep their defined qualifiers.

Owned structures use `typedef struct sir[Name]_s { ... } sir[Name]_t;` with no `m_` prefix on struct fields. Enums use `typedef enum sir[Name]_e : uintN_t { SIR_VALUE = ... } sir[Name]_t;` with explicit fixed-width underlying integer types. Each enum value starts `SIR_`. Class fields retain the established Hungarian `m_` conventions.

Every SIREngine-owned header uses the 2023–2027 SIREngine MPLv2 notice, `__SIRENGINE_[HEADER_NAME]_H__` include guard, `#include <SIRPCH.hpp>`, conditional guarded `#pragma once`, and an annotated closing `#endif`. The canonical PCH file is `Build/CMake/SIRPCH.hpp`, published to owned targets through CMake include paths. Imported Godot-derived headers retain all original copyright/licensing attribution as well.

## Code and formatting

- C++17, compiler extensions disabled; pinned clang-format 23.1.5 and the repository's `.clang-format`.
- Four-column tabs, 140-column limit. Structural class/struct/enum/namespace/function opening braces on separate lines; braces always required on control flow.
- Reference ampersands bind to their types (`T&`, `T&&`); pointer stars retain right alignment (`T *p`). The canonical `.clang-format` explicitly sets `ReferenceAlignment: Left` and `PointerAlignment: Right`.
- In classes, order public/protected/private, then constructors/destructor, operators, getters/setters, and ordinary methods. Limit explicit function parameters to four and keep signatures on a single line where the 140-column limit permits.
- Zero-argument methods use `( void )`. Use `nullptr`, meaningful `constexpr`, and source-justified `noexcept`.
- Mandatory storage prefixes: `m_` instance fields, `s_` static storage, `g_` external globals. Semantic prefixes: `n` count, `i` index, `p` general pointer, `sz` fixed character array, `psz` C-string pointer (`char *` or `const char *`), `f` float, `b` boolean and `e` enum. Combine where applicable.
- Shared constants, when introduced, use approved uppercase `SNAKE_CASE` `constexpr` declarations in the corresponding owning headers. The removed experimental `SIRConstants.h` is not an API.
- Keep each method's cyclomatic complexity below 35 and condition checks at no more than three; use one material operation per source line and blank lines between major phases.
- File-private free functions explicitly say `static`; place ordinary definitions in translation units. Inline methods follow the owner's `ID_INLINE` policy after the macro is introduced in the real project.
- Prefer C runtime primitives for bytewise operations when object lifetimes and copy semantics permit; otherwise explicit indexed loops.
- Avoid elementary `std::` or `eastl::` algorithm wrappers, ranges-style pipelines and callback allocations in hot-path iteration. Meaningful sorting, binary search and heap primitives remain available.

## Interfaces, dependencies and review

Do not introduce speculative engine-wide container aliases or hidden allocator routing. The old experimental `SIRContainers.h` is removed. First-party systems may link pinned EASTL when a real consumer and its allocator policy exist.

Imported engine and framework code must preserve required ABI, source provenance and serialization contracts. Review source hashes, ownership and allocation lifetimes, test affected source translation units under the actual target config, and run exact-source formatting and compiler/link validation before acceptance.
