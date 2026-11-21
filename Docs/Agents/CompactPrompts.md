# Short prompt patterns

With `SIR Director` selected:

```
Complete issue #39. Delegate source inspection and independent verification.
```

```
Investigate the FrameGraph resource lifetime. Read-only; return source evidence.
```

```
Refactor Source/Core/Containers/<type> with behavior and ABI unchanged. Implement and test.
```

```
Plan a three-worker split for <subsystem>. Freeze APIs and file ownership. Do not edit.
```

```
Integrate the approved worker patches from baseline <SHA>. Verify and report receipts.
```

```
Profile:memory <container> against <baseline SHA> with matching workload.
```

Explicit skills:

```
/sir-complete-issue #39
/sir-investigate GPU resource eviction
/sir-architecture-plan streaming ownership refactor
/sir-verify current working diff
/sir-profile allocator growth
```

The compact prompt supplies the goal, not the entire instruction policy. Agents must still discover current source, permissions and target-specific requirements.
