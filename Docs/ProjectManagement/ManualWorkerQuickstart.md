# Manual ChatGPT worker quickstart

SIREngine uses **manually launched** worker chats. GitHub Issues and the SIREngine Project are the work queue. No cron service, SQLite coordinator, project-sync script, or Projects GraphQL token is required.

## Launch prompt: random existing work package

Copy the following into a new worker chat while connected to the SIREngine GitHub app:

> Operate as a SIREngine Marathon WORKER in `SIGAAMDAD/SIREngine`.
>
> I authorize you to select **one** existing, open, bounded work-package issue from the SIREngine backlog at random and implement its **current documented scope**. Do not invent new architecture, extend scope, or treat roadmap parent issues as single coding tasks. This instruction authorizes implementation of the chosen existing work package only, not integration.
>
> Read all candidate issues (including paginated results), check owner-approved plans or this explicit authorization, parent scope, dependencies, exact source ownership, existing claims, PRs and current `develop` HEAD. Exclude blocked, in-progress, conflicting or incomplete-contract tasks. Randomize only among **eligible** candidates; do not choose the first issue shown by search.
>
> Claim the issue **before** editing source. If concurrent chats are possible, use the existing `ops/sirengine-control` fixed-path claim protocol; otherwise post a clear issue comment and label it `marathon:active`. Record the selected issue, source baseline, owned paths, work branch and this launch authorization.
>
> Implement on the appropriate dedicated feature branch, verify actual files with compiler/build/tests/coverage when required, push focused commits, and open or update a PR. Give the exact SHA and any unmet acceptance gates. Do not merge without my explicit approval. Once work is safely checkpointed, stop after this one package and report its issue/PR links.

## Launch prompt: keep selecting work

Use when you want a single chat to continue until its runtime ends:

> Run the SIREngine Marathon WORKER protocol for `SIGAAMDAD/SIREngine`. I authorize implementation of **existing, eligible, bounded work packages** drawn randomly from the open backlog, one at a time. Preserve approved architecture and work-package scope. Claim/check ownership before writes, produce real commits and tested PRs, and maintain exact GitHub handoff checkpoints.
>
> After publishing each PR or completing an authorized task, release or reconcile its claim, refresh the full current GitHub queue, and select the next independent eligible item. Never merge without separate authorization. Stop only on exhausted eligible work, a blocker that prevents safe independent progress, or the host execution limit.

## Verifier prompt

> Work as an independent SIREngine Marathon VERIFIER. Select one open SIREngine PR or explicit verification issue awaiting qualification. Inspect its exact head SHA, required contracts, relevant CMake/CTest/GoogleTest and platform gates; run the checks you can actually execute. Publish PASS/FAIL/NOT_RUN evidence tied to that SHA. Do not call a skipped platform PASS. Do not merge.

## Integrator prompt

> Work as a SIREngine Marathon INTEGRATOR. Audit the open PRs and their dependencies against current `develop`, exact SHA verification, and owner-written merge approvals. Merge **only** explicitly authorized PR revisions that pass all required gates. Record actual merge SHA, update linked tasks and parent milestones, and continue to the next eligible independently authorized PR. Leave all others open with exact blockers.

## Minimal issue claim / handoff comment

```text
MANUAL MARATHON CLAIM
Issue: #<number> | Worker: <session-id> | Role: WORKER
Owner authorization: <explicit launch instruction / GitHub approval link>
Plan/work-package revision: <exact issue revision or approved package key>
Baseline develop SHA: <40-character SHA>
Work branch / current SHA: <branch> / <SHA>
Exclusive writable paths: <paths>
Dependencies: <verified predecessors>
Claim: <control-branch path + token/epoch OR single-writer advisory claim>
Next action / acceptance: <specific executable step>
```

For a handoff, append the current code SHA, PR URL, tests (PASS/FAIL/NOT_RUN), blockers, and next action. Do not reset existing branches or overwrite another worker's work.

## Where to look

- [Pick a task](https://github.com/SIGAAMDAD/SIREngine/issues?q=is%3Aissue+is%3Aopen+-label%3Akind%3Aroadmap+-label%3Amarathon%3Aactive+-label%3Amarathon%3Ablocked)
- [Roadmap parents](https://github.com/SIGAAMDAD/SIREngine/issues?q=is%3Aissue+label%3Akind%3Aroadmap)
- [Active claims](https://github.com/SIGAAMDAD/SIREngine/issues?q=is%3Aissue+is%3Aopen+label%3Amarathon%3Aactive)
- [Blocked items](https://github.com/SIGAAMDAD/SIREngine/issues?q=is%3Aissue+is%3Aopen+label%3Amarathon%3Ablocked)
- [Open PRs](https://github.com/SIGAAMDAD/SIREngine/pulls)
- [Project setup and workflow](GitHubProjects.md)

The issue search links are convenience entrypoints. Before random selection, verify the **entire** current eligible set through GitHub, not just a partial search page.
