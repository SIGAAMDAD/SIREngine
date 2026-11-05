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

# GRDG resource graph regression coverage.
add_executable(SIRResourceInterfaceTests SIRResourceInterfaceTests.cpp)
target_link_libraries(SIRResourceInterfaceTests PRIVATE SIR::Resources)
add_test(NAME resources.interfaces.headers COMMAND SIRResourceInterfaceTests)
sir_configure_owned_target(SIRResourceInterfaceTests)

add_executable(SIRResourceGraphTests SIRResourceGraphTests.cpp)
target_link_libraries(SIRResourceGraphTests PRIVATE SIR::Resources)
add_test(NAME resources.graph.lifecycle COMMAND SIRResourceGraphTests)
sir_configure_owned_target(SIRResourceGraphTests)

add_executable(SIRResourceMemoryIntegrationTests SIRResourceMemoryIntegrationTests.cpp)
target_link_libraries(SIRResourceMemoryIntegrationTests PRIVATE SIR::Resources)
add_test(NAME resources.memory.domains COMMAND SIRResourceMemoryIntegrationTests)
sir_configure_owned_target(SIRResourceMemoryIntegrationTests)

# Native file/mapped-file loading through Core Jobs and GRDG staging.
add_executable(SIRResourceFileLoadTests SIRResourceFileLoadTests.cpp)
target_link_libraries(SIRResourceFileLoadTests PRIVATE SIR::Resources)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(sirWrappedCall IN ITEMS fstat mmap munmap pread)
        target_link_options(SIRResourceFileLoadTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
endif()
add_test(NAME resources.streaming.native-file-io COMMAND SIRResourceFileLoadTests)
sir_configure_owned_target(SIRResourceFileLoadTests)

# Cross-subsystem stress matrix for native GRDG I/O, Core Jobs and staging.
add_executable(SIRInfrastructureIntegrationTests SIRInfrastructureIntegrationTests.cpp)
target_link_libraries(SIRInfrastructureIntegrationTests PRIVATE SIR::Resources)
add_test(NAME resources.integration.stress COMMAND SIRInfrastructureIntegrationTests)
set_tests_properties(resources.integration.stress PROPERTIES TIMEOUT 120)
sir_configure_owned_target(SIRInfrastructureIntegrationTests)

# Bounded provider-owned derived representations and fence-aware retirement.
add_executable(SIRDerivedResourceRegistryTests SIRDerivedResourceRegistryTests.cpp)
target_link_libraries(SIRDerivedResourceRegistryTests PRIVATE SIR::Resources)
add_test(NAME resources.derived.retirement COMMAND SIRDerivedResourceRegistryTests)
sir_configure_owned_target(SIRDerivedResourceRegistryTests)

add_executable(SIRTaskSchedulerTests SIRTaskSchedulerTests.cpp)
target_link_libraries(SIRTaskSchedulerTests PRIVATE SIR::Core)
add_test(NAME core.jobs.scheduler COMMAND SIRTaskSchedulerTests)
sir_configure_owned_target(SIRTaskSchedulerTests)

add_executable(SIRTaskGraphTests SIRTaskGraphTests.cpp)
target_link_libraries(SIRTaskGraphTests PRIVATE SIR::Core)
add_test(NAME core.jobs.graph COMMAND SIRTaskGraphTests)
sir_configure_owned_target(SIRTaskGraphTests)

add_executable(SIRTaskProviderTests SIRTaskProviderTests.cpp)
target_link_libraries(SIRTaskProviderTests PRIVATE SIR::Core)
add_test(NAME core.jobs.provider COMMAND SIRTaskProviderTests)
sir_configure_owned_target(SIRTaskProviderTests)

add_executable(SIRTaskFenceTrackerTests SIRTaskFenceTrackerTests.cpp)
target_link_libraries(SIRTaskFenceTrackerTests PRIVATE SIR::Core)
add_test(NAME core.jobs.fences COMMAND SIRTaskFenceTrackerTests)
sir_configure_owned_target(SIRTaskFenceTrackerTests)

add_executable(SIRTaskSchedulerStressTests SIRTaskSchedulerStressTests.cpp)
target_link_libraries(SIRTaskSchedulerStressTests PRIVATE SIR::Core)
add_test(NAME core.jobs.stress COMMAND SIRTaskSchedulerStressTests)
set_tests_properties(core.jobs.stress PROPERTIES TIMEOUT 90)
sir_configure_owned_target(SIRTaskSchedulerStressTests)

add_executable(SIRSchedulerExperimentTests SIRSchedulerExperimentTests.cpp)
target_link_libraries(SIRSchedulerExperimentTests PRIVATE SIR::Core)
add_test(NAME core.jobs.experiments COMMAND SIRSchedulerExperimentTests)
set_tests_properties(core.jobs.experiments PROPERTIES TIMEOUT 90)
sir_configure_owned_target(SIRSchedulerExperimentTests)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME profiling.scheduler.experiment-analysis
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Profiling/TestSIRSchedulerComparison.py")
    add_test(NAME profiling.grdg.benchmark-validator
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Profiling/TestSIRGRDGBenchmarkValidator.py")
    add_test(NAME profiling.grdg.coverage-collector
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Profiling/TestSIRGRDGCoverageCollector.py")
endif()

add_executable(SIRSchedulerQueueCandidateTests
    SIRSchedulerQueueCandidateTests.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRSchedulerQueueCandidates.cpp)
target_link_libraries(SIRSchedulerQueueCandidateTests PRIVATE SIR::Core)
add_test(NAME core.jobs.queue-candidates COMMAND SIRSchedulerQueueCandidateTests)
set_tests_properties(core.jobs.queue-candidates PROPERTIES TIMEOUT 90)
sir_configure_owned_target(SIRSchedulerQueueCandidateTests)

# Core-owned file, mapped-file, and anonymous-pipe services.
foreach(sirTest IN ITEMS SIRPlatformFileTests SIRPlatformFileMappingTests SIRPlatformPipeTests)
    add_executable(${sirTest} ${sirTest}.cpp)
    target_link_libraries(${sirTest} PRIVATE SIR::Core)
    add_test(NAME core.platform.${sirTest} COMMAND ${sirTest})
    sir_configure_owned_target(${sirTest})
endforeach()

add_executable(SIRSemaphoreTests SIRSemaphoreTests.cpp)
target_link_libraries(SIRSemaphoreTests PRIVATE SIR::Core)
add_test(NAME core.platform.semaphore COMMAND SIRSemaphoreTests)
sir_configure_owned_target(SIRSemaphoreTests)

# Platform process, filesystem and current-thread controls.
add_executable(SIRChildProcessTests SIRChildProcessTests.cpp)
target_link_libraries(SIRChildProcessTests PRIVATE SIR::Core)
add_test(NAME core.platform.child-process COMMAND SIRChildProcessTests)
sir_configure_owned_target(SIRChildProcessTests)

add_executable(SIRPlatformFileSystemTests SIRPlatformFileSystemTests.cpp)
target_link_libraries(SIRPlatformFileSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.filesystem COMMAND SIRPlatformFileSystemTests)
sir_configure_owned_target(SIRPlatformFileSystemTests)

add_executable(SIRPlatformThreadControlTests SIRPlatformThreadControlTests.cpp)
target_link_libraries(SIRPlatformThreadControlTests PRIVATE SIR::Core)
add_test(NAME core.platform.thread-control COMMAND SIRPlatformThreadControlTests)
sir_configure_owned_target(SIRPlatformThreadControlTests)


# Extended cross-platform I/O regression matrix, including descriptor and
# handle lifecycle, concurrent positional writes, mapping, pipe and IPC tests.
add_executable(SIRPlatformIORegressionTests SIRPlatformIORegressionTests.cpp)
target_link_libraries(SIRPlatformIORegressionTests PRIVATE SIR::Core)
add_test(NAME core.platform.io.regression COMMAND SIRPlatformIORegressionTests)
set_tests_properties(core.platform.io.regression PROPERTIES TIMEOUT 120)
sir_configure_owned_target(SIRPlatformIORegressionTests)

# Production Linux syscalls are intercepted at the final test link boundary.
# Fault injection does not modify the shipping platform implementation.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRPlatformIOFaultInjectionTests SIRPlatformIOFaultInjectionTests.cpp)
    target_link_libraries(SIRPlatformIOFaultInjectionTests PRIVATE SIR::Core)
    foreach(sirWrappedCall IN ITEMS open pread pwrite write mmap msync fstat stat fsync ftruncate close sysconf munmap pipe2 sem_wait sem_timedwait posix_spawnp)
        target_link_options(SIRPlatformIOFaultInjectionTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
    add_test(NAME core.platform.io.fault-injection COMMAND SIRPlatformIOFaultInjectionTests)
    set_tests_properties(core.platform.io.fault-injection PROPERTIES TIMEOUT 60)
    sir_configure_owned_target(SIRPlatformIOFaultInjectionTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRGRDGDeathTests SIRGRDGDeathTests.cpp)
    target_link_libraries(SIRGRDGDeathTests PRIVATE SIR::Resources)
    target_link_options(SIRGRDGDeathTests PRIVATE "LINKER:--wrap=abort")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "--coverage")
        target_compile_definitions(SIRGRDGDeathTests PRIVATE SIR_GRDG_GCOV_ENABLED=1)
    endif()
    add_test(NAME resources.lifetime.death COMMAND SIRGRDGDeathTests)
    sir_configure_owned_target(SIRGRDGDeathTests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRTaskSchedulerFaultInjectionTests SIRTaskSchedulerFaultInjectionTests.cpp)
    target_link_libraries(SIRTaskSchedulerFaultInjectionTests PRIVATE SIR::Core)
    target_link_options(SIRTaskSchedulerFaultInjectionTests PRIVATE "LINKER:--wrap=pthread_create")
    add_test(NAME core.jobs.thread-fault-injection COMMAND SIRTaskSchedulerFaultInjectionTests)
    sir_configure_owned_target(SIRTaskSchedulerFaultInjectionTests)
endif()

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
add_dependencies(SIRGraphicsModuleTests SIRPlatformTestModule)
target_compile_definitions(SIRGraphicsModuleTests PRIVATE SIR_TEST_NO_ABI_MODULE_PATH="$<TARGET_FILE:SIRPlatformTestModule>")
# Inject one OS unload error only inside this unit executable, never in the
# production library or other tests. The injection is Linux linker-local.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(SIRGraphicsModuleTests PRIVATE "LINKER:--wrap=dlclose")
endif()
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
include(${CMAKE_CURRENT_SOURCE_DIR}/StringPoolTests.cmake)

if(SIR_WITH_VULKAN)
    add_executable(SIRVulkanMemoryPolicyTests SIRVulkanMemoryPolicyTests.cpp)
    target_link_libraries(SIRVulkanMemoryPolicyTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.memory-policy COMMAND SIRVulkanMemoryPolicyTests)
    sir_configure_owned_target(SIRVulkanMemoryPolicyTests)

    add_executable(SIRVulkanSealedPoolTests SIRVulkanSealedPoolTests.cpp)
    target_link_libraries(SIRVulkanSealedPoolTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.sealed-pools COMMAND SIRVulkanSealedPoolTests)
    set_tests_properties(render.vulkan.sealed-pools PROPERTIES SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRVulkanSealedPoolTests)

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

    add_executable(SIRVulkanShaderTests SIRVulkanShaderTests.cpp)
    target_link_libraries(SIRVulkanShaderTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.shader-envelope COMMAND SIRVulkanShaderTests)
    sir_configure_owned_target(SIRVulkanShaderTests)

    add_executable(SIRVulkanBufferBarrierTests SIRVulkanBufferBarrierTests.cpp)
    target_link_libraries(SIRVulkanBufferBarrierTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.buffer-barriers COMMAND SIRVulkanBufferBarrierTests)
    sir_configure_owned_target(SIRVulkanBufferBarrierTests)

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
