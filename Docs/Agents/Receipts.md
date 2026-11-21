# Receipt template

```
TASK: <identifier>
BASELINE: <repo, branch, SHA; initial dirty state>
AUTHORIZED SCOPE: <what user requested>
RESULT: COMPLETE | PARTIAL | BLOCKED
CHANGED: <actual modified paths and their purpose>
CONTRACTS: <ABI/serialization/ownership/threading/API changes, or NONE>
DELEGATION: <worker, scope, actual result, or NONE>
FORMAT: PASS|FAIL|NOT RUN; command, version, files
BUILD: PASS|FAIL|NOT RUN; exact command, compiler, target, exit
TESTS: PASS|FAIL|NOT RUN; exact command, count and failures
AST/COVERAGE: PASS|FAIL|NOT RUN; method, source, thresholds and output
SANITIZERS/GPU/BENCHMARKS: PASS|FAIL|NOT RUN; exact evidence
GIT/GITHUB: <operations actually performed and verified, or NONE>
DEFECTS: <ordered findings with path:line>
REMAINING: <precisely what is incomplete and why>
```

Never label PASS without actual tool output. Avoid vague claims like "fully verified" when only unit tests ran. For a read-only investigation replace CHANGED with source evidence and use NOT RUN for skipped execution.
