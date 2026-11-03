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

## Command and completion guarantees

- Each accepted play/stop command receives an engine sequence number.
  Only matching play-start, play-failure, stop, and natural-finish records
  may transition the corresponding logical playback state.
- Reused playback slots increment their generation; restarting the audio
  system changes its epoch. Old completions and commands cannot mutate
  the new logical handle.
- A failed native **play** submission marks the occurrence failed. A failed
  native **stop** submission restores the previous state so the caller can
  retry. Failures of parameter commands do not invalidate an active voice.
- The normal queue and reserved stop queue are each preallocated,
  capacity-bounded, and processed with finite per-update budgets. If
  a stop cannot be queued, the API returns SIR_AUDIO_CAPACITY and leaves
  its state unchanged; the caller owns the retry.
- A backend returning an error from submit must not retain the borrowed
  command pointer or create native ownership that the error leaves
  unaccounted. Successful asynchronous submissions must ultimately
  report their corresponding completion. Callbacks submit fixed-size
  completion records; only the logical owner thread updates handle slots.
- Native providers must drain completions without silent loss. Wave A
  drains after each dispatch to exercise small-capacity completion queues;
  later providers may batch only with equivalent capacity guarantees.
- The 64-byte command, 32-byte completion, and 16-byte handle layouts are
  locked to ABI version 1 by compile-time checks.

## Scope remaining after Wave A contract and mock tests

Emitter/listener registration and spatial payload transport, cooked
package manifests, FMOD native lifetime, bank residency and device
recovery are not implemented by this contract-only provider. No
FMOD/Wwise/OpenAL device playback is represented by mock test success.
