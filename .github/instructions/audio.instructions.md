---
name: SIR audio adapters
description: Protect SIR audio backend contracts and vendor integration boundaries.
applyTo: 'Source/Audio/**,Build/CMake/SIRAudio.cmake,Tests/**/*Audio*'
---
# Audio

- Preserve the first-party audio abstraction; keep FMOD/Wwise/OpenAL SDK integrations optional and isolated.
- Do not invent vendor event IDs, bank names, asset paths or licensing capabilities.
- Keep realtime audio callback paths allocation-free and lock-free where established. Explain buffer ownership, channels, threading, sample formats and shutdown order.
- Test generic audio code without requiring an unavailable licensed SDK; report which backend was actually exercised.
