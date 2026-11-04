# 21 — SIR-Lang semantic-analysis foundation

**Stage:** initial symbol and scalar-type resolution over `SIRLangParse` syntax trees.
**Status:** implemented narrow foundation, not a full v1 type checker or executable compiler.

## Input and ownership

`SIRLangAnalyze(const sirLangSyntaxTree_t*, sirLangSemanticOutput_t*)` consumes a successful, borrowed source-preserving parse tree. The host supplies fixed-capacity arrays for declarations, resolved references, and diagnostics. No semantic heap or private symbol registry is introduced. The source must outlive the analysis and its token-index-based output. `SIR_LANG_SEMANTIC_CAPACITY` means the results are truncated and not suitable for compilation.

The analyzer validates token extents, root identity, child/sibling indexes, and bounded sibling chains. It refuses parse trees with active syntax diagnostics and reports semantic errors with original source offsets. Nested analysis has a hard recursion limit; malformed graph cycles cannot cause unbounded recursive execution.

## Current semantic checks

- Module and nested lexical declarations: schemas, behaviors, classes, functions, events, records, enums and interfaces, plus function parameters and stack-local `let` bindings.
- Duplicate symbol detection within a scope; local values become visible after their initializer, so use-before-declaration is diagnosed.
- Forward global declarations and independently predeclared schema fields, including condition-bank flags. Behaviors may appear before their state schemas.
- Builtin primitive type names, known concrete math/value types, generic-family names, user-declared nominal types, and host-registered external types.
- Basic exact-width scalar type compatibility for initialized locals, assignments, predicates and function returns. Literal integer suffixes and representable ranges are validated, including base-2 and base-16 integer forms.
- `self.Field` resolves against the declared schema. Conditions occupy logical bool fields; physical bit-bank placement belongs to a later schema/layout compiler.
- Records of source `iUseNode` to `iDeclarationNode`, or `UINT32_MAX` for registered host symbols/intrinsics, available to a future typed IR builder.
- Mutable top-level `let` is rejected. Only fixed-capacity caller-owned temporary buffers are used by the analyzer.

## Explicit remaining contracts

The current symbol output does **not** yet provide complete nominal type identity for comparing two distinct user record types, full generic-arity and constraint checking, numeric constant folding for unary minimum signed values, function overload resolution, intrinsic signature tables, full engine-capability/member types, control-flow return-completeness, definite assignment, alias/effect and borrow/lifetime proofs, typed HIR/SSA generation, or native code emission. In this stage, `SIR_LANG_VALUE_UNKNOWN` is a conservative unresolved expression type, not an implicit `any` type for executable code. The later compiler verifier must refuse artifacts containing unresolved types.

The host's `SIRScriptBindingRegistry` and planned schema registry must supply immutable callable/member descriptors, known effects, owner lanes, FieldIDs, and ABI revisions before the language's complete semantic check can be frozen. This module does not invent host implementations or introduce parallel registries.

## Integration

The implementation lives at `Source/Scripting/Language/SIRLangSemantic.h/.cpp` within the existing `SIRScripting` library. `Tests/Unit/SIRLangSemanticTests.cpp` is registered as `scripting.language.semantic` in the repository's `ScriptingTests.cmake`.

Focused local verification uses C++17, warnings-as-errors, ASan and UBSan, and clang-format 23.1.5. Fixture tests build valid parser-shape trees with the exact public node/token representation to test semantic outcomes independently of parser internals. A complete native CMake build, end-to-end parse-to-analysis integration, compiler-backed AST/style audit, Windows build, and final host binding ABI validation remain separate acceptance gates.
