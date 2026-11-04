# 22 — SIR-Lang typed function signatures and call resolution

**Implementation stage:** bounded compile-time signature collection and call validation. This module is a semantic-analysis successor, not an executable compiler, LLVM frontend, native ABI implementation, or general-purpose HIR yet.

## Entry point and ownership

`SIRLangResolveSignatures(tree, semantic, output)` runs after successful `SIRLangAnalyze`. Its inputs are borrowed syntax, tokens, source bytes, and semantic symbols/references. The caller owns all signature, parameter, call, per-node type, and diagnostic buffers. The pass does not allocate heap memory, register runtime instances, mutate engine state, or invoke native bindings.

The pass collects every declared source function before resolving expressions, so a function may call a later function. Every signature records the declaration node, parameter-name tokens, ordered parameter types, return type, and deterministic source-local signature fingerprint. Every resolved call records the source node and either a source declaration or a host FunctionID/SignatureID. The `pNodeTypes` output is indexed by syntax-node ID for subsequent typed-HIR lowering.

## Type identity and call rules

Scalar identity uses the existing explicit-width `sirLangValueKind_t`. Composite and nominal values retain their source type node and a canonical token-spelling fingerprint. Two composite types compare equal only when their fingerprints match *and* their canonical token spellings match; the hash is an accelerator, not proof. Generic type arguments such as `option<u16>` and `option<u32>` remain distinct. Type-name aliases and cross-module stable `SchemaID` identity are not asserted by this provisional representation.

The pass checks positional arity, explicit named arguments, unknown and duplicate parameter names, positional arguments after named arguments, argument types, assignments, and return expressions. It also checks the return type of a call used as a condition. Nested calls are resolved recursively with a strict depth budget; calls to later declarations work because all source signatures are collected before the walk. Source functions are limited to 32 parameters in this first implementation. More formal parameters produce an explicit compile diagnostic, not an overrun.

Calls to registered external functions require an immutable typed signature supplied by the host, including FunctionID and SignatureID, a return type, and ordered parameter name/type descriptors. In this stage native signature values are restricted to scalar built-in kinds; opaque/unknown native calls remain diagnosed as untyped. Host signature validation rejects null names, invalid IDs, absent parameter buffers, duplicated names, void-valued parameters, unsupported composite native signatures, and conflicting external names.

## Integration and limitations

The preceding semantic pass still owns declaration/scope validity, primitive type legality, integer-literal overflow and schema-reference checking. The new pass does not replace or silently widen that behavior. It adds the call signatures and inferred call results that were previously missing. The existing `SIRScriptBindingRegistry` native descriptors do not yet expose the full typed parameter list/effect metadata; a future immutable binding-schema snapshot must supply the external signature data from one authoritative owner.

Member-call method lookup, polymorphism, higher-order calls, overload selection, generic monomorphization, conversions, method effect analysis, durable type IDs, and LLVM/GraphVM emission are intentionally pending. An unsupported native or member call is reported rather than assumed safe. SIR-Lang syntax and the resulting type/call records remain provisional until the language and host ABI are frozen.

## Local validation evidence

The focused native test executable compiled with `clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined` and executed successfully. The same suite executed with GCC `--coverage`. The selected `SIRLangSignatures.cpp` translation unit measured **90.77% line coverage** (390 instrumented lines) and **72.55% branches taken** (419 branches); **97.61%** of branches had their condition evaluated. These figures are scoped to the new resolver, not the whole engine or the combined lexer/parser/semantic compiler. Its assertions use caller-constructed valid parser-tree fixtures; integrated native CMake/CTest and editor/runtime execution are not asserted by this local fixture run.

The source files were formatted and checked using the local pinned `clang-format 23.1.5`. Full repository CMake/CTest on Windows and Linux remains an integration acceptance gate.

## Next compiler boundary

Produce typed HIR from the validated syntax tree, semantic references, source function signatures and `pNodeTypes` without duplicating symbol binding or world-state ownership. Bind native effects and stable schema/layout identities through the future immutable host registry snapshot, then lower into verified control-flow/SSA IR and the baseline GraphVM bytecode.
