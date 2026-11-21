# SIREngine-owned C++ standard

**Authority:** owner-approved rules consolidated November 20, 2026. Applies to first-party `Source/`, `Tests/`, and `Tools/` C++ only. Vendor code under `ThirdParty/` retains its own requirements. Existing architecture and allocation contracts continue unless explicitly revised here.

## 1. Language and compatibility

- **C++23 is primary.** Build in C++17 only as the compatibility fallback when the compiler/toolchain does not support the required C++23 mode. Feature-specific facilities must also have C++17-compatible implementations when used in portable engine code.
- Disable non-required compiler language extensions. Pin formatting to clang-format 23.1.5.
- C++17 fallback selection uses language/library feature checks, not assumed library availability from the language mode. The C++17 fallback must be compiled and tested for affected code.
- Approved with C++17 paths: `std::bit_cast`, `std::byteswap`, `std::endian`, `std::span`, `std::expected`, `std::is_constant_evaluated`, `constexpr`, `constinit`, `if constexpr`, `if consteval`, `std::to_underlying`, `std::source_location`, concepts, and `requires`.
- `consteval` is allowed. C++23-only features and newer `constexpr` capabilities must have an appropriate C++17 path where used in portable code.
- Language/toolchain portability is the requirement; new language facilities are optional, not preferred for their own sake.
- `std::string_view` is forbidden; use the existing SIREngine string-view implementation.

See [C++23 compatibility](cpp23-compatibility.md) for fallback and compilation rules.

## 2. Files, names, namespaces, types

- First-party directories, C++ headers, and source files use PascalCase; engine-owned class names use `SIR` where established.
- First-party classes, structs, enums, templates, and other declarations are in the **global namespace**. Named and anonymous namespaces are forbidden. Empty namespaces and `using namespace` are forbidden.
- Class instance fields use `m_`, static fields use `s_`; structs **do not** use `m_`.
- Semantic prefixes remain `n` (count/quantity), `i` (index), `p` (pointer), `sz` (fixed character array), `psz` (C string pointer), `f` (float), `b` (Boolean), and `e` (enumeration). Combine with storage prefixes when needed.
- Owned POD structures retain the established declaration pattern `typedef struct sir[Name]_s { ... } sir[Name]_t;` with no second alias. Struct fields have no `m_` prefix.
- Enumerations use `enum class sir[EnumName] : uintN_t` with an explicit fixed-width underlying integer type; enumerators use the `SIR_` prefix. Do not produce typedef aliases for enums.
- `using Name = ...` type aliases and `typedef ExistingType NewName` aliases are forbidden. This includes aliases that merely rename generated struct types.
- Engine-defined integer storage and arithmetic use fixed-width integer types (`uint8_t`, `int32_t`, `uint64_t`, and so on). Plain `int`, `short`, `long`, and other primitive integer declarations are forbidden in first-party definitions. Exact externally required ABI and language entry-point signatures are retained.
- `std::size_t` and other `std::` replacements for established C integral types are forbidden. Use fixed-width integer types for engine-defined quantities and `size_t` only where a size-oriented interface requires it.
- `double` is prohibited without a documented precision requirement, such as large-world coordinates.
- Prefer `const` for every declaration, parameter, method, and view that does not require mutation.
- Never use `auto` or range-based `for`; use explicit types and indexed loops.

## 3. Class and struct declarations

