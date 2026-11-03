# SIREngine-owned C++17 standard

**Scope:** `include/sir/`, `src/`, `tests/` and SIR-created C++ integration code. Inherited Godot and third-party code retains its own source formatting unless a separately approved migration changes it.

## Structure and formatting

- C++17, no compiler-specific extensions. Four-column tab indentation, 140-column maximum; exact formatter **clang-format 22.1.5** with root `.clang-format`.
- Opening braces on a new line for class/struct/union/enum/namespace and function/method definitions; control-statement braces stay attached. Every `if`, `else`, `for`, `while`, `switch`, `do`, `catch` body has braces.
- Zero-parameter methods spell `( void )`. Use `nullptr`, `constexpr` for compile-time constants, and `noexcept` only after verifying the actual called code is nonthrowing.
- One major operation per line; blank line between distinct operational segments. Cyclomatic complexity <35; no more than three boolean checks in any condition.
- Instance members `m_`, static storage `s_`, external globals `g_`. Semantic Hungarian prefixes: `n` counts, `i` indices, `p` pointers, `sz` fixed arrays/C strings, `psz` runtime arrays, `f` floating numbers, `b` booleans, `e` enum typed values. Combine storage and semantic prefixes where applicable.
- Shared constants use uppercase `SNAKE_CASE` `constexpr` variables in `include/sir/core/SIRConstants.h` or a formally approved subsystem constants header, with no `g_` prefix.
- External ABI signatures are exempt from renaming only when required by a real Godot or SDK interface, with source evidence.

## C++ implementation design

- Put ordinary non-inline method bodies in `.cpp`, not within class definitions. Only intentionally inlined header methods use `ID_INLINE` declared in the class and defined out-of-class, after the project establishes the macro in a shared header.
- Use explicit `static` on file-private free functions; no anonymous namespaces.
- Prefer C byte/string primitives (`memcpy`, `memmove`, `memset`, etc.) when semantically safe for trivial objects; use explicit typed loops for nontrivial types.
- Do not use elementary `std::`/`eastl::` algorithm wrappers for iteration, fill/copy, linear search, count, transformation or filtering; no ranges/views/execution pipelines or function-wrapper callbacks on elementary hot traversals. Nontrivial sorting/heap/binary-search algorithms remain permitted.
- All owned allocation must eventually use a traced SIR/NMD allocator domain; no unaccounted STL/EASTL heap fallback in accepted shipping code. Never claim a default EASTL alias alone satisfies this.
- Preserve upstream API, ABI, editor binding and serialized version behavior in every patch; use a real compilation database and Clang AST for semantic renames.

## Enforcement

Run the codebase-patcher inventory/closure, formatter and source-bound Nomad-style AST profile on modified SIR C++; perform source-hashed manual review of exception safety, naming intent, memory semantics and operation grouping. Compile and test every dependency-closed patch. The root formatter is not applied to unmodified `engine/godot` or `third_party` code; use each upstream checkout's pinned formatter there.
