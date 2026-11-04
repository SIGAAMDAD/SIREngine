# 23 - SIR-Lang P0 typed IR, type layouts, GraphVM and bytecode foundation

**Implementation boundary:** first executable scalar subset of the planned P0 compiler, on `feat/sirlang-v1-compiler-foundation` after function signature resolution. This is not a frozen general SIR-Lang v1 ABI or completed P0 language implementation.

## Implemented source and interfaces

- `SIRLangTypeSystem` provides caller-owned, bounded type descriptors, stable module/schema-anchored nominal identifiers, scalar and selected composite layout calculations, strict alignment/capacity/overflow validation and deterministic type identity.
- `SIRLangBuildHIR` stores resolved source declarations, function/capability identity and expression type records. `SIRLangBuildCIR` lowers a **supported scalar subset** of typed functions, locals, integer literals, arithmetic, calls, return expressions and two-way conditional branches with explicit SSA PHI joins. Unsupported syntax or types produce failure, not speculative code.
- `SIRLangAnalyzeEffects` provides conservative interprocedural native-call effect aggregation, deterministic-mode rejection, owner-lane checks, mutation/suspension conflict detection and borrowed-slice return rejection. These are initial checks, not a complete borrow checker, resource lifetime proof or whole-engine effects system.
- `SIRLangVerifyCIR` validates a bounded typed register program: operand/result types, typed calls, jump target ranges, single-definition registers, reachability, definite assignment along every predecessor path, and the current constrained PHI shape. GraphVM executes only programs accepted by this verifier.
- `SIRLangRunGraphVM` uses bounded frames/registers, caller-defined fuel, checked integer arithmetic, typed native calls and exact layout/binding digest comparisons. External scalar arguments/results must have canonical, width-valid bit patterns.
- `SIRLangEncodeGraphVM` and `SIRLangDecodeGraphVM` define a little-endian immutable v1 package with version, schema identity, exact layout and binding digests, explicit element counts, canonical zero padding, and SHA-256 integrity. The decoder compares native capability entries against host-supplied immutable descriptors and verifies the decoded graph before publication.

## Provisional binary format

Magic `SIRLBC01`; version `1`; fixed header **96 bytes**; function record **32 bytes**; instruction record **32 bytes**; capability record **40 bytes**; parameter type is one byte and argument register one LE32. The first 64 header bytes bind magic, version, header size, schema, layout digest, binding digest, counts and total length. Bytes 64-95 hold `SHA256(SHA256(header[0:64]) || SHA256(payload[96:]))`. This is a corruption digest and **not** an authentication signature. Every mutation requires verification before runtime use.

The native capability manifest includes FunctionID, SignatureID, BindingRevision, owner lane, effect flags, parameter signature offsets, arity and return type. The current C++17 GraphVM execution ABI uses scalar `sirLangValueKind_t` register values and a host typed callback, not native pointer bytecode.

## Tests performed and scope

Focused CMake + CTest builds use the real P0 source files, the checked-in semantic/signature implementations, and caller-owned test fixtures. Five focused executable test suites pass with C++17 `-Wall -Wextra -Wpedantic -Werror`, AddressSanitizer, and UndefinedBehaviorSanitizer. Tests exercise nominal/composite layouts, forward calls and HIR-to-CIR-to-VM, SSA PHI joins, verifier rejection and runtime bounds, serialized bytecode round-trips and corruption rejection, effects/determinism/lanes, and invalid externally supplied narrow scalar values.

**Local harness limitations:** Full engine source is not checked out; the focused harness uses a minimal local PCH/parser-tree declaration stub and an OpenSSL-backed **test-only SHA-256 shim** with the same `SIRScriptSHA256` signature as the existing real engine implementation. Neither test shim is part of this repository patch. Tests do not demonstrate engine CMake linkage, real parser-to-emitter integration, exact executable runtime performance, Windows compiler acceptance, or source-based 100% coverage. The repository's `Source/Scripting/CMakeLists.txt` must link the real `SIRScriptSHA256.cpp` already present in `SIRScripting`.

## Remaining P0 completion gates

1. Freeze and validate full lexical/grammar and diagnostic ABI, numeric overflow/conversion semantics, generic/nominal schema and field identities, persistent layout revisions, type constraints, and standard intrinsic signatures.
2. Complete type checking and HIR lowering of all approved language constructs: composite values, methods/overloads, generics, loops, records, variants, all built-ins, and control-flow return completeness.
3. Lower the full event/state/transaction language with ordered effects, mutable borrows, validated suspension, durable operation handling, entity state and schema effects. Add full alias/lifetime and capability proofs.
4. Complete a canonical CFG/SSA IR with general loops, multiple predecessors, verified effect ordering, dominance, phi placement and deterministic machine-independent semantics. Current PHI format intentionally supports a restricted two-way join.
5. Add a real source-file-to-GraphVM end-to-end acceptance target with the repository lexer/parser, and connect the compiled program to the existing engine scheduler/runtime and code manager while preserving ownership.
6. Perform full CMake/CTest compilation, Windows/Linux CI matrices, compiler-derived AST/style gates, sanitizer and fuzzing passes, corrupt-bytecode mutation tests, real engine SHA-256 interoperation, and 100% reachable line/branch acceptance.

No part of this document promotes the prototype v1 bytecode as a stable release ABI. Incompatible contract revisions must increment `SIR_LANG_VM_VERSION` and provide explicit format rejection/migration handling.
