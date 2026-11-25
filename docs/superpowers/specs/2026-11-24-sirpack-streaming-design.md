# SIRPack Streaming Runtime Design

## Intent

Implement the SIRPack runtime described by `SIRPack-Streaming-Implementation-Brief.md`: immutable package ownership, VFS generation leases, bounded chunk I/O, independently decodable codecs, explicit streaming-unit resolution, prediction, and GRDG-owned publication. The brief is the product contract; this document records the implementation decisions needed where the repository is still skeletal.

## Goals

- Replace placeholder SIRPack and compression contracts with the frozen runtime contracts.
- Mount validated SIRM/SIRP data through the existing VFS table and preserve source identity across asynchronous work.
- Provide bounded, generation-checked I/O and codec services with real positional reads and strict digest/length validation.
- Resolve explicit streaming units and deterministic prediction without creating a second ticket authority.
- Publish only through the existing GRDG writer and provide a fixture-driven end-to-end path.

## Non-goals

- Legacy compatibility with the current empty `SIRPack::open()` API or old codec interfaces.
- Native IOCP/io_uring/DirectStorage backends, proprietary codecs, or GPU-specific residency claims.
- Importing the unrelated `system/meshing` resource-graph implementation.
- Domain-specific renderer, physics, audio, or navigation provider implementations.

## Constraints

- C++17, existing SIR Engine allocators, Core Jobs, VFS, and GRDG are authoritative.
- No new standard-library threading primitives or hidden allocation fallbacks.
- All transient queues, pools, and records are bounded and generation checked.
- Workers never call GRDG writer methods or provider callbacks.
- Existing unrelated working-tree changes remain untouched.

## Approaches

1. Add adapters around the current placeholders. Rejected: it preserves incomplete ownership and conflicting interfaces.
2. Import the whole meshing branch. Rejected: it introduces unrelated GRDG changes and violates the brief's authority boundary.
3. Rewrite the SIRPack/streaming seams in-place, using the current Core/VFS/GRDG primitives and adding focused runtime files. Chosen because it satisfies the brief while keeping the existing engine owners canonical.

## Design

Shared package/runtime descriptors live under `Source/Resources`; physical package ownership is split between `SIRPackShard`, `SIRPack`, and `SIRPackMountService`. Streaming owns bounded I/O, unit manifests, prediction, materialization operations, and composition. Compression is a Resources concern so cooker and runtime can share the same profile. All public work is explicit-result, fixed-capacity, and owner-thread serialized; move-only leases carry physical source lifetime without owning decoded payloads.

The first supported codec profile is NONE plus version-1 ZSTD/LZ4 when the pinned dependency is available. A codec is admitted at mount time and must consume exactly the encoded frame and produce exactly the catalog's decoded length. Canonical content identity uses decoded digest/format metadata, never outer compression.

## Acceptance

The implementation is complete only when fresh CMake configuration builds the affected targets, the new SIRPack foundation/runtime/I/O/codec/prediction/publication/lifecycle tests are registered, and the integrated fixture demonstrates mount → positional transfer → decode/verify → GRDG publication → delayed provider acknowledgement → retirement. Negative tests must prove stale generation, short read, digest, capacity, cancellation, and shutdown behavior.
