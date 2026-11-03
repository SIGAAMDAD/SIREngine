# SIREngine-owned C++17 standard

**Scope:** first-party `Engine/Source/`, `Tests/` and future `Tools/` C++ sources. Externally licensed libraries under `ThirdParty/` retain their own API and formatting conventions.

## Code and formatting

- C++17, compiler extensions disabled; pinned clang-format 22.1.5 and the repository's `.clang-format`.
- Four-column tabs, 140-column limit. Structural class/struct/enum/namespace/function opening braces on separate lines; braces always required on control flow.
- Zero-argument methods use `( void )`. Use `nullptr`, meaningful `constexpr`, and source-justified `noexcept`.
- Mandatory storage prefixes: `m_` instance fields, `s_` static storage, `g_` external globals. Semantic prefixes: `n` count, `i` index, `p` pointer, `sz` fixed array or C string, `psz` dynamically allocated array, `f` float, `b` boolean and `e` enum. Combine where applicable.
- Shared constants, when introduced, use approved uppercase `SNAKE_CASE` `constexpr` declarations in the corresponding owning headers. The removed experimental `SIRConstants.h` is not an API.
- Keep each method's cyclomatic complexity below 35 and condition checks at no more than three; use one material operation per source line and blank lines between major phases.
- File-private free functions explicitly say `static`; place ordinary definitions in translation units. Inline methods follow the owner's `ID_INLINE` policy after the macro is introduced in the real project.
- Prefer C runtime primitives for bytewise operations when object lifetimes and copy semantics permit; otherwise explicit indexed loops.
- Avoid elementary `std::` or `eastl::` algorithm wrappers, ranges-style pipelines and callback allocations in hot-path iteration. Meaningful sorting, binary search and heap primitives remain available.

## Interfaces, dependencies and review

Do not introduce speculative engine-wide container aliases or hidden allocator routing. The old experimental `SIRContainers.h` is removed. First-party systems may link pinned EASTL when a real consumer and its allocator policy exist.

Imported engine and framework code must preserve required ABI, source provenance and serialization contracts. Review source hashes, ownership and allocation lifetimes, test affected source translation units under the actual target config, and run exact-source formatting and compiler/link validation before acceptance.
