# Model assignment (runtime-selected)

VS Code Copilot model identifiers are harness/provider qualified and can differ from NVIDIA catalog API names. Select actual offered identifiers using the agent editor's `model` completion or picker. This suite intentionally omits fixed `model` fields so an unavailable provider does not prevent agent startup.

| Worker | Good initial candidate | Why test it | What must be measured |
| --- | --- | --- | --- |
| SIR Director | Step 5 Preview | Strong general-reasoning and fast published inference results | Plan correctness, handoff cost, actual tool calls |
| SIR Implementer | MiniMax M3.1 Flash | Relatively strong published software-agent and refactor benchmarks | Issue acceptance time, tests passed, regressions/rework |
| SIR Verifier | A different high-capability model from the implementer | Independent failure patterns | False-negative defects, test quality |
| SIR Researcher | Nemotron 3.5 Super VL / smaller available model | High throughput for bounded search; multimodal input when needed | Evidence accuracy, latency, context usage |
| SIR Architect | MiniMax M3.1 Flash or Step 5 Preview | More demanding reasoning and multi-step dependency analysis | Source-grounding, cross-worker contracts |
| SIR Build Engineer | Fast tool-capable model | Tool output and deterministic commands matter more than prose | Correct failures triaged per minute |

These are initial choices, not guaranteed rankings or runtime model IDs. Verify model/tool access and published benchmarks against current endpoints before imposing a model policy. Kimi is excluded because it is unavailable to the user.

Avoid using the Artificial Analysis output-token rate as a substitute for tested end-to-end implementation speed. Measure a representative SIREngine issue on identical source baseline, tools and acceptance gates.
