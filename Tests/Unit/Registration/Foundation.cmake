add_executable(SIRTerrainHeightfieldCodecTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTerrainHeightfieldCodecTests.cpp)
target_link_libraries(SIRTerrainHeightfieldCodecTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.heightfield-codec COMMAND SIRTerrainHeightfieldCodecTests)
sir_configure_owned_target(SIRTerrainHeightfieldCodecTests)

add_executable(SIRTerrainRegionCatalogTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTerrainRegionCatalogTests.cpp)
target_link_libraries(SIRTerrainRegionCatalogTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.region-catalog COMMAND SIRTerrainRegionCatalogTests)
sir_configure_owned_target(SIRTerrainRegionCatalogTests)

add_executable(SIRTerrainHeightfieldViewTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTerrainHeightfieldViewTests.cpp)
target_link_libraries(SIRTerrainHeightfieldViewTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.heightfield-view COMMAND SIRTerrainHeightfieldViewTests)
sir_configure_owned_target(SIRTerrainHeightfieldViewTests)

add_executable(SIRTerrainResourceBridgeTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTerrainResourceBridgeTests.cpp)
target_link_libraries(SIRTerrainResourceBridgeTests PRIVATE SIR::Terrain)
add_test(NAME terrain.resources.grdg-bridge COMMAND SIRTerrainResourceBridgeTests)
sir_configure_owned_target(SIRTerrainResourceBridgeTests)

add_executable(SIRTerrainRegionBundleTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTerrainRegionBundleTests.cpp)
target_link_libraries(SIRTerrainRegionBundleTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.region-bundle COMMAND SIRTerrainRegionBundleTests)
sir_configure_owned_target(SIRTerrainRegionBundleTests)

add_executable(SIRTerrainRegionPersistenceTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRTerrainRegionPersistenceTests.cpp)
target_link_libraries(SIRTerrainRegionPersistenceTests PRIVATE SIR::Terrain)
add_test(NAME terrain.data.region-persistence COMMAND SIRTerrainRegionPersistenceTests)
sir_configure_owned_target(SIRTerrainRegionPersistenceTests)

add_executable(SIREngineLifecycleTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIREngineLifecycleTests.cpp)
target_link_libraries(SIREngineLifecycleTests PRIVATE SIR::Core)
add_test(NAME core.application.system-lifecycle COMMAND SIREngineLifecycleTests)
sir_configure_owned_target(SIREngineLifecycleTests)

