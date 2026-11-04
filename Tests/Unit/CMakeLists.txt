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

# Shared renderer contract tests run on Windows/Linux without a GPU or native driver.
add_executable(SIRGraphicsInterfaceTests SIRGraphicsInterfaceTests.cpp)
target_link_libraries(SIRGraphicsInterfaceTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.interface COMMAND SIRGraphicsInterfaceTests)
sir_configure_owned_target(SIRGraphicsInterfaceTests)


if(SIR_WITH_VULKAN)
    add_executable(SIRVulkanDeviceTests SIRVulkanDeviceTests.cpp)
    target_link_libraries(SIRVulkanDeviceTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.headless COMMAND SIRVulkanDeviceTests)
    set_tests_properties(render.vulkan.headless PROPERTIES SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRVulkanDeviceTests)

    add_executable(SIRVulkanContractTests SIRVulkanContractTests.cpp)
    target_link_libraries(SIRVulkanContractTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.contracts COMMAND SIRVulkanContractTests)
    sir_configure_owned_target(SIRVulkanContractTests)

    add_executable(SIRVulkanSharedContractTests SIRVulkanSharedContractTests.cpp)
    target_link_libraries(SIRVulkanSharedContractTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.shared-contracts COMMAND SIRVulkanSharedContractTests)
    sir_configure_owned_target(SIRVulkanSharedContractTests)

    add_executable(SIRVulkanBarrierTests SIRVulkanBarrierTests.cpp)
    target_link_libraries(SIRVulkanBarrierTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.barriers COMMAND SIRVulkanBarrierTests)
    sir_configure_owned_target(SIRVulkanBarrierTests)

    add_executable(SIRVulkanTransferLayoutTests SIRVulkanTransferLayoutTests.cpp)
    target_link_libraries(SIRVulkanTransferLayoutTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.transfer-layout COMMAND SIRVulkanTransferLayoutTests)
    sir_configure_owned_target(SIRVulkanTransferLayoutTests)

    add_executable(SIRVulkanRecordedCommandsTests SIRVulkanRecordedCommandsTests.cpp)
    target_link_libraries(SIRVulkanRecordedCommandsTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.recorded-commands COMMAND SIRVulkanRecordedCommandsTests)
    sir_configure_owned_target(SIRVulkanRecordedCommandsTests)

    add_executable(SIRVulkanCopyBatchTests SIRVulkanCopyBatchTests.cpp)
    target_link_libraries(SIRVulkanCopyBatchTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.copy-batch COMMAND SIRVulkanCopyBatchTests)
    sir_configure_owned_target(SIRVulkanCopyBatchTests)

    add_executable(SIRVulkanSharedTests SIRVulkanSharedTests.cpp)
    target_link_libraries(SIRVulkanSharedTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.shared-api COMMAND SIRVulkanSharedTests)
    set_tests_properties(render.vulkan.shared-api PROPERTIES SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRVulkanSharedTests)
endif()
