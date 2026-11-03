# Runtime subsystems

This directory contains built-in SIREngine runtime systems: Terrain, Rendering, Resources, World, Physics, Animation, Audio, Networking and Scripting. Each subsystem owns its first-party code and declares dependencies when it enters the native CMake build.

The current native executable implements only its bootstrap lifecycle. Terrain and the other subsystem directories mark source ownership and do not imply a completed runtime implementation.