add_executable(SIRApplicationTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRApplicationTests.cpp)
target_link_libraries(SIRApplicationTests PRIVATE SIR::Core)
add_test(NAME core.application.lifecycle COMMAND SIRApplicationTests)
add_test(NAME launcher.version COMMAND SIREngine --version)
set_tests_properties(launcher.version PROPERTIES PASS_REGULAR_EXPRESSION "SIREngine native foundation")
add_test(NAME launcher.help COMMAND SIREngine --help)
set_tests_properties(launcher.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: SIREngine")
add_test(NAME launcher.unknown-option COMMAND SIREngine --unknown)
set_tests_properties(launcher.unknown-option PROPERTIES WILL_FAIL TRUE)

sir_configure_owned_target(SIRApplicationTests)

add_library(SIRPlatformTestModule SHARED ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRDynamicLibraryFixture.cpp)
sir_configure_owned_target(SIRPlatformTestModule)

add_executable(SIRSystemTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRSystemTests.cpp)
add_dependencies(SIRSystemTests SIRPlatformTestModule)
target_compile_definitions(SIRSystemTests PRIVATE SIR_TEST_MODULE_PATH="$<TARGET_FILE:SIRPlatformTestModule>")
target_link_libraries(SIRSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.lifecycle COMMAND SIRSystemTests)
sir_configure_owned_target(SIRSystemTests)

add_executable(SIRMemoryManagerTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRMemoryManagerTests.cpp)
target_link_libraries(SIRMemoryManagerTests PRIVATE SIR::Core)
add_test(NAME core.memory.manager COMMAND SIRMemoryManagerTests)
sir_configure_owned_target(SIRMemoryManagerTests)

# Source-coverage obligations for preflight-only memory reservations, budgets,
# allocator metadata and rejection without caller-output mutation.
add_executable(SIRMemoryLayoutCoverageTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRMemoryLayoutCoverageTests.cpp)
target_link_libraries(SIRMemoryLayoutCoverageTests PRIVATE SIR::Core)
add_test(NAME core.memory.layout-preflight-negative COMMAND SIRMemoryLayoutCoverageTests)
sir_configure_owned_target(SIRMemoryLayoutCoverageTests)

# Real owner-managed pool pressure, pending reclamation and rollback boundaries.
add_executable(SIRMemoryPressureBoundaryTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRMemoryPressureBoundaryTests.cpp)
target_link_libraries(SIRMemoryPressureBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.pressure-boundaries COMMAND SIRMemoryPressureBoundaryTests)
sir_configure_owned_target(SIRMemoryPressureBoundaryTests)

# Guard-page stack bookmarks, reset states and all memory release policies.
add_executable(SIRVirtualStackBoundaryTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRVirtualStackBoundaryTests.cpp)
target_link_libraries(SIRVirtualStackBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.virtual-stack-boundaries COMMAND SIRVirtualStackBoundaryTests)
sir_configure_owned_target(SIRVirtualStackBoundaryTests)

# Exercise all compact bucket class profiles and negative admission contracts.
add_executable(SIRCompactBucketBoundaryTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRCompactBucketBoundaryTests.cpp)
target_link_libraries(SIRCompactBucketBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.compact-bucket-boundaries COMMAND SIRCompactBucketBoundaryTests)
sir_configure_owned_target(SIRCompactBucketBoundaryTests)

# AVL extent rotations, stable handle generations, compaction and fragmentation.
add_executable(SIRHandleHeapBoundaryTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRHandleHeapBoundaryTests.cpp)
target_link_libraries(SIRHandleHeapBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.handle-heap-fragmentation COMMAND SIRHandleHeapBoundaryTests)
sir_configure_owned_target(SIRHandleHeapBoundaryTests)

# Hierarchical free-bitmaps, slot reuse and payload-preserving page trimming.
add_executable(SIRBitmapBoundaryTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRBitmapBoundaryTests.cpp)
target_link_libraries(SIRBitmapBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.memory.bitmap-boundaries COMMAND SIRBitmapBoundaryTests)
sir_configure_owned_target(SIRBitmapBoundaryTests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRMemoryManagerFaultInjectionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRMemoryManagerFaultInjectionTests.cpp)
    target_link_libraries(SIRMemoryManagerFaultInjectionTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS mmap mprotect madvise sysconf)
        target_link_options(SIRMemoryManagerFaultInjectionTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.memory.virtual-fault-injection COMMAND SIRMemoryManagerFaultInjectionTests)
    sir_configure_owned_target(SIRMemoryManagerFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRVirtualMemoryRollbackTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRVirtualMemoryRollbackTests.cpp)
    target_link_libraries(SIRVirtualMemoryRollbackTests PRIVATE SIR::Core)
    target_link_options(SIRVirtualMemoryRollbackTests PRIVATE "LINKER:--wrap=madvise")
    add_test(NAME core.platform.decommit-failure COMMAND SIRVirtualMemoryRollbackTests)
    sir_configure_owned_target(SIRVirtualMemoryRollbackTests)
endif()

add_executable(SIRThreadingPrimitiveTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRThreadingPrimitiveTests.cpp)
target_link_libraries(SIRThreadingPrimitiveTests PRIVATE SIR::Core)
# Clang's libstdc++ implementation of std::atomic::is_lock_free() calls
# __atomic_is_lock_free, supplied by libatomic on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(SIRThreadingPrimitiveTests PRIVATE atomic)
endif()
add_test(NAME core.threading.primitives COMMAND SIRThreadingPrimitiveTests)
sir_configure_owned_target(SIRThreadingPrimitiveTests)

include(${PROJECT_SOURCE_DIR}/Tests/Unit/SIRAllocatorMigration.cmake)
include(${PROJECT_SOURCE_DIR}/Tests/Unit/EventTests.cmake)
add_executable(SIRContainerTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRContainerTests.cpp)
target_link_libraries(SIRContainerTests PRIVATE SIR::Core)
add_test(NAME core.containers.basic COMMAND SIRContainerTests)
sir_configure_owned_target(SIRContainerTests)

add_executable(SIRContainerDifferentialTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRContainerDifferentialTests.cpp)
target_link_libraries(SIRContainerDifferentialTests PRIVATE SIR::Core)
add_test(NAME core.containers.differential COMMAND SIRContainerDifferentialTests)
sir_configure_owned_target(SIRContainerDifferentialTests)

add_executable(SIRContainerExtendedTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRContainerExtendedTests.cpp)
target_link_libraries(SIRContainerExtendedTests PRIVATE SIR::Core)
add_test(NAME core.containers.extended COMMAND SIRContainerExtendedTests)
sir_configure_owned_target(SIRContainerExtendedTests)

add_executable(SIRContainerOptimizationTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRContainerOptimizationTests.cpp)
target_link_libraries(SIRContainerOptimizationTests PRIVATE SIR::Core)
add_test(NAME core.containers.optimization COMMAND SIRContainerOptimizationTests)
sir_configure_owned_target(SIRContainerOptimizationTests)

add_executable(SIRContainerMemoryTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRContainerMemoryTests.cpp)
target_link_libraries(SIRContainerMemoryTests PRIVATE SIR::Core)
add_test(NAME core.containers.memory COMMAND SIRContainerMemoryTests)
sir_configure_owned_target(SIRContainerMemoryTests)

add_executable(SIRContainerHeapAdapterTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRContainerHeapAdapterTests.cpp)
target_link_libraries(SIRContainerHeapAdapterTests PRIVATE SIR::Core)
add_test(NAME core.containers.heap-adapter COMMAND SIRContainerHeapAdapterTests)
sir_configure_owned_target(SIRContainerHeapAdapterTests)

