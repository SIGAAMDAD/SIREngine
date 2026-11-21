# Source and ownership guide

The connected `main` tree includes:

- `Source/Core/` - platform, memory, containers, strings, objects, jobs, systems, VFS, events, configuration.
- `Source/Renderer/` - rendering/device/resource-graph-related source.
- `Source/Terrain/`, `Source/Streaming/`, `Source/World/`, `Source/Mesh/` - terrain/world data and residency boundaries.
- `Source/Audio/`, `Source/Scripting/`, `Source/Resources/`, `Source/Editor/`, `Source/UI/` - subsystem implementation roots.
- `Tools/ShaderCooker/`, `Tools/StringCatalogCooker/`, `Tools/RegistrationCooker/` - cook and tooling code.
- `Tests/`, `Verification/` - source-owned tests and qualification.
- `Build/CMake/`, `CMakeLists.txt`, `CMakePresets.json` - build and dependency source of truth.
- `Docs/Architecture/`, `Docs/Standards/`, `Docs/Decisions/` - engineering contracts; source behavior must still be verified.
- `ThirdParty/` - excluded from SIREngine-owned formatting and global-namespace rules.

Use directory discovery on the actual branch. This list is a starting map, not a guarantee that every discussed feature is implemented.
