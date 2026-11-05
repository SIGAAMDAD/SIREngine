## Change contract

What is being implemented, and which engine subsystem owns it?

## Source and behavior

- [ ] Actual source/interface changes are described; external ABI and serialized behavior checked.
- [ ] Public docs/ADR updated where ownership, data format or behavior changes.
- [ ] No build outputs, SDK redistributables or generated cache files committed.

## Verification

- [ ] Pinned SIR formatter and C++ style checks pass.
- [ ] Relevant translation units built and linked (list exact config and compiler).
- [ ] Affected tests and target-platform checks run (record what was not run).
- [ ] Memory allocations, reclamation and hot-path performance measured if relevant.
- [ ] Godot-specific formatter/SCons editor checks run for changes under engine source.

## Evidence

Commit SHA, commands, logs, benchmark comparisons and remaining blockers:
