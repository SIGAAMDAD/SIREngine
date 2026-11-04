# Resource catalog foundation (R3)

`SIRResourceCatalog` owns only resource *metadata*, using caller-supplied
arrays; the future SIREngine resource loader owns bytes, and the renderer/
GRDG own GPU residency. Resources are referenced by stable resource IDs;
path strings are atoms from the shared string pool.

A metadata pass registers external/embedded resources, then a second pass
adds dependencies in any declaration order. Each edge checks duplicate,
missing-resource and transitive cycle constraints without dynamic memory.
The catalog can be sealed before use by streaming/asset services.

This is not a ResourceLoader/ResourceSaver, an asset importer or a GRDG
implementation. It supplies a native metadata boundary for those ports.
