# Install and smoke test

1. Back up existing `AGENTS.md`, `.github/agents/`, `.github/skills/` and `.github/instructions/` if present. Extract the suite overlay into the SIREngine repository root; inspect collisions before overwriting local customizations.
2. Open the VS Code **Copilot** harness and run **Chat: Open Customizations**. Confirm SIR Director and the specialist subagents appear. Specialists are intentionally hidden from the main picker.
3. Set the active available model for the Director; specialized agents inherit the session model unless explicitly configured later. Configure ContextCache MCP separately using its verified server name; allow read-only tool namespaces explicitly if you use per-agent tool restrictions.
4. Run `python Tools/AgentConfig/validate_suite.py --root .` then `python Tools/AgentConfig/validate_suite.py --self-test`.
5. Smoke test `SIR Director: Ask SIR D3D11 Programmer to inspect the driver, read-only. Report files and findings.` Observe actual subagent tool calls; do not infer success from narrative.
6. For a real code task, verify the specialist produces source diffs, test receipts and a bounded post-acceptance improvement outcome.

No project source, `.clang-format`, `.editorconfig`, `.clangd`, CMake presets, local `.vscode/mcp.json`, model IDs or Git branches are changed by this overlay.
