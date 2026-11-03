# Resources runtime subsystem

The GRDG resource-type interfaces are proposed, declaration-only C++17 contracts.
They do not implement residency, publication, streaming, or reclamation yet.

- [Resource inheritance, dependency graphs and ownership contract](../../../../Docs/Architecture/14-grdg-resource-interfaces.md)
- Public declarations: `Public/SIR/Resource/`
- Compile-only interface test: `Tests/Unit/SIRResourceInterfaceTests.cpp`

Specialized views currently share a review header; implementation moves them into their consuming runtime modules.