- Class sections appear `public:`, then `protected:` (only when needed), then `private:`. Never insert empty access sections.
- Group **all constructors and the destructor together**, without other declarations between them. Then place assignment/other operators, getters/setters and validity methods, other methods, then data members within their access sections.
- **Every constructor is `explicit`**, including default, copy, and move constructors. Use direct initialization; no copy-initialization assumption for explicit copy/move constructors.
- Every class explicitly declares its copy constructor, move constructor, copy assignment, and move assignment, each `= delete`, `= default`, or manually implemented. Destructors are declared according to actual ownership. This rule applies to **classes**, not to POD/aggregate structs.
- Destructors use empty `()` rather than `( void )`. Other zero-argument methods use `( void )`.
- No ordinary function bodies inside a class declaration. `= delete` and `= default` are allowed. `constexpr` and `consteval` functions are permitted exceptions.
- **Explicit `inline` is forbidden**. Methods may be force-inlined only through the project-approved force-inline attribute/macro, with definitions outside the class declaration except for permitted `constexpr`/`consteval` bodies.
- Structs primarily hold data. Small utility getters/setters, validity checks, comparisons, and trivial data transformations are permitted, as are operators. Struct utility functions follow the same out-of-body definition rule except `constexpr`/`consteval` and `= delete`/`= default`.
- Use `[[nodiscard]]` for failure-bearing results, acquired handles, and ownership transfer; `explicit` for constructors, `override` for overriding virtual functions, `final` for classes that are intentionally closed, and `noexcept` for destructors and operations that guarantee it.
- Function signatures have at most **four explicit parameters**; cyclomatic complexity must remain below 35; no compound condition contains more than three logical checks. Braces are mandatory around control-flow bodies.
- Never use `const_cast`. C-style casts are banned. Prefer `static_cast` for conversions; `reinterpret_cast` only for documented low-level memory or ABI operations. Basic compiler RTTI (`typeid`, `dynamic_cast`) is allowed.
- Use `nullptr`; implicit narrowing is forbidden. Prefer checks against an explicit invalid-value sentinel or a small `IsValid()` method for resource input validity.

### Class layout example

```cpp
class SIRResource final
{
public:
    explicit SIRResource() noexcept;
    explicit SIRResource( const SIRResource& ) = delete;
    explicit SIRResource( SIRResource&& ) noexcept = default;
    ~SIRResource() noexcept;

    SIRResource& operator=( const SIRResource& ) = delete;
    SIRResource& operator=( SIRResource&& ) noexcept = default;

    [[nodiscard]] bool IsValid( void ) const noexcept;
    void Reset( void ) noexcept;

private:
    uint32_t m_nResourceId{};
};
```

## 4. Headers and compilation units

- Owned header guards are exactly `__SIR_[CLASS_NAME]_H__` where `[CLASS_NAME]` is the uppercased class/header name with the leading `SIR` prefix removed (for example, `SIRResource.h` -> `__SIR_RESOURCE_H__`). Retain the established conditional `#pragma once` and annotated closing guard.
- The required PCH inclusion for migrated headers is **`#include <Core/SIRPCH.h>`**. Retain existing licensing text and the SIREngine 2023–2027 MPLv2 notice.
- Keep ordinary non-template and non-`constexpr` definitions in `.cpp` files. Definitions required for template instantiation and permitted compile-time functions may live outside class bodies in headers.
- Prefer explicit template instantiation in `.cpp` files when appropriate; templates exposed to multiple translation units must make the necessary definitions available.
- Template declarations use descriptive prefixed parameters, e.g. `template<typename TValue, uint32_t nCount, float fAmount>`.
- Public headers must be self-contained in their declared build configuration. Keep each source file in the owning subsystem.
- Standalone free functions are forbidden unless documented: required language entry points, third-party/OS callbacks, non-member operators, and compulsory ABI interfaces are preapproved categories. Other free functions require an explicit owner justification; otherwise use a class method.
- Global/free-floating variables are forbidden. Shared constants belong to approved owning types as `static constexpr` members. Explicit `inline` variables are forbidden; `constinit` may be used for appropriately owned static storage.
- Keep external-library symbols qualified with `std::` or `eastl::` as appropriate; never introduce `using namespace`.

## 5. Containers and algorithms

