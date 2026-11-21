---
name: SIR platform boundaries
description: Apply platform and compiler isolation for first-party engine code.
applyTo: 'Source/Core/Platform/**,Build/CMake/**,Source/Main.cpp'
---
# Platform and compiler boundaries

- Keep platform implementations under `Source/Core/Platform/` or an existing explicitly owned backend directory. Shared code communicates through stable interfaces.
- Preserve Windows, Linux/X11/Wayland and other platform support exactly as implemented; never claim unsupported targets were tested.
- Guard backend code with existing feature flags and capability detection; avoid assuming a single graphics driver, OS, or SDK.
- Require real compiler/link evidence for requested Windows/MSVC, Linux/GCC/Clang, and optional platform targets. Verify configured compiler standard and compiler extensions per target.
- Do not copy vendor SDK binaries or headers into the repository. Keep third-party source attribution and ABI requirements.
