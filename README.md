# SIREngine

**A Godot-derived game engine built for authored worlds, explicit resource ownership, and predictable runtime costs.**

SIREngine is the engine being developed for **The Nomad**, **Sigilbound**, and subsequent games. Its starting point is Godot: the editor, scene tools, import pipeline, scripting integration, and established platform infrastructure provide a practical environment in which to build games. Its long-term direction is a substantially different runtime, organized around controlled memory, shared resources, offline-processed content, and systems that can be measured from startup to shutdown.

The project brings together two approaches to engine design. Godot supplies a capable authoring environment and a portable foundation. The runtime design draws on the resource discipline of seventh-generation engines: compact data, bounded working sets, explicit allocation lifetimes, streaming, and carefully separated hot and cold work. Modern graphics APIs, tools, and concurrency are used where they serve those goals.

SIREngine is intended to be a **single engine developed across multiple games**, with improvements to the underlying systems benefiting all of them. Gameplay rules, world content, save schemas, network packets, and per-game budgets remain the responsibility of each game.

## Why this engine exists

Building a complex game involves more than rendering triangles or simulating objects. The editor must let developers work quickly with scenes, materials, terrain, animation, scripting, and inspection. The shipping executable has a different job: keep the required world state resident, load and unload assets efficiently, simulate predictable workloads, and avoid spending memory on structures that were useful only during authoring.

SIREngine separates those responsibilities.

A scene can be made of convenient Godot nodes in the editor without requiring every static object to remain a fully instantiated node in the shipping world. Meshes, terrain, collision, navigation, animation data, and other assets are candidates for conversion into compact, versioned packages. The runtime uses stable resource identities to track their data and the derived representations needed by each subsystem.

The design goal is to make memory and work **accountable**. When a region streams in, the engine should be able to explain what was loaded, which systems reference it, how much committed memory it occupies, when it can be evicted, and what work was required to bring it into use.

## Architecture

The intended engine is organized around four connected responsibilities:

| Area | Responsibility |
| --- | --- |
| **Authoring** | Godot-derived editor, scenes, inspectors, imports, scripting, debugging, and iteration tools |
| **Cooking and packaging** | Deterministic conversion of authored assets into versioned, platform-ready data and dependency graphs |
| **Runtime foundation** | Memory domains, EASTL containers, job scheduling, resource identity, streaming, and diagnostics |
| **Subsystem providers** | Graphics, physics, animation, navigation, audio, networking, and platform-specific services |

The engine's core is independent of any particular game. A title supplies its content, gameplay systems, resource priorities, and configuration; SIREngine supplies the execution and resource infrastructure.

### Author in the editor; stream compact data in the game

The editor remains a full scene-based workspace. Artists and designers can place objects, author terrain, inspect materials, configure collisions, and use the tooling already developed around Godot.

The planned cooking pipeline takes those authoring representations and produces validated data for the shipping runtime:

```text
Godot editor scenes, materials, meshes, terrain, animation
                         |
                  Import and cook
                         |
             Asset IDs + dependencies
                         |
               Versioned world packages
                         |
            Streaming and resource graph
                         |
             Runtime subsystem consumers
```

Static scenery, repeated meshes, terrain regions, collision structures, navigation tiles, and hierarchical LOD data can be represented as compact spatial records rather than independently allocated editor objects. Dynamic entities continue to use the behavior and state machinery their gameplay requires. The result is an engine that can retain a rich editor workflow while giving the shipping runtime a different representation of the same authored world.

### One authoritative resource graph

The planned **Global Resource Dependency Graph (GRDG)** is the common resource authority for the engine. A resource has a stable identity, a versioned handle, known dependencies, an owner, and a residency state. Rendering, physics, navigation, audio, gameplay, and editor tools access that identity rather than maintaining unrelated CPU-owned copies of the same underlying asset.

The design uses **one writer and multiple lock-free readers** for published resource records. Readers observe immutable versions; publication and retirement are controlled so that readers, jobs, GPU work, and audio callbacks cannot outlive the memory they reference. Derived data—such as a GPU buffer, a physics acceleration structure, or a decoded audio representation—is accounted for under the same resource identity.

