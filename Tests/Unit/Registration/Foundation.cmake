add_executable(SIRTerrainHeightfieldCodecTests SIRTerrainHeightfieldCodecTests.cpp)
target_link_libraries(SIRTerrainHeightfieldCodecTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.heightfield-codec COMMAND SIRTerrainHeightfieldCodecTests)
sir_configure_owned_target(SIRTerrainHeightfieldCodecTests)

add_executable(SIRTerrainRegionCatalogTests SIRTerrainRegionCatalogTests.cpp)
target_link_libraries(SIRTerrainRegionCatalogTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.region-catalog COMMAND SIRTerrainRegionCatalogTests)
sir_configure_owned_target(SIRTerrainRegionCatalogTests)

add_executable(SIRTerrainHeightfieldViewTests SIRTerrainHeightfieldViewTests.cpp)
target_link_libraries(SIRTerrainHeightfieldViewTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.heightfield-view COMMAND SIRTerrainHeightfieldViewTests)
sir_configure_owned_target(SIRTerrainHeightfieldViewTests)

add_executable(SIRTerrainResourceBridgeTests SIRTerrainResourceBridgeTests.cpp)
target_link_libraries(SIRTerrainResourceBridgeTests PRIVATE SIR::Terrain)
add_test(NAME terrain.resources.grdg-bridge COMMAND SIRTerrainResourceBridgeTests)
sir_configure_owned_target(SIRTerrainResourceBridgeTests)

add_executable(SIRTerrainRegionBundleTests SIRTerrainRegionBundleTests.cpp)
target_link_libraries(SIRTerrainRegionBundleTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.region-bundle COMMAND SIRTerrainRegionBundleTests)
sir_configure_owned_target(SIRTerrainRegionBundleTests)

add_executable(SIRTerrainRegionPersistenceTests SIRTerrainRegionPersistenceTests.cpp)
target_link_libraries(SIRTerrainRegionPersistenceTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.region-persistence COMMAND SIRTerrainRegionPersistenceTests)
sir_configure_owned_target(SIRTerrainRegionPersistenceTests)

add_executable(SIREngineLifecycleTests SIREngineLifecycleTests.cpp)
target_link_libraries(SIREngineLifecycleTests PRIVATE SIR::Core)
add_test(NAME core.application.system-lifecycle COMMAND SIREngineLifecycleTests)
sir_configure_owned_target(SIREngineLifecycleTests)

