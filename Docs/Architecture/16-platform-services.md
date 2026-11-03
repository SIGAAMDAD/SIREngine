# 16 - Native platform services

**Implementation:** Windows/Linux runtime foundation, split by responsibility. C++17, global namespace. Most platform interfaces keep OS includes in implementations; the low-level thread and synchronization headers retain native handle types pending an ABI-preserving abstraction. The existing `SIRPlatformUtils` method names remain a thin transitional ABI facade; new consumers include only the owning service header.

| Class | Ownership | Windows translation unit | Linux translation unit |
| --- | --- | --- | --- |
| `SIRVirtualMemory` | OS reservations, page protection, commit and decommit | `Windows/SIRVirtualMemoryWindows.cpp` | `Linux/SIRVirtualMemoryLinux.cpp` |
| `SIRDynamicLibrary` | Module handle, symbol query, unload and last loader error | `Windows/SIRDynamicLibraryWindows.cpp` | `Linux/SIRDynamicLibraryLinux.cpp` |
| `SIRPlatformTime` | Monotonic clock in nanoseconds | `Windows/SIRPlatformTimeWindows.cpp` | `Linux/SIRPlatformTimeLinux.cpp` |
| `SIRPlatformProcess` | Logical CPU count, spin hint, yield and sleep | `Windows/SIRPlatformProcessWindows.cpp` | `Linux/SIRPlatformProcessLinux.cpp` |
| `SIRPlatformUtils` | Compatibility only; forwards to the four owners | Common source | Common source |

## UE5 source references used

The provided UE5 `Runtime.zip` includes `Runtime/Core/Private/Windows/WindowsPlatformMemory.cpp`, `WindowsPlatformProcess.cpp`, `WindowsPlatformTime.cpp`, `Runtime/Core/Private/Unix/UnixPlatformMemory.cpp`, `UnixPlatformProcess.cpp` and the corresponding `Linux` platform files. Those were consulted for the **implementation responsibilities and operating-system facilities**: separate memory/process/time interfaces; virtual reservation versus commitment; Windows `LoadLibrary` and symbol-resolution behavior; Unix `dlopen`/`dlsym`/`dlclose`; and one-time high-resolution clock calibration. SIREngine implementations are independently written against Windows and POSIX OS APIs. UE5 engine source is not imported, and its types, module metadata, memory accounting, process startup, and platform headers do not become SIREngine dependencies.

## Contracts and limits

- Virtual memory uses page-aligned sizes; Windows `VirtualFree(MEM_RELEASE)` releases the original allocation regardless of the supplied byte count, while Linux `munmap` consumes the explicitly supplied range. `SIRMemoryManager` owns its reservation sizes/identities and must not infer portable partial-release semantics.
- Page commit, decommit and page protection operate only on reservations controlled by the memory manager. Linux decommit revokes access and advises the kernel to discard backing pages; kernel physical-residency accounting remains separately observable.
- Library loading is explicit, caller-owned and unrefcounted by this wrapper. Windows uses UTF-8 conversion and modern DLL search flags; Unix uses `RTLD_NOW | RTLD_LOCAL`. Symbol pointers must not be used after unload. Query errors on the same thread immediately after the failing operation; the error state is not a persistent field of the handle.
- Windows monotonic timing uses cached `QueryPerformanceFrequency`; Linux uses `CLOCK_MONOTONIC`. Nanosecond conversion is integer arithmetic. CPU counts reflect online logical processors, not job-affinity settings.
- The facade is retained until all NomadFramework and SIREngine callers have migrated to the focused API. Do not duplicate its implementations or add new APIs there.

## Verification

`SIRSystemTests` builds and loads a dedicated shared-library fixture, then exercises the direct class APIs and legacy forwarding methods: page reservation and invalid requests, partial commit/decommit, protection validation, module load/symbol lookup/close/error, clock monotonicity, processor hints, thread lifecycle and synchronization. Linux GCC/Clang and sanitizer runs are local gates. Windows MSVC compilation, native tests and the repository-pinned formatter remain mandatory acceptance steps before publication to the engine branch.


## OS diagnostics: memory, CPU and host information

The platform layer additionally exposes three independent, allocation-free-to-call-site
snapshot interfaces (system libc and OS calls may allocate internally):

- `SIRPlatformMemory::querySystem` returns host physical and swap memory and, when
  supported, commit totals and cgroup/Windows Job constraints. `nValidFields`
  identifies optional measurements. Linux's physical available value is
  `/proc/meminfo:MemAvailable` where supplied, falling back to conservative free
  memory; Windows uses `GlobalMemoryStatusEx`. Windows `GetPerformanceInfo` reports
  commit totals; these are **not** pagefile or swap totals. Linux uses `sysinfo`
  for swap and reads its commit estimates from `/proc/meminfo`. The optional
  Linux cgroup v2 snapshot currently checks `/sys/fs/cgroup` as mounted for
  the process only when the cgroup namespace reports a root membership;
  custom mount mappings remain unreported rather than returning unrelated limits.
- `SIRPlatformMemory::queryProcess` observes resident/peak memory and page faults.
  Windows obtains private commit from `PROCESS_MEMORY_COUNTERS_EX`; Linux obtains
  resident, anonymous resident and virtual address size from `/proc/self/status`.
  Linux does **not** label anonymous resident bytes as private commit. Windows
  process virtual-address size and Linux private commit remain unavailable,
  indicated by their missing validity bits. The process footprint is not a
  substitute for SIRMemoryManager's own domain-level requested/committed/resident accounting.
- `SIRCPUInfo::query` returns CPUID vendor/brand, usable instruction sets (AVX
  feature bits are suppressed if XCR0 does not enable their register state),
  cache sizes per cache instance and logical/physical core counts. Linux counts
  processors in the process affinity mask and resolves package/core IDs via sysfs;
  Windows uses GetActiveProcessorCount and GetLogicalProcessorInformationEx.
  An unavailable physical-core count remains zero, not an estimated SMT ratio.
- `SIRSystemInfo::query` provides UTF-8 host/OS names, native process ID,
  kernel/OS version, uptime, architecture and virtual-memory page granularity.
  Windows derives the build version from RtlGetVersion when available.

Each snapshot is written to the output only on success. These functions are
intended for startup hardware reports, memory-pressure diagnostics, editor
statistics and profiler telemetry, not allocation bookkeeping on hot paths.
Windows Job and Linux cgroup limits are group constraints, which can include
other processes; their reported usage must not be attributed solely to SIREngine.
