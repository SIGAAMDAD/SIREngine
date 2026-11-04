# 20 — SIR-Lang compiler syntax foundation

**Status:** Implemented syntax-only frontend slice. This does not produce typed IR, bytecode, LLVM IR, script instances or executable programs.

## Source authority

- Branch: `feat/sirlang-v1-compiler-foundation`, derived from the scripting foundation merged with current `develop`.
- Shared scripting runtime, bindings, program caching and compiler-worker coordination remain under `Source/Scripting/Core` and `Worker`. The language frontend does not duplicate those owners.
- Source grammar remains provisional pending the v1 language syntax decision. The frontend accepts both documented `fn` and `function` spellings and uses the established indentation token stream.

## Implemented frontend API

`SIRLangParse(source, tree)` consumes a `sirLangSourceView_t` and a caller-supplied `sirLangSyntaxTree_t`. The caller owns bounded token, syntax-node and diagnostic arrays; no compilation allocations are performed inside parsing. A populated tree borrows source bytes and requires them to remain alive.

The token stream preserves byte-accurate offsets; source trivia is retained verbatim in the borrowed source. Syntax nodes form an index-linked tree and identify half-open token spans. `SIRLangSyntaxSpan` resolves byte spans, including the full unaltered source range for the module root. No pointers into container-owned transient allocations are persisted by the parser.

Grammar subset: imports, schemas and fields, condition banks, behaviors, classes, records, interfaces, enums, event handlers, functions, parameters, typed locals, indentation-based blocks, `when`/`otherwise`, `for`, `mutate`, `state`, assignments and reducer assignments, spawn/destroy, emit/after, transition, return/break/continue, function calls with named arguments, member/index access, parentheses, unary and binary operators, nested generic type syntax and stack fixed arrays.

Diagnostics retain source byte offsets, line/column and lexer error category. Parsing recovers at line boundaries; later independent declarations remain available after syntax failures. Token, node, diagnostic and recursive-nesting limits are explicitly bounded. Capacity exhaustion is a hard error, never silent truncation treated as success.

## Verification

`Tests/Unit/SIRLangParserTests.cpp` covers nested behavior/schema/event programs, numeric comparisons and operator precedence, generic types, explicit source retention (including comments and CRLF), line recovery, lexical errors, over-capacity buffers, maximum depth and 1,200 deterministic malformed inputs. Register the target under `Tests/Unit/ScriptingTests.cmake` and compile `Language/SIRLangParser.cpp` under `SIRScripting`.

This milestone's local acceptance is a strict C++17 compile of the lexer, parser and test with `-Wall -Wextra -Wpedantic -Werror`, passing AddressSanitizer/UndefinedBehaviorSanitizer execution, and project-pinned clang-format 23.1.5. Full SIREngine Windows/Linux CMake/CTest and ABI/style gates require an integrated checkout and remain separate acceptance gates.

## Next implementation slice

Define and freeze the v1 grammar manifest and syntax node revision, then implement semantic symbol/scope resolution and schema-aware type checking against an immutable scripting binding registry snapshot. Add negative tests for invalid types, unknown capabilities, frame-lifetime escapes and persistent state ownership. Do not introduce a competing runtime, world-state owner or script VM.
