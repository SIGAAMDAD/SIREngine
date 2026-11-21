# Installation and smoke tests

## Apply safely

1. From the actual SIREngine root, save local edits and inspect `git status --short`.
2. Open the ZIP and review paths. **Merge**, do not blindly overwrite, if `AGENTS.md`, `.github/agents`, `.github/instructions` or `.github/skills` already contain local files.
3. Keep existing `.clang-format`, `.editorconfig`, `.clangd`, `.clang-tidy`, `.vscode/settings.json`, `.vscode/mcp.json`, CMake files and Git history intact. This suite deliberately ships no replacements.
4. Run `python Tools/AgentConfig/validate_suite.py --root .` from the SIREngine repository root. It checks package structure, frontmatter, distinct names, references and forbidden Local-harness configuration.
5. In VS Code select the Copilot harness, run **Chat: Open Customizations**, inspect instruction files, custom agents and skill visibility. Start a new chat to pick up changes.

## Agent checks

1. Select **SIR Researcher** through a parent agent delegation: ask `Use SIR Researcher to trace a small existing class. Do not edit. Return files and invariants.` Confirm an actual subagent was invoked and did not edit.
2. Select **SIR Director** and ask it to delegate **SIR Verifier** to review a known diff. Confirm the result came back to the director.
3. If enabling `context: fork` skills, enable `github.copilot.chat.skillTool.enabled`, restart the session, invoke `/sir-investigate` and inspect the isolated result.
4. Verify that a scoped instruction loads for a C++ source file and not for an unrelated Markdown file.
5. Select actual runtime model IDs from model dropdown/completion, not API catalog marketing names. Test tool calling and delegation with each candidate.
6. Validate read-only role tool restrictions at runtime; `user-invocable: false` only hides it from the picker.

## Common failures

- **No agent:** confirm the Copilot harness, repository root, valid YAML and `.github/agents/*.agent.md` path.
- **No subagent:** confirm `agent` tool alias availability and user-invocable/disable-model-invocation settings; look for native Copilot `task` calls. Local's `runSubagent` is not used here.
- **No skill:** check directory/name parity, YAML, slash-command visibility, fresh chat and experimental fork setting when used.
- **Incorrect model:** inspect the Copilot runtime ID, provider access and tool support. This suite doesn't pin unavailable models.
- **No cache results:** inspect the user's existing MCP server configuration separately; fall back to working-tree search.
- **Unexpected code style:** inspect actual target C++ standard and repository `.clang-format`; not all newer policy is yet enforced on `main`.

## Static validator

- Run it again after every instruction/agent/skill edit.
- Negative test: `python Tools/AgentConfig/validate_suite.py --self-test`.
- Static checks cannot prove VS Code loads files or the AI follows them. The live checks above are required for operational acceptance.