- Prefer **specialized algorithm- and workload-specific containers** over generic containers when the workload justifies one. Document container allocation, performance, ownership and invalidation behavior.
- SIREngine-owned containers and EASTL are the preferred data-structure/library facilities. Prefer EASTL algorithms over STL equivalents. Standard-library traversal/copy/transform/fill/move algorithms are prohibited where EASTL equivalents are available.
- `std::copy`, `std::fill`, and `std::move` are not used instead of approved EASTL equivalents. For trivial bytewise copying and fill operations, prefer `memcpy`, `memmove`, and `memset` as appropriate.
- For complex/nontrivial objects, use typed copy/move/fill operations with proper construction and destruction semantics.
- Prefer `Type value{};` over `Type value = {};` or bytewise clearing during initial object creation. Use `memset` for eligible byte-oriented/simple representations requiring explicit clearing.
- Bytewise copy/move of typed objects must be valid for their object representation (notably trivially copyable types). Never use bytewise operations to bypass nontrivial construction, assignment, or destruction.
- Prefer SoA to AoS for homogeneous record collections, especially where hot-path access patterns benefit. Ordinary ownership-bearing objects retain their correct object layouts.
- Persisted Boolean **fields** are packed: use `bool bEnabled : 1` or explicit dedicated integer masks. Eight-bit stored Boolean fields are forbidden. Normal Boolean expressions, parameters, return values, and local temporaries remain permitted. Use fixed-width mask storage for binary/ABI-visible representations.
- Avoid abstractions, ranges/pipelines, or callbacks that offer no substantive benefit over clear indexed operations.

## 6. Ownership, allocators, pointers

- **Override the global C++ `operator new` and `operator delete` family** and route engine allocations through the established SIREngine allocator/memory-routing infrastructure. Support the applicable scalar/array, aligned, sized, and nothrow signatures, with paired release semantics, bootstrap safety, explicit failure behavior, and allocation accounting.
- Ordinary `new`/`delete` expressions are permitted; the engine's overridden operators control their allocation/deallocation paths. Constructor invocation and object lifetime must remain correct.
- `malloc`, `calloc`, `realloc`, and `free` are forbidden in first-party code. Approved third-party/OS boundaries use explicit allocator adapters.
- Prefer internal SIREngine smart pointers over raw owning pointers. `std::unique_ptr`, `std::shared_ptr`, and `std::weak_ptr` are forbidden.
- Document unique/shared/weak ownership, borrowing, nullability, reference-counting behavior and allocator domain for each internal smart-pointer category.
- For raw storage, construct and destroy objects explicitly, respecting alignment and lifetime. Declare iterator, handle, view and pointer invalidation rules.
- Respect GRDG canonical resource ownership, reader epochs, job completion and GPU/audio retirement fences.

## 7. Threading, errors, diagnostics

- `std::atomic<T>` is the approved atomic type. Document memory ordering and use acquire/release only when synchronization requires it; relaxed ordering is appropriate only when no cross-data ordering is needed.
- `std::thread`, `std::mutex`, `std::lock_guard`, and `std::condition_variable` are strictly forbidden in first-party engine code. Use the appropriate existing engine threading/synchronization facilities.
- Define lock acquisition order and thread ownership; do not introduce data races or unbounded lock lifetime.
- Exceptions are forbidden. Recoverable failures use explicit non-throwing result/status contracts; assertions are allowed for invariants. Failures from allocation and construction must have defined behavior.
- Lambdas and `std::function` are forbidden. Callback representations must have explicit ownership and lifetime contracts.
- Use `[[nodiscard]]` to enforce relevant results. Respect ABI, endianness, packed layout and synchronization correctness at module boundaries.

## 8. Platforms and validation

- **Platform-specific implementation code belongs only in `Source/Core/Platform/`.**
- The portable engine must support its documented operating systems and graphics driver levels through existing backend capability contracts.
- Apply `.clang-format`, `.editorconfig`, `.clangd`, `.clang-tidy`, and repository policy checks to first-party owned code. Structural rules not expressible in stock clang-format/clang-tidy require AST-aware policy checks.
- Enforce current compilation, tests, sanitizer, coverage, source ownership, and formatting requirements from the architecture and qualification documentation. C++17 compatibility fallback code must be actually compiled/tested.
- Changing the **standards** does not claim an immediate whole-tree source migration. An implementation is compliant only after the corresponding source and toolchain gates pass.
