# GRDG resource runtime

This directory now contains the first native, executable GRDG ownership slice:

- Fixed-capacity, generation-safe resource registry and canonical content keys.
- Single-owner writer, immutable published records, read scopes and conservative
  quiescent-state reclamation; read-side atomic operations do not acquire locks.
- Explicit caller-supplied allocator callbacks; all metadata, staging and payload
  allocations are accounted for and budget-checked. NMD integration is pending.
- Borrowed views across resource kinds, staging-to-resident ownership transfer,
  byte-verified chunk deduplication and dependency/cycle validation.
- Provider pins keeping retired revisions alive until consumers release them.

The public declarations reside under `Public/SIR/Resource/`. All GRDG types are
in the global scope: SIREngine's resource implementation declares no namespaces.

**Unimplemented:** backend-native derived-resource registry, format-specific
image/mesh/audio decoders, streaming I/O and integration into the Godot-derived
engine/editor. Specialized view methods remain interface declarations until
corresponding cooked schemas and subsystem adapters are established.

See [resource interfaces and graph](../../../../Docs/Architecture/14-grdg-resource-interfaces.md)
and `Tests/Unit/SIRResourceGraphTests.cpp`.
