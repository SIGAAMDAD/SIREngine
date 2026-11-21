# SIREngine VS Code agent suite

A repository-level, additive configuration for the **VS Code Copilot harness**. This package is delivered as a folder overlay; it does not change source, build presets, `.clang-format`, `.clangd`, `.clang-tidy`, `.editorconfig`, Git branches or GitHub issues.

## Files

- `AGENTS.md` - short shared policy, authority, task boundaries, build/verification rules.
- `.github/instructions/*.instructions.md` - scoped engineering standards by language/subsystem.
- `.github/agents/*.agent.md` - director and restricted specialists; the director can delegate using the Copilot `agent` alias, mapped to the native `task` mechanism.
- `.github/skills/*/SKILL.md` - explicit reusable slash workflows; expensive read-only ones use `context: fork` (experimental).
- `Docs/Agents/` - operation contracts, receipts, model routing, install/test checklist and current standard delta.
- `Tools/AgentConfig/validate_suite.py` - static configuration integrity checks (also includes negative fixtures).

## Start

1. Inspect [Installation](Installation.md), compare with any local rules and copy the overlay into the SIREngine root.
2. Open VS Code with the **Copilot harness** and run `Chat: Open Customizations`; verify agents, instructions and skills are discovered.
3. Enable `github.copilot.chat.skillTool.enabled` only if using forked skills.
4. Select **SIR Director**, choose an available model, and issue a concise task, e.g. `Complete issue #39; delegate investigation and independent verification.`
5. Watch the actual subagent activity, source diff and build/test receipts. A textual promise of delegation is not proof of a spawned subagent.

The suite intentionally does **not** hardcode external model IDs or a guessed `mcp.json` server command. See [Model routing](Models.md) and [Context cache](ContextCache.md).

## Design

Global policy remains small. Scoped instruction files activate only for matching paths; skill bodies are loaded when invoked. Model selection occurs in VS Code using actual provider-qualified runtime IDs, not unverified catalog IDs.

Source inspection was based on public `SIGAAMDAD/SIREngine` `main` as observed on 2026-11-20. See [Policy delta](PolicyDelta.md) for newer policy vs currently committed C++17 configuration. This archive does not certify any local checkout or model-dependent VS Code execution.
