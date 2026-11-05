# 14 - Managed scripting and Mono embedding

**Status:** Mono SDK dependency discovery established; C# engine hosting remains unimplemented.

SIREngine plans C# scripting through an embedded Mono runtime. A compatible Mono embedding SDK can be enabled with `SIR_WITH_MONO=ON`. This creates a CMake target, `SIR::MonoRuntime`, with the native include and link dependencies, but the Core/Launcher does not yet load managed assemblies.

## Engine-facing contract

- The scripting interface is engine-owned, independent of the Mono ABI. Public engine headers should expose stable script IDs, script instance handles and binding descriptors rather than `MonoObject*` or other unmanaged external pointers.
- Managed assembly loading, reference resolution, per-game script bindings, invocation, exceptions and serialization must have explicit contracts.
- Wrapper lifetime and GC rooting must be coordinated with engine object/GRDG handles. Native resource allocations and managed Mono heap bytes are reported as distinct budget categories.
- Editor compilation, code completion, script hot reload and managed debugging are editor/tooling responsibilities. Runtime deployment must include a compatible Mono runtime and selected managed assemblies.
- Platform-specific embedding and dynamic-library loading must pass Windows and Linux native runtime tests; macOS is deferred under the platform plan.

## Acceptance

Require real compile/link tests against the selected embedding SDK, a managed assembly smoke load, method invocation, native-to-managed object lifetime tests, GC/exception tests, editor script reload and launch/package tests on supported platforms. Pin the final runtime version when the bridge exists.

See [Dependency acquisition](../Dependencies.md) and the existing engine startup/editor milestone.
