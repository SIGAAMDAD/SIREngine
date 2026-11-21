# C++23 primary / C++17 fallback contract

C++23 is the preferred SIREngine language mode. C++17 is selected only when a supported toolchain cannot provide C++23 or when explicitly verifying compatibility. An available C++23 syntax mode does not by itself guarantee that every C++23 standard-library facility is implemented.

## Selection

1. Configure `SIR_CXX_STANDARD=AUTO` (default) to choose C++23 if the compiler advertises `cxx_std_23`, otherwise C++17.
2. Use `SIR_CXX_STANDARD=17` to exercise fallback paths in qualification builds, or `=23` to require C++23 explicitly.
3. For individual features, use the appropriate `__cpp_*` and `__cpp_lib_*` feature-test macros (and verified implementation support). Never assume the complete standard library from a language version flag.
4. Do not use the `using Alias = Type` syntax to provide compatibility shims. Keep real SIREngine-owned compatibility classes/functions, or provide guarded source alternatives.
5. Fallback paths must preserve the documented observable behavior. When C++17 cannot reproduce language-level guarantees, use explicitly separate code paths and tests.

## Required facility mapping

| Modern facility | C++17-compatible mechanism |
| --- | --- |
| `std::bit_cast` | Bytewise `memcpy` for eligible trivially copyable objects with equal sizes; compile-time usage must have its own implementation |
| `std::byteswap` | Byte-swap intrinsics or explicit bit operations |
| `std::endian` | Verified platform/build byte-order definitions |
| `std::span` | SIREngine pointer-and-length view |
| `std::expected` | SIREngine result/error class |
| `std::is_constant_evaluated` | Supported compiler builtin or a separate C++17 code path |
| `constinit` | Static constant-initialization contracts verified under C++17 |
| `consteval` | `constexpr` plus explicitly tested compile-time call sites where possible |
| `if consteval` | C++17-compatible template or conditional-compilation code paths |
| `std::to_underlying` | `static_cast` to `std::underlying_type_t<T>` where applicable (no new type aliases) |
| `std::source_location` | Explicit file/function/line capture, or compiler builtins where available |
| `requires` / concepts | C++17 traits and substitution constraints, or alternate overloads |
| `if constexpr` / base `constexpr` | Directly available in C++17; newer constexpr operations require feature checks |

No general-purpose `std::string_view` fallback is needed: the existing engine string-view implementation is mandatory.

## Verification

- Compile normal toolchains in primary C++23 mode when available.
- Run a C++17 fallback compilation/qualification target for affected portable components.
- Test both paths for equivalent results, ownership, allocation routing, and failure behavior.
- Compiler- or platform-specific facilities must not leak into the portable public ABI.
