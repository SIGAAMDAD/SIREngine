# Mono embedding runtime

Mono is the planned C# scripting runtime dependency. This directory holds **no runtime source or vendored SDK binaries**.

Install an appropriate Mono embedding development SDK on the target platform, then enable:

```sh
cmake --preset tooling -DSIR_WITH_MONO=ON -DSIR_MONO_SDK_ROOT=/path/to/mono/sdk
```

On Unix, the build first attempts `pkg-config` discovery for `mono-2` or `mono-2.0` when an explicit SDK root is not set. Otherwise CMake searches for `mono/jit/jit.h` and a compatible Mono embedding library. Successful discovery exposes `SIR::MonoRuntime`.

This is a link-time SDK contract. Actual C# scripting will require assembly loading, native/managed bindings, GC/lifetime rules, Godot-compatible C# behaviors, editor integration and deployable Mono runtime binaries. Those features are not yet implemented.

The legacy Mono source repository is not being copied into the engine. Source-level Mono implementation is maintained within the modern .NET runtime ecosystem; the version of an embedding SDK must be selected for an actual integration target.
