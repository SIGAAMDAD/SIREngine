# Agent-suite static validation

From SIREngine root:

```
python Tools/AgentConfig/validate_suite.py --root .
python Tools/AgentConfig/validate_suite.py --self-test
```

The validator checks YAML frontmatter, names/locations, instructions `applyTo`, limited tool aliases, Copilot-vs-Local fields, relative Markdown links, duplicate skill names, and policy conflicts. It does **not** emulate VS Code's runtime and cannot assert agent/subagent activation, compiled source correctness or MCP availability.
