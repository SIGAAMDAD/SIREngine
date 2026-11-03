# 04 - Containers and allocation policy

**Status:** EASTL-family repositories pinned; native container integration is scheduled for the framework-port milestone.

SIREngine targets C++17 and plans to use EASTL for suitable contiguous and associative containers. Exact allocator behavior is determined per owned subsystem, with tracing and runtime budget enforcement rather than a single heap policy for every container.

The former experimental `SIRConstants.h`, `SIRContainers.h` and `containers_smoke.cpp` bootstrap artifacts have been removed from the new source tree. They did not implement centralized allocation or an accepted shared API. There are no engine-wide `SIRList`, `SIRHashMap`, `SIRString` or `SIRArray` public aliases in this baseline.

The pinned `ThirdParty/EASTL`, `EABase` and `EAAssert` sources are available for opted-in CMake builds with `SIR_WITH_EASTL=ON`. The native Core does not depend on EASTL until selected framework components are ported. A new public container/allocator API should be based on the source-grounded NomadFramework inventory and introduced with real consumers, memory tracing, and native tests.

Hot storage should be compact, with stable lifetimes and predictable growth or fixed capacities where needed. Avoid introducing a container adapter that disguises untracked heap allocations, iterator invalidation or resource ownership.
