# Engine source

Native C++17 SIREngine source is divided by responsibility:

- `Core/`: portable foundation, allocation domains, math, containers, job scheduling and common services.
- `Runtime/`: built-in terrain, graphics, resources, world, animation, audio, physics, networking and scripting.
- `Editor/`: scene tools, import pipelines, terrain editing and authoring UI.
- `Developer/`: diagnostics and profiling.
- `Programs/`: engine executables and command-line tools.
- `Platform/`: Windows, Linux and later macOS.

Terrain is a built-in runtime system under `Runtime/Terrain/`, with integrated editor tooling under `Editor/Terrain/`; it is not an optional extension.

The native Core/Launcher bootstrap is currently built. Other systems are added as source and acceptance tests are ported.
