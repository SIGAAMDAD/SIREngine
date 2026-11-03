# Wave A: portable audio contracts

The owned audio interface is separate from FMOD/Wwise/OpenAL provider details.
The backend ABI is version 1. Audio commands and completions are fixed POD
messages; no provider SDK headers are part of exported interfaces.

Playback identity retains 32-bit slot, 32-bit generation and 64-bit epoch.
SIRAudioSystem uses one logical owner thread for playback handle changes and
completion processing. Global/emitter parameter commands may have multiple
producers, with producers quiesced before shutdown.

A bounded MPMC queue is used for submission. A dedicated critical queue
reserves capacity for stop requests. Full queues explicitly return
SIR_AUDIO_CAPACITY; accepted commands are never silently overwritten.
The subsystem allocates its slot tables and queues through a required
caller-provided sirContainerAllocator_t, allowing SIR_MEMORY_DOMAIN_AUDIO
binding via SIRContainerHeapAdapter.

SIRAudioMockBackend supplies deterministic playback and failure completions.
The mock is a test fixture only, not an audio playback provider. FMOD context,
event instance ownership, bank residency and SDK/device tests are later waves.

All API calls that mutate logical playing handles are owner-thread-only.
Shutdown requires every producer and backend callback to be quiesced.
