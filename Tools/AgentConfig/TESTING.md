# Validator negative-case coverage

The static checker accepts a root directory and returns nonzero on missing suite members, malformed frontmatter, wrong skill directory/name, missing `applyTo`, duplicate agent names, Local-only agent fields, unsupported tool aliases, broken relative links and missing mandatory root contracts.

Use `--self-test` to exercise the parser. The full suite check also validates all packaged files. Live Copilot harness loading, subagent invocation, model/tool access and actual repository compile correctness require runtime checks in `Docs/Agents/Installation.md`.