This graph is intended to make streaming and memory reclamation observable rather than scattered across independent caches. Its contracts are detailed in [the GRDG specification](docs/architecture/03-grdg.md).

### Memory as an engine-wide system

SIREngine is designed around centralized CPU allocation ownership, explicit domains, and measurable budgets. The memory system will ultimately account for allocations made by the inherited Godot core, SIREngine modules, physics, audio, rendering-side CPU structures, and integrated libraries.

Different workloads need different allocation behavior. Frame scratch data can come from transient arenas; resident resources need stable storage and controlled eviction; physics needs per-step scratch and longer-lived structures; audio needs bounded callback-safe allocation; editor tooling has different lifetime and capacity requirements from a shipping game. Those domains share reporting and budget enforcement, without forcing every allocation pattern through the same suballocator.

The Nomad is being designed around a **512 MiB CPU-memory envelope**. That is a demanding game configuration used to evaluate the engine's design, not a hard-coded memory limit imposed on every SIREngine title or on the editor. Device-local GPU allocations, mapped files, external SDK memory, and CPU resident memory must be reported distinctly.

Memory statistics are meant to include committed and resident bytes, retained pool capacity, allocation traffic, high-water marks, failures, and resource ownership. See [memory and allocator architecture](docs/architecture/02-memory-and-allocators.md).

### Data-oriented runtime engineering

The runtime favors contiguous storage, compact POD records, short ownership chains, predictable update loops, and explicit work scheduling. Hot structures should fit within cache-line-sized working sets where their access patterns justify it. Expensive conversion, dependency discovery, and shader/material preparation should occur offline or during controlled streaming instead of inside ordinary frame updates.

