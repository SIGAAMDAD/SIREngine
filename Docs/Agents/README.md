# SIREngine agent suite — optimized edition

**Start with SIR Director** in the VS Code Copilot harness. Ask a compact task such as `Complete issue #39; delegate driver work and verification.` All shared rules are in `AGENTS.md`; source-scoped rules are in `.github/instructions/`; task workflows are under `.github/skills/`.

- [Roles and ownership](Roles.md): requested backend programmers, systems programmers and creative design specialists.
- [Improvement loop](ImprovementLoop.md): every programmer implements, tests, then researches/measures one bounded enhancement.
- [Delegation contract](Delegation.md): exact file ownership and source baseline for subagents.
- [Verification](Verification.md): real compile, tests, sanitizers and benchmark receipts.
- [ContextCache](ContextCache.md): source/corpus retrieval; only inject relevant evidence and check exact source revision.
- [Models](Models.md): choose offered models at runtime; no hardcoded unavailable identifiers.
- [Policy differences](PolicyDelta.md): checked-in language standard and any future target policy require explicit reconciliation.
- [Install](Installation.md): copy the overlay, inspect existing local files and run validator.

Design: one short shared contract, a small number of file-matched instructions, distinct specialist prompts, reusable skills and measured acceptance gates. No per-specialist duplicate skills or automatic open-ended agent loops.
