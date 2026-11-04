# Native World Partition — initial implementation

`Source/World/Partition/SIRWorldPartition` accepts a canonical, sorted, cooked
spatial cell directory and produces bounded, deterministic streaming demands.
It **does not** load packages, own resources, instantiate runtime Nodes, advance
physics, create a second mutable world database, or publish GRDG versions.

- Caller-owned aligned storage is used for the copied cell directory and scratch
  state. No allocation is performed on the evaluate path.
- The cooker supplies cell coordinates and stable authored IDs. Each manifest
  generation is validated for sorted unique grid coordinates and unique IDs.
- Grid coordinates use signed 32-bit cell units. Sources have circular X/Y
  interest and optional separate vertical slice radius.
- World Data Layer bits are conjunctive: `(required & enabled) == required`.
  Zero denotes globally eligible cells; the always-loaded flag requests active
  residency without any source when its layer requirements permit it.
- Overlapping sources produce one demand per cell using maximum target stage,
  maximum priority and earliest deadline, preserving deterministic cell order.
- One partition-owned consumer ID and manifest generation identify the demand
  stream. Each evaluation takes an explicit request epoch. `SIRStreamingDemandReconciler`
  now performs full-set epoch reconciliation against the streaming coordinator.
- Output capacity failure is atomic: the required count is returned without
  modifying the caller's demand buffer. Manifest storage stays bounded.
- The directory is a runtime in-memory view; its binary layout is **not** the
  SIRPack serialization contract. SIRPack and GRDG remain separate owners.

## Integration follow-up

1. Freeze SIRPack world-cell manifest format and stream coordinates/dimensions.
2. Connect existing reconciliation tickets to GRDG resource requests, pending
   I/O completion, and provider-retirement acknowledgements.
3. Connect cell resource bundles to the canonical GRDG/IO coordinator (PR #23).
4. Add committed world-state views and layer policy; GPU/Jolt/nav provider-safe
   activation and HLOD/terrain demand are separate implementations.
5. Benchmark grid lookup, load-to-activate time, cancellation and memory.

The initial query scans a contiguous sorted X range per source after a binary
lower bound. More aggressive 2D indexing is a future performance decision based
on measured sparse/dense world layouts.
