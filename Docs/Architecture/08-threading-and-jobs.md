# 08 — Threading, queues and job lifecycle

**Status:** proposed contract.

SIREngine uses explicit job ownership and small stable work descriptors. The job system must accommodate rendering submission, resource I/O, asset decompression, physics preparation, editor tools and asynchronous audio requests with controlled priorities and backpressure.

## Scheduling

- Declare producers, consumers, capacity, ordering guarantees and cancellation semantics per queue.
- Prefer bounded ring buffers or fixed-capacity work lists on recurring hot paths. An unbounded queue is an explicit design decision with memory accounting.
- Worker jobs receive borrowed GRDG views guarded by a reader epoch; they may not mutate a published record.
- GRDG publication/eviction remains single-writer; worker completion posts compact commands/results to that owner.
- Resource lifetime extends past CPU jobs if GPU fences, physics steps or audio callbacks still reference the bytes.

## Shutdown and stalls

Shutdown is an ordered drain with bounded deadline and diagnostic evidence: stop input producers, cancel queued tasks, drain active readers and provider fences, reclaim retired payloads, then release allocator domains. A worker cannot retain borrowed memory past cancellation or teardown.

Instrument queue depths, steal attempts, time blocked, task time, wakeups, dropped work, retirement backlog and allocation counts. Test one-thread fallback as well as oversubscribed and low-core-count configurations. Avoid putting generic function wrappers or hidden heap captures in per-item hot queue operations.

## Verification

ThreadSanitizer and deterministic stress tests must exercise producer/consumer races, writer publication, cancellation, device loss, shutdown and reentrant callbacks. Lock freedom is a measured/proven property of individual queue operations, not a label applied to the entire engine.
