# 17 — Core threading primitives

**Status:** `SIRAtomic`, `SIRAtomicFlag`, `SIRScopedLock`, and `SIRConditionalScopedLock` are ported from the original NomadFramework API into the SIREngine Core layer. The already integrated `SIRThread`, `SIRMutex`, `SIRRecursiveMutex`, `SIRConditionVariable`, and `SIRSpinLock` remain their existing Core implementations.

## Atomic ownership and representation

- `Source/Core/Platform/SIRAtomic.h` implements an allocation-free, fixed-size, lock-free `SIRAtomic<T>` for arithmetic, enumeration, and pointer values of 1, 2, 4, or 8 bytes. Unsupported or non-lock-free targets fail at compile time.
- Atomics use C++17 `std::atomic<T>` for compiler-specific operations. This replaces the original 46 KiB compiler-intrinsic selection layer without changing the public `load`, `store`, `exchange`, compare-exchange, arithmetic/bitwise fetch, or pointer-offset API. A floating-point fetch addition/subtraction uses a weak-CAS retry loop in C++17.
- Memory order is an explicitly sized `sirMemoryOrder_t`, with original numeric values and SIREngine enumerator names; translation to native `std::memory_order` is explicit and avoids assumptions about the standard library's enum representation.
- The compare-exchange failure order is normalized to a valid strength at or below the success order, with release/acq_rel failure modes rewritten to relaxed/acquire. Invalid load/store combinations are normalized to sequential consistency.
- No reference ownership or reclamation algorithm is implied by `SIRAtomic`. GRDG publication and reader reclamation require their own owner-approved protocols.

## Synchronization and scoped ownership

- `SIRAtomicFlag` is implemented in C++17 using `SIRAtomic<bool>`; unlike C++20-only `std::atomic_flag::test`, it supports test, test-and-set and clear in the engine's target language.
- `SIRSpinLock` now shares that atomic flag and uses test-test-and-set, a CPU-relax loop, and a bounded thread yield when contended. It is appropriate for brief critical sections, not blocking waits.
- `SIRScopedLock<Lockable>` and `SIRConditionalScopedLock<Lockable>` enforce lexical ownership and prohibit copying/moving. `Lockable` must expose nonthrowing `lock()` and `unlock()` methods and must outlive the guard.
- Existing native mutex, recursive mutex, thread, and condition-variable classes retain their Windows and POSIX implementations and lifecycle semantics.

## Verification

`Tests/Unit/SIRThreadingPrimitiveTests.cpp` covers order mapping, CAS expected-value updates, primitive arithmetic/bitwise operations, pointer scaling, boolean/enum/floating atomics, flag transitions, locked/conditional RAII, multithread atomic counters, and spinlock critical-section exclusivity. Windows and Linux native CI must pass before merge.
