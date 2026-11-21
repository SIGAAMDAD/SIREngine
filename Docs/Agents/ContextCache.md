# Context-cache MCP integration

The Rust source-context cache discussed for SIREngine can shorten repeated investigations, but its executable path, startup arguments, protocol and tool names have **not** been supplied or verified in this package. Do not invent a runnable `.vscode/mcp.json` or overwrite an existing one.

## Contract for any connected cache

1. Use cache to locate candidate files, symbols, dependency relationships and provenance.
2. Include source baseline / branch / commit SHA in queries where the tool permits it.
3. Read the exact current source before citing behavior, editing files or asserting that a cached AST corresponds to the active compile flags.
4. Compare cache index identity to the checkout and invalidate stale results before implementation.
5. Keep cache retrieval concise (relevant paths, symbol, summary, version), not entire archived documents.
6. If MCP tools are absent, fall back to project file search and the real compilation database without blocking the task.

To bind the cache, inspect your existing `.vscode/mcp.json` and confirm the server name and actual tool identifiers through VS Code's MCP inventory. You may add `<confirmed-server-name>/*` to an appropriate custom agent `tools` list only after proving that namespace exists.

See [the VS Code MCP documentation](https://code.visualstudio.com/docs/copilot/customization/mcp-servers) for the configuration format supported by your installed version.
