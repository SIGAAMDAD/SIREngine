# Engine source

Native C++17 SIREngine sources are partitioned by role:

- `Core/`: platform-neutral foundation, memory, containers, system services and jobs.
- `Runtime/`: graphics, resources, world, animation, audio, physics, networking and scripting.
- `Editor/`: authoring application, scene editing, asset tools and inspection.
- `Developer/`: profiling, diagnostics and development-time services.
- `Programs/`: native applications and content tools.
- `Platform/`: Windows, Linux, and (after those ports) macOS-specific services.

The current compiled program is a minimal native lifecycle bootstrap in `Core/` and `Programs/Launcher/`. Other directories are reserved for actual ports and do not claim complete subsystems.
