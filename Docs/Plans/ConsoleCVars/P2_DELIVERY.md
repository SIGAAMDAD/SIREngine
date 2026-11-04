# P2 — Cooked registration descriptors and dynamic overlay

**Branch:** `feature/console-cvars-commands` — source authored for PR #275; tracking [issue #277](https://github.com/SIGAAMDAD/SIREngine/issues/277).

## Native runtime owner

`Source/Core/Registration/SIRRegistrationCatalog.{h,cpp}` loads P0 SIRC v1 metadata through `sirValidateRegistrationCatalog` and the P1 `sirRegistrationCookedName` adapter into caller-supplied SIRMemory-owned buffers. It validates section schemas and bounds, name ordinals and stable IDs, canonical qualified names, command/CVar name exclusivity, sorted unique command binding IDs, CVar schema IDs, type/flag/default payloads and mandatory callback availability **before publishing initialized state**. The descriptor table and name scratch use no implicit runtime heap allocation.

Command bindings are resolved from the caller's sorted runtime binding table by stable 64-bit binding ID and owning module ID. The cooked file never contains C++ function pointers. Registration handles are `{slot,generation}`; stale reuse is rejected. `registerDynamic` admits cold qualified module records subject to capacity, schema/name/binding conflicts, and the shared string pool's bounded allocation policy. `unregisterDynamic` and `unregisterModule` invalidate handles and clear callbacks. Invocation and module-code execution leases are owned by P4 console dispatcher; P2 never calls a command implicitly.

## Offline cooker

`Tools/StringCatalogCooker/SIRStringCatalogCooker.{h,cpp}` generates deterministic sorted canonical SIRS v1 names. `Tools/RegistrationCooker/SIRRegistrationCooker.{h,cpp}` normalizes/shuffles inputs deterministically and generates SIRC v1 metadata and typed default payloads. Includes strict duplicate name/binding/schema admission, module namespace policy, flag/type checks and UTF-8 string default validation. Both cook products are built together so SIRC references the correct SIRS corpus hash.

`SIRRegistrationCooker` CLI accepts a declarative line manifest and writes two binary files:

```text
SIRRegistrationCooker input.manifest output.sirs output.sirc
command NAME BINDING_ID CAPABILITIES OWNER_ID ARG_SCHEMA_ID
cvar NAME SCHEMA_ID TYPE_NUMBER FLAGS OWNER_ID DEFAULT_BITS_OR_STRING
```

The optional v1 scalability section remains empty until P6 defines grouped profile value ownership; it is rejected if supplied without that schema contract.

## Tests and acceptance

New `core.registration.cooker` linked native fixture validates shuffled-input byte equality, real SIRStringPool cooked identity, static command and CVar lookups, callback binding, dynamic module registration, stale handle reuse/unload, duplicate names, wrong ordinals, corrupted defaults and missing callbacks. `core.registration.cooker-cli` cooks re-ordered manifest files and compares SHA-256 outputs, then rejects duplicate manifests. Both are native CMake/CTest targets.

**Pending this commit's CI:** Linux and Windows native build, both tests, source/formatter checks. P2 must remain open until exact-source tests pass. Future P3 owns live typed CVar storage and event-credit transaction commits; P4 owns command invocation and execution leases.

**Caller-lifetime requirements:** Input SIRC bytes, SIRS pool and backing entry/name/binding buffers must remain alive until `SIRRegistrationCatalog::shutdown`. Shutdown/retirement requires no outstanding module callback invocation; P4 will enforce leases.
