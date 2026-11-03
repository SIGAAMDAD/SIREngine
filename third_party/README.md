# Pinned dependencies

All open-source C++ dependencies are pinned Git submodules. This bootstrap intentionally contains **no committed build output or redistributable FMOD/Wwise SDKs**.

| Path | Source | Role |
| --- | --- | --- |
| `engine/godot` | SIGAAMDAD/godot | Godot fork and editor/runtime source |
| `third_party/EASTL` | electronicarts/EASTL | Container implementation |
| `third_party/EABase` | electronicarts/EABase | EASTL platform primitives |
| `third_party/EAAssert` | electronicarts/EAAssert | EA assertion dependency, reserved |

Install audio SDKs separately and pass `SIR_FMOD_SDK_ROOT` or `SIR_WWISE_SDK_ROOT` when the matching dependency is enabled. OpenAL is resolved with CMake's `find_package(OpenAL)`. The current foundation does not build audio adapter binaries.
