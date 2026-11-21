---
name: SIR types and interface layout
description: Apply struct, enum, class, header and public API conventions in owned C++.
applyTo: 'Source/**/*.{h,hpp},Tests/**/*.{h,hpp},Tools/**/*.{h,hpp}'
---
# Owned C++ type and API policy

- C-style engine-owned structs and enums use the canonical `sir[Name]_t` public name. Preserve established `typedef struct sir[Name]_s { ... } sir[Name]_t` and `typedef enum sir[Name]_e : uintN_t { SIR_VALUE = ... } sir[Name]_t` forms where they are already project policy; avoid secondary typedefs/`using` aliases. New `enum class` use requires an explicitly approved exception, not a stealth style change.
- Structs represent data and limited local utility/operators, do not prefix struct fields `m_`; class members use `m_` for instance storage.
- Use an explicit fixed-width underlying integer type for serialized enums, stable values, and the agreed engine prefix for enumerators.
- No namespaces or `using namespace` in first-party owned headers. Preserve C ABIs and required third-party qualifiers.
- Keep inheritance and RTTI behavior consistent with the owning subsystem; basic RTTI where approved, no new type-erasure infrastructure by default.
- No public ABI, vtable layout, object layout, serialization schema, or exported symbol changes without explicit scope and dependent consumer audit.
- Headers include only their actual dependencies; preserve existing project PCH/compiler/platform headers as found in the checkout. Do not fabricate a new canonical `SIRPCH` path during unrelated work.
- Validate public headers in actual consumer translation units and applicable compiler variants after edits.
