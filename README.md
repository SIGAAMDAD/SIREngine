# SIREngine

SIREngine is the reusable engine foundation for **The Nomad**, **Sigilbound**, and future games. It retains Godot's editor, scene authoring and platform abstraction while incrementally replacing runtime backends, allocation domains and shared resource ownership with a tightly budgeted, 7th-generation-inspired design.

## Source baseline

This branch is a **clean foundation**. `engine/godot` is an exact, pinned checkout of [SIGAAMDAD/godot](https://github.com/SIGAAMDAD/godot); the three EASTL-family dependencies are likewise pinned Git submodules. SIREngine-specific integrations live outside that checkout until they are incorporated with their own audited migration. No legacy SIREngine build outputs, old SDL binaries, or the former custom-engine code are present at this branch tip.

**Status:** repository infrastructure and type aliases are bootstrap implementation; the complete Godot memory, resource, rendering and audio integrations are architectural work, not completed engine features. Godot is still compiled by SCons, invoked from the CMake superbuild.

## Quick start

```sh
git clone --recurse-submodules https://github.com/SIGAAMDAD/SIREngine.git
cd SIREngine
python -m pip install "cmake==4.4.5" "scons==4.11.1"
cmake --preset linux-ci
cmake --build --preset linux-ci
ctest --preset linux-ci
```

On Windows use the `windows-ci` presets. Full Godot editor compilation is exposed by `linux-godot` and `windows-godot`, with the platform dependencies documented in [the build contract](docs/architecture/10-build-and-dependencies.md). A `macos-preview` preset exists for manual experimentation; macOS acceptance follows Windows and Linux.

## Architecture and contribution

Start with [the architecture index](docs/README.md), [the source roadmap](docs/roadmap/foundation.md), [coding standards](docs/standards/cpp.md), and [contributing](CONTRIBUTING.md). SIREngine-owned C++ is C++17 with EASTL container aliases. Memory and resource ownership are explicit system contracts; an alias is not a new allocator or shared-resource implementation.

## License

The repository carries its pre-existing GPL-2.0 license text in [LICENSE](LICENSE). Pinned Godot and EASTL-family dependencies retain their own upstream licenses; refer to [third-party notices](THIRD_PARTY_NOTICES.md).