SIREngine uses **C++17** and [EASTL](https://github.com/electronicarts/EASTL) as the initial container foundation. Engine-facing aliases such as `SIRList`, `SIRHashMap`, `SIRArray`, and `SIRString` establish a consistent vocabulary while specialized allocators and storage policies are introduced. Fixed-capacity containers and borrowed views are available for workloads that should not allocate dynamically.

The code style is deliberately explicit: functions, storage, lifetimes, and data flow should be easy to inspect in source and in profiler traces. The [C++ standard](docs/standards/cpp.md) documents the repository's formatting and code-generation rules.

## Rendering and world systems

SIREngine's rendering direction is built around efficient residency, clear visibility decisions, controlled GPU uploads, and backend-independent resource lifetime management. The target graphics APIs are:

| Platform | Graphics backends |
| --- | --- |
| Windows | Direct3D 12, Vulkan 1.4 and newer, OpenGL 4.6 |
| Linux | Vulkan 1.4 and newer, OpenGL 4.6 |
| macOS | Planned after the Windows and Linux engine paths are established |

A dedicated OpenGL 4.6 backend and Vulkan 1.4 support are development targets; they are not simply renamings of Godot's existing compatibility renderer or inherited Vulkan driver.

World rendering is expected to work with streamed terrain, hierarchical LOD, mesh instances, materials, and visibility data whose ownership is tied to the GRDG. Physics and navigation should consume compatible cooked geometry and spatial information without forcing full independent CPU copies of source assets. The engine's graphics and world interfaces will preserve the editor's authoring tools as the runtime representation evolves.

The target is strong visual readability, authored detail, responsive input, and consistent frame times with a controlled memory footprint. Rendering features are evaluated by what they cost in GPU time, CPU scheduling, residency, and content authoring—not only by whether a driver supports them.

## Audio and other engine services

Audio is a backend service with consistent resource and lifetime rules. **FMOD Studio** is the first intended integration; **Wwise** and **OpenAL** are later provider targets. Banks, event metadata, streaming samples, voices, and provider-owned allocations must be tracked through the appropriate memory and resource domains. The engine-level interface should remain independent of the chosen audio SDK.

The same ownership model extends into physics, animation, navigation, filesystem access, networking, logging, configuration, debugging, and jobs. Individual games define their own gameplay behavior and protocol contents; the engine supplies common infrastructure and integration points.

### Relationship to NomadFramework

[NomadFramework](https://github.com/SIGAAMDAD/NomadFramework) is the earlier reusable framework developed for game-facing services, including logging, console and configuration variables, events, file access, saves, online services, and audio integration. That work is a technical reference for SIREngine's common-system design and future source migration.

SIREngine takes those reusable ideas into a different context: systems become part of a complete engine with explicit allocation domains, resource residency, platform backends, editor integrations, and shared ownership rules. Source from NomadFramework will be audited and adapted at the applicable engine boundaries rather than assumed to be already integrated into the current repository.

## Development status

This `develop` branch is the **new source foundation** for the engine migration. It is not yet a finished SIREngine runtime.

| Component | Current repository state |
| --- | --- |
| Godot source | Owner's Godot fork pinned as the source baseline |
| Build orchestration | CMake 4.4.5 for SIR-owned code; Godot built through its existing SCons graph |
| Language and containers | C++17 configuration and initial EASTL aliases |
| Tests and CI | Foundation smoke tests and Windows/Linux workflows defined |
| Memory domains and Godot allocation migration | Architecture specified; native integration in progress |
| GRDG and cooked-resource runtime | Architecture specified; implementation pending |
| Graphics | Inherited Godot source; SIREngine backend changes pending |
| Audio | Optional SDK dependency declarations; engine providers pending |
| Game integrations | The Nomad and Sigilbound integration pending |

The source layout is deliberately transitional. Godot is currently pinned under `engine/godot/`; SIREngine-owned headers, modules, and build infrastructure live beside it. A later source consolidation will bring the engine tree under the SIREngine module structure once the corresponding build and interface migrations have been validated. The project's [architecture documents](docs/README.md) and [implementation roadmap](docs/roadmap/foundation.md) distinguish the intended design from completed work.

## Building the foundation

Clone **`develop`** with its pinned submodules:

```sh
git clone --branch develop --recurse-submodules https://github.com/SIGAAMDAD/SIREngine.git
cd SIREngine
python -m pip install "cmake==4.4.5" "scons==4.11.1"
```

For a Linux C++17/EASTL foundation build:

```sh
cmake --preset linux-ci
cmake --build --preset linux-ci
ctest --preset linux-ci
```

On Windows, use `windows-ci` in place of `linux-ci`. The `linux-godot` and `windows-godot` presets expose the pinned Godot editor build, which additionally requires the native platform compiler and development dependencies. A macOS foundation preset exists for manual preview.

The [build and dependency guide](docs/architecture/10-build-and-dependencies.md) documents the current entrypoints and the division between CMake-owned source and the inherited SCons build.

### Repository map

```text
engine/godot/       Pinned Godot engine and editor source
include/sir/        SIREngine public C++ interfaces and types
src/                SIREngine-owned runtime and integration modules
cmake/              Dependency resolution and SCons orchestration
third_party/        Pinned EASTL-family dependencies
tests/              Foundation tests and future integration suites
tools/              Future cookers, shader tools, and validators
scripts/            Repository hygiene and formatting checks
docs/architecture/  Subsystem design contracts
docs/standards/     Coding and review standards
docs/adr/           Recorded architecture decisions
.github/            CI, contribution, and issue infrastructure
```

## Engineering and contribution

Engine changes are expected to carry actual source, reproducible build inputs, and evidence appropriate to their scope. A renderer change needs native driver tests; an allocator migration needs allocation traces and failure behavior; a resource-publication change needs concurrency and lifetime tests. A successful formatter or a documentation update is not a substitute for those checks.

See [contributing](CONTRIBUTING.md), [governance](GOVERNANCE.md), [C++ standards](docs/standards/cpp.md), and [the full architecture index](docs/README.md).

## Licensing and attribution

SIREngine retains the repository's [GPL-2.0 license text](LICENSE). Its pinned Godot and EASTL-family components retain their respective upstream copyright and license notices. External FMOD and Wwise SDKs are not distributed with this repository. See [third-party notices](THIRD_PARTY_NOTICES.md).
