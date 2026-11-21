# Director operating procedure

## Task intake

Record task ID and requirements; current source branch/SHA; dirty files; explicit branch/commit/push/issue permissions; affected systems; acceptance tests; external dependencies.

### Work modes

| Request | Expected mode |
| --- | --- |
| Explain, inspect, audit | Read only; return source-grounded findings. |
| Plan, design, split | No source edits; produce contracts, negative tests and ownership plan. |
| Implement, fix, complete | Apply authorized source changes and test; don't stop at a plan. |
| Port | Preserve external behavior and origin/provenance; separate redesign. |
| Integrate | Verify exact patches/SHAs and reconcile into one source baseline. |
| Benchmark | Measure actual baseline/candidate with matched settings. |

## Delegation threshold

Use the main agent for small direct changes. Delegate when investigations are broad, independent review improves correctness, or disjoint code work genuinely permits parallelism. Delegation is not a performance optimization if coordination or rework exceeds the saved time.

## Native subagents

Use `SIR Director` with the `agent` alias in Copilot custom-agent frontmatter; the runtime uses native `task`. Each worker has separate conversational context, not automatically separate filesystem ownership. Review actual tool activity. Never assume subagents launched because the director described a plan.

## Evidence gates

- Source/read audit: actual path and symbol references.
- Modified code: diff and changed owned files only.
- Formatting: specific formatter version and command.
- Compiler: actual target/compiler, `CMakePresets.json` and compile exit.
- Tests: actual CTest/test binary command, tests run and failures.
- Hardware: driver/API/OS GPU proof only when actually available.
- GitHub: final issue/PR/commit URL and state readback only after authorized mutation.

See [Delegation](Delegation.md), [Verification](Verification.md) and [Receipts](Receipts.md).
