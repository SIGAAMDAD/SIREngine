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
add_executable(SIRGraphicsABITests SIRGraphicsABITests.cpp)
target_link_libraries(SIRGraphicsABITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.binary-abi COMMAND SIRGraphicsABITests)
sir_configure_owned_target(SIRGraphicsABITests)

add_executable(SIRGraphicsPipelineABITests SIRGraphicsPipelineABITests.cpp)
target_link_libraries(SIRGraphicsPipelineABITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.pipeline-eras COMMAND SIRGraphicsPipelineABITests)
sir_configure_owned_target(SIRGraphicsPipelineABITests)

add_executable(SIRGraphicsPipelineBridgeTests SIRGraphicsPipelineBridgeTests.cpp)
target_link_libraries(SIRGraphicsPipelineBridgeTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.pipeline-bridge COMMAND SIRGraphicsPipelineBridgeTests)
sir_configure_owned_target(SIRGraphicsPipelineBridgeTests)

add_library(SIRGraphicsABIMockModule SHARED SIRGraphicsABIMockModule.cpp)
target_link_libraries(SIRGraphicsABIMockModule PRIVATE SIR::Graphics)
sir_configure_owned_target(SIRGraphicsABIMockModule)

add_executable(SIRGraphicsModuleTests SIRGraphicsModuleTests.cpp)
target_link_libraries(SIRGraphicsModuleTests PRIVATE SIR::Graphics)
add_dependencies(SIRGraphicsModuleTests SIRGraphicsABIMockModule)
target_compile_definitions(SIRGraphicsModuleTests PRIVATE SIR_TEST_ABI_MODULE_PATH="$<TARGET_FILE:SIRGraphicsABIMockModule>")
add_test(NAME renderer.graphics.binary-module COMMAND SIRGraphicsModuleTests)
sir_configure_owned_target(SIRGraphicsModuleTests)

# Shared ABI v1 conformance is runnable without Vulkan, Direct3D or a GPU.
# The independent suites keep layout drift, pipeline eras and invalid inputs
# separately addressable by CTest, sanitizers and source-based LLVM coverage.
add_executable(SIRGraphicsABIContractTests SIRGraphicsABIContractTests.cpp)
target_link_libraries(SIRGraphicsABIContractTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.contract COMMAND SIRGraphicsABIContractTests)
sir_configure_owned_target(SIRGraphicsABIContractTests)

add_executable(SIRGraphicsABIPropertyTests SIRGraphicsABIPropertyTests.cpp)
target_link_libraries(SIRGraphicsABIPropertyTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.properties COMMAND SIRGraphicsABIPropertyTests)
sir_configure_owned_target(SIRGraphicsABIPropertyTests)

add_executable(SIRGraphicsABINegativeMatrixTests SIRGraphicsABINegativeMatrixTests.cpp)
target_link_libraries(SIRGraphicsABINegativeMatrixTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.negative-matrix COMMAND SIRGraphicsABINegativeMatrixTests)
sir_configure_owned_target(SIRGraphicsABINegativeMatrixTests)

add_executable(SIRGraphicsABIEraMatrixTests SIRGraphicsABIEraMatrixTests.cpp)
target_link_libraries(SIRGraphicsABIEraMatrixTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.era-matrix COMMAND SIRGraphicsABIEraMatrixTests)
sir_configure_owned_target(SIRGraphicsABIEraMatrixTests)

set_tests_properties(
    renderer.graphics.interface
    renderer.graphics.binary-abi
    renderer.graphics.pipeline-eras
    renderer.graphics.pipeline-bridge
    renderer.graphics.binary-module
    renderer.graphics.contract
    renderer.graphics.properties
    renderer.graphics.era-matrix
    renderer.graphics.negative-matrix
    PROPERTIES LABELS "graphics-abi;cpu;conformance" TIMEOUT 90)

if(SIR_WITH_GOOGLETEST)
    include(GoogleTest)
    add_executable(SIRGraphicsABIAssertionGoogleTests SIRGraphicsABIAssertionGoogleTests.cpp)
    target_link_libraries(SIRGraphicsABIAssertionGoogleTests PRIVATE SIR::Graphics gtest_main)
    sir_configure_owned_target(SIRGraphicsABIAssertionGoogleTests)
    gtest_discover_tests(SIRGraphicsABIAssertionGoogleTests
        TEST_PREFIX "renderer.graphics.gtest."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "graphics-abi;gtest;conformance" TIMEOUT 90)
endif()

add_subdirectory(Config)
