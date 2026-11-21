# Agent overlay validator

Run `python Tools/AgentConfig/validate_suite.py --root .` and `python Tools/AgentConfig/validate_suite.py --self-test` after extracting. Requires PyYAML (`pip install pyyaml`). Checks YAML, required roles, skill names, links and native Copilot aliases; it cannot prove that local provider models/MCP tools actually execute. Inspect and smoke-test the suite in VS Code.
