# SIREngine foundation and implementation roadmap

The `develop` branch is a freshly cleaned repository-level baseline. Work is split into gates so that an infrastructure commit does not get confused with a completed engine rewrite.

| Phase | Deliverable | Initial status |
| --- | --- | --- |
| F0 | Clean develop tree, documented layout, pinned Godot/EASTL sources | Implemented in bootstrap tree |
| F1 | CMake 4.4.5, C++17 aliases, formatting, Windows/Linux CI, nightly editor builds | Workflow definitions implemented; remote execution unverified at bootstrap |
| F2 | Exact-source Godot build baselines, SCons compile databases, source/ABI inventory | Pending |
| F3 | NomadFramework/NMDMemory source audit, central allocator trace and domain adapters | Pending — original `nframework.zip` not mounted here |
| F4 | EASTL allocator routing, great container migration and central allocation limits | Pending |
| F5 | GRDG canonical storage, lock-free reader publication and dependency graph | Pending |
| F6 | Editor-to-cooked runtime asset system, chunk streaming and node reduction | Pending |
| F7 | D3D12, Vulkan 1.4+, dedicated OpenGL 4.6 backend acceptance | Pending beyond inherited drivers |
| F8 | FMOD integration; later OpenAL and Wwise SDK adapters | Pending beyond declarations |
| F9 | The Nomad and Sigilbound scene integrations; measured performance/memory | Pending |
| F10 | Native codebase consolidation and macOS support after Windows/Linux | Pending |

## Graduation rules

Every phase must provide actual source, exact build provenance, compiler/link output, format/AST gates and the relevant runtime/performance tests. Milestone state changes require a commit and evidence. Nightly CI verifies the upstream pinned editor, while separate acceptance tests gradually add actual SIR runtime modules.
