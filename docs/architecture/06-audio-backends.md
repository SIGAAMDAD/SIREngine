# 06 — Audio provider architecture

**Status:** dependencies declared; FMOD provider integration planned first, OpenAL and Wwise follow.

The public SIR audio interface owns event/voice handles, scheduling requests, bus routing metadata and decoded-resource identity. It does not expose FMOD or Wwise SDK types in core headers. Only one audio provider is active per engine runtime configuration, unless explicit interoperability is introduced and measured.

| Provider | Dependency | Target order | Current status |
| --- | --- | --- | --- |
| FMOD Studio | External licensed FMOD Studio API | First | Optional SDK resolution via CMake only |
| OpenAL | System OpenAL package | Later | `find_package(OpenAL)` entrypoint |
| Wwise | External licensed Wwise SDK | Later | Optional header SDK target declaration |

## Memory and streaming

FMOD allocation callbacks must be routed to the audio domain of NMDMemory before claiming budget compliance. Profile Studio banks, events, sample streaming, DSP state, voice counts, callback threading and bank unload. The game's soundscape policy selects banks, event counts, voice limits and wartapes processing; those are not fixed engine global constants.

Provider-resident audio bytes and transformed/decoded buffers are derived resources in GRDG. Bank metadata has one canonical owner. Preserve predictable audio callback timing: audio-thread code must not perform unbounded allocation, file I/O or resource graph mutation.

## APIs and failure behavior

Backend-independent calls include bank load/unload, event start/stop, parameter update, group/bus volume, listener transform, device selection and async readiness. The provider translates handles and reports specific errors and allocation deltas; the public layer governs lifecycle and time stamps.

## Acceptance

Compile a minimal provider on each supported OS, run device enumeration and silent-headless tests, load representative banks/events, exercise bank eviction and callback allocation traces, and measure CPU/committed memory at low and peak voice loads. Verify license/API redistribution separately at packaging time.
