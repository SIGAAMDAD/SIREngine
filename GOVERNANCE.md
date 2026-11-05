# Project governance

SIREngine is maintained under the SIGAAMDAD account. The project owner determines architectural scope, accepted dependencies, branching and releases; outside contributors participate through issues, ADRs and reviewed pull requests.

`main` represents released/stable engine history. `develop` is the integration branch for the new Godot-derived version. Feature branches are short-lived, focused and validated; a PR does not automatically merge because its tests are green. Breaking engine ABI, memory ownership, platform support or licensing decisions require an ADR and an explicit migration contract.

Code owners and maintainers should review source evidence and measured results, not inferred claims of performance. CI must report actual results and incomplete target coverage accurately.