add_executable(SIRApplicationTests SIRApplicationTests.cpp)
target_link_libraries(SIRApplicationTests PRIVATE SIR::Core)
add_test(NAME core.application.lifecycle COMMAND SIRApplicationTests)
add_test(NAME launcher.version COMMAND SIREngine --version)
set_tests_properties(launcher.version PROPERTIES PASS_REGULAR_EXPRESSION "SIREngine native foundation")
add_test(NAME launcher.help COMMAND SIREngine --help)
set_tests_properties(launcher.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: SIREngine")
add_test(NAME launcher.unknown-option COMMAND SIREngine --unknown)
set_tests_properties(launcher.unknown-option PROPERTIES WILL_FAIL TRUE)

sir_configure_owned_target(SIRApplicationTests)

add_library(SIRPlatformTestModule SHARED SIRDynamicLibraryFixture.cpp)
sir_configure_owned_target(SIRPlatformTestModule)

add_executable(SIRSystemTests SIRSystemTests.cpp)
add_dependencies(SIRSystemTests SIRPlatformTestModule)
target_compile_definitions(SIRSystemTests PRIVATE SIR_TEST_MODULE_PATH="$<TARGET_FILE:SIRPlatformTestModule>")
target_link_libraries(SIRSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.lifecycle COMMAND SIRSystemTests)
sir_configure_owned_target(SIRSystemTests)

add_executable(SIRMemoryManagerTests SIRMemoryManagerTests.cpp)
target_link_libraries(SIRMemoryManagerTests PRIVATE SIR::Core)
add_test(NAME core.memory.manager COMMAND SIRMemoryManagerTests)
sir_configure_owned_target(SIRMemoryManagerTests)

# Source-coverage obligations for preflight-only memory reservations, budgets,
# allocator metadata and rejection without caller-output mutation.
add_executable(SIRMemoryLayoutCoverageTests SIRMemoryLayoutCoverageTests.cpp)
target_link_libraries(SIRMemoryLayoutCoverageTests PRIVATE SIR::Core)
add_test(NAME core.memory.layout-preflight-negative COMMAND SIRMemoryLayoutCoverageTests)
sir_configure_owned_target(SIRMemoryLayoutCoverageTests)

# Real owner-managed pool pressure, pending reclamation and rollback boundaries.
add_executable(SIRMemoryPressureBoundaryTests SIRMemoryPressureBoundaryTests.cpp)
target_link_libraries(SIRMemoryPressureBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.pressure-boundaries COMMAND SIRMemoryPressureBoundaryTests)
sir_configure_owned_target(SIRMemoryPressureBoundaryTests)

# Guard-page stack bookmarks, reset states and all memory release policies.
add_executable(SIRVirtualStackBoundaryTests SIRVirtualStackBoundaryTests.cpp)
target_link_libraries(SIRVirtualStackBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.virtual-stack-boundaries COMMAND SIRVirtualStackBoundaryTests)
sir_configure_owned_target(SIRVirtualStackBoundaryTests)

# Exercise all compact bucket class profiles and negative admission contracts.
add_executable(SIRCompactBucketBoundaryTests SIRCompactBucketBoundaryTests.cpp)
target_link_libraries(SIRCompactBucketBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.compact-bucket-boundaries COMMAND SIRCompactBucketBoundaryTests)
sir_configure_owned_target(SIRCompactBucketBoundaryTests)

# AVL extent rotations, stable handle generations, compaction and fragmentation.
add_executable(SIRHandleHeapBoundaryTests SIRHandleHeapBoundaryTests.cpp)
target_link_libraries(SIRHandleHeapBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.handle-heap-fragmentation COMMAND SIRHandleHeapBoundaryTests)
sir_configure_owned_target(SIRHandleHeapBoundaryTests)

# Hierarchical free-bitmaps, slot reuse and payload-preserving page trimming.
add_executable(SIRBitmapBoundaryTests SIRBitmapBoundaryTests.cpp)
target_link_libraries(SIRBitmapBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.bitmap-boundaries COMMAND SIRBitmapBoundaryTests)
sir_configure_owned_target(SIRBitmapBoundaryTests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRMemoryManagerFaultInjectionTests SIRMemoryManagerFaultInjectionTests.cpp)
    target_link_libraries(SIRMemoryManagerFaultInjectionTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS mmap mprotect madvise sysconf)
        target_link_options(SIRMemoryManagerFaultInjectionTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.memory.virtual-fault-injection COMMAND SIRMemoryManagerFaultInjectionTests)
    sir_configure_owned_target(SIRMemoryManagerFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRVirtualMemoryRollbackTests SIRVirtualMemoryRollbackTests.cpp)
    target_link_libraries(SIRVirtualMemoryRollbackTests PRIVATE SIR::Core)
    target_link_options(SIRVirtualMemoryRollbackTests PRIVATE "LINKER:--wrap=madvise")
    add_test(NAME core.platform.decommit-failure COMMAND SIRVirtualMemoryRollbackTests)
    sir_configure_owned_target(SIRVirtualMemoryRollbackTests)
endif()

if(SIR_WITH_GOOGLETEST)
    add_executable(SIRGoogleTests GoogleApplicationTests.cpp)
    target_compile_features(SIRGoogleTests PRIVATE cxx_std_17)
    target_link_libraries(SIRGoogleTests PRIVATE SIR::Core gtest_main)
    add_test(NAME core.application.googletest COMMAND SIRGoogleTests)
    sir_configure_owned_target(SIRGoogleTests)
endif()

# Validate the standalone CMake profiler comparison without vendor libraries.
add_test(NAME profiling.benchmark-comparison
    COMMAND "${CMAKE_COMMAND}"
        "-DSIR_ROOT=${PROJECT_SOURCE_DIR}"
        "-DSIR_TEST_ROOT=${PROJECT_BINARY_DIR}/RegressionSmoke"
        -P "${PROJECT_SOURCE_DIR}/Tests/Benchmarks/GoogleBenchmarkCompareTests.cmake")

add_executable(SIRThreadingPrimitiveTests SIRThreadingPrimitiveTests.cpp)
target_link_libraries(SIRThreadingPrimitiveTests PRIVATE SIR::Core)
# Clang's libstdc++ implementation of std::atomic::is_lock_free() calls
# __atomic_is_lock_free, supplied by libatomic on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(SIRThreadingPrimitiveTests PRIVATE atomic)
endif()
add_test(NAME core.threading.primitives COMMAND SIRThreadingPrimitiveTests)
sir_configure_owned_target(SIRThreadingPrimitiveTests)

include(${CMAKE_CURRENT_SOURCE_DIR}/SIRAllocatorMigration.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/EventTests.cmake)
add_executable(SIRContainerTests SIRContainerTests.cpp)
target_link_libraries(SIRContainerTests PRIVATE SIR::Core)
add_test(NAME core.containers.basic COMMAND SIRContainerTests)
sir_configure_owned_target(SIRContainerTests)

add_executable(SIRContainerDifferentialTests SIRContainerDifferentialTests.cpp)
target_link_libraries(SIRContainerDifferentialTests PRIVATE SIR::Core)
add_test(NAME core.containers.differential COMMAND SIRContainerDifferentialTests)
sir_configure_owned_target(SIRContainerDifferentialTests)

add_executable(SIRContainerExtendedTests SIRContainerExtendedTests.cpp)
target_link_libraries(SIRContainerExtendedTests PRIVATE SIR::Core)
add_test(NAME core.containers.extended COMMAND SIRContainerExtendedTests)
sir_configure_owned_target(SIRContainerExtendedTests)

add_executable(SIRContainerOptimizationTests SIRContainerOptimizationTests.cpp)
target_link_libraries(SIRContainerOptimizationTests PRIVATE SIR::Core)
add_test(NAME core.containers.optimization COMMAND SIRContainerOptimizationTests)
sir_configure_owned_target(SIRContainerOptimizationTests)

add_executable(SIRContainerMemoryTests SIRContainerMemoryTests.cpp)
target_link_libraries(SIRContainerMemoryTests PRIVATE SIR::Core)
add_test(NAME core.containers.memory COMMAND SIRContainerMemoryTests)
sir_configure_owned_target(SIRContainerMemoryTests)

add_executable(SIRContainerHeapAdapterTests SIRContainerHeapAdapterTests.cpp)
target_link_libraries(SIRContainerHeapAdapterTests PRIVATE SIR::Core)
add_test(NAME core.containers.heap-adapter COMMAND SIRContainerHeapAdapterTests)
sir_configure_owned_target(SIRContainerHeapAdapterTests)

