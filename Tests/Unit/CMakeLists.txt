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

if(SIR_WITH_OPENGL)
    add_executable(SIRGL46FramebufferCacheTests
        Renderer/OpenGL/SIRGLFramebufferCacheTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFramebufferCache.cpp")
    target_link_libraries(SIRGL46FramebufferCacheTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FramebufferCacheTests)
    add_test(NAME renderer.opengl46.framebuffer-cache COMMAND SIRGL46FramebufferCacheTests)

    add_executable(SIRGL46FramebufferTests
        Renderer/OpenGL/SIRGLFramebuffersTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFramebuffers.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46FramebufferTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FramebufferTests)
    add_test(NAME renderer.opengl46.framebuffers COMMAND SIRGL46FramebufferTests)

    add_executable(SIRGL46FormatsTests
        Renderer/OpenGL/SIRGLFormatsTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFormats.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46FormatsTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FormatsTests)
    add_test(NAME renderer.opengl46.formats COMMAND SIRGL46FormatsTests)

    add_executable(SIRGL46TexturesTests
        Renderer/OpenGL/SIRGLTexturesTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLTextures.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46TexturesTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46TexturesTests)
    add_test(NAME renderer.opengl46.textures COMMAND SIRGL46TexturesTests)

    add_executable(SIRGL46SamplersTests
        Renderer/OpenGL/SIRGLSamplersTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLSamplers.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46SamplersTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46SamplersTests)
    add_test(NAME renderer.opengl46.samplers COMMAND SIRGL46SamplersTests)

    add_executable(SIRGL46BuffersTests
        Renderer/OpenGL/SIRGLBuffersTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLBuffers.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46BuffersTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46BuffersTests)
    add_test(NAME renderer.opengl46.buffers COMMAND SIRGL46BuffersTests)

    add_executable(SIRGL46HandlePoolTests
        Renderer/OpenGL/SIRGLHandlePoolTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLHandlePool.cpp")
    target_link_libraries(SIRGL46HandlePoolTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46HandlePoolTests)
    add_test(NAME renderer.opengl46.handle-pool COMMAND SIRGL46HandlePoolTests)

    add_executable(SIRGL46FunctionLoaderTests
        Renderer/OpenGL/SIRGLFunctionLoaderTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46FunctionLoaderTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FunctionLoaderTests)
    add_test(NAME renderer.opengl46.function-loader COMMAND SIRGL46FunctionLoaderTests)

    add_executable(SIRGL46FoundationTests
        Renderer/OpenGL/SIRGLFoundationTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLCapabilities.cpp")
    target_link_libraries(SIRGL46FoundationTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FoundationTests)
    add_test(NAME renderer.opengl46.foundation COMMAND SIRGL46FoundationTests)

    find_package(Threads REQUIRED)
    add_executable(SIRGL46ModuleTests Renderer/OpenGL/SIRGLModuleTests.cpp)
    target_link_libraries(SIRGL46ModuleTests PRIVATE SIR::Graphics Threads::Threads "${CMAKE_DL_LIBS}")
    target_compile_definitions(SIRGL46ModuleTests PRIVATE SIR_TEST_GL46_MODULE_PATH="$<TARGET_FILE:SIRGL46Driver>")
    add_dependencies(SIRGL46ModuleTests SIRGL46Driver)
    sir_configure_owned_target(SIRGL46ModuleTests)
    add_test(NAME renderer.opengl46.module COMMAND SIRGL46ModuleTests)
endif()

add_subdirectory(Config)
include(${CMAKE_CURRENT_SOURCE_DIR}/StringPoolTests.cmake)

# Native bounded logical VFS path validation.
add_executable(SIRVFSPathTests SIRVFSPathTests.cpp)
target_link_libraries(SIRVFSPathTests PRIVATE SIR::Core)
add_test(NAME core.vfs.path-normalization COMMAND SIRVFSPathTests)
sir_configure_owned_target(SIRVFSPathTests)

# Bounded VFS mount registry, overlay precedence, and lifetime leases.
add_executable(SIRVFSMountTableTests SIRVFSMountTableTests.cpp)
target_link_libraries(SIRVFSMountTableTests PRIVATE SIR::Core)
add_test(NAME core.vfs.mount-registry COMMAND SIRVFSMountTableTests)
sir_configure_owned_target(SIRVFSMountTableTests)

# VFS metadata-only overlay selection and generation-pinned winner lease.
add_executable(SIRVFSOverlayResolverTests SIRVFSOverlayResolverTests.cpp)
target_link_libraries(SIRVFSOverlayResolverTests PRIVATE SIR::Core)
add_test(NAME core.vfs.overlay-resolver COMMAND SIRVFSOverlayResolverTests)
sir_configure_owned_target(SIRVFSOverlayResolverTests)

# VFS immutable logical file identity, source pins and deferred close.
add_executable(SIRVFSFileHandleTableTests SIRVFSFileHandleTableTests.cpp)
target_link_libraries(SIRVFSFileHandleTableTests PRIVATE SIR::Core)
add_test(NAME core.vfs.file-handle-lifetime COMMAND SIRVFSFileHandleTableTests)
sir_configure_owned_target(SIRVFSFileHandleTableTests)

# Root-bound native directory opens: traversal/reparse denial and held handles.
add_executable(SIRPlatformRootDirectoryTests SIRPlatformRootDirectoryTests.cpp)
target_link_libraries(SIRPlatformRootDirectoryTests PRIVATE SIR::Core)
add_test(NAME core.platform.rooted-directory COMMAND SIRPlatformRootDirectoryTests)
sir_configure_owned_target(SIRPlatformRootDirectoryTests)

# Generation-pinned VFS directory source to native platform read integration.
add_executable(SIRVFSDirectorySourceTests SIRVFSDirectorySourceTests.cpp)
target_link_libraries(SIRVFSDirectorySourceTests PRIVATE SIR::Core)
add_test(NAME core.vfs.directory-source COMMAND SIRVFSDirectorySourceTests)
sir_configure_owned_target(SIRVFSDirectorySourceTests)

# Device-independent FrameGraph DAG, hazard and subresource validation.
add_executable(SIRFrameGraphTests SIRFrameGraphTests.cpp)
target_link_libraries(SIRFrameGraphTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.compiler COMMAND SIRFrameGraphTests)
set_tests_properties(renderer.framegraph.compiler PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphTests)

# Provider-admitted FrameGraph transient residency planning and negative cases.
add_executable(SIRFrameGraphTransientTests SIRFrameGraphTransientTests.cpp)
target_link_libraries(SIRFrameGraphTransientTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.transient COMMAND SIRFrameGraphTransientTests)
set_tests_properties(renderer.framegraph.transient PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphTransientTests)

include(${CMAKE_CURRENT_SOURCE_DIR}/MeshCacheTests.cmake)

# Cross-owner provider completion and graphics binary boundary conformance.
add_executable(SIRProviderCompletionTests SIRProviderCompletionTests.cpp)
target_link_libraries(SIRProviderCompletionTests PRIVATE SIR::Resources)
add_test(NAME resources.provider.multilane-contract COMMAND SIRProviderCompletionTests)
sir_configure_owned_target(SIRProviderCompletionTests)

add_executable(SIRGraphicsABISharedBoundaryTests SIRGraphicsABISharedBoundaryTests.cpp)
target_link_libraries(SIRGraphicsABISharedBoundaryTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.shared-boundary COMMAND SIRGraphicsABISharedBoundaryTests)
sir_configure_owned_target(SIRGraphicsABISharedBoundaryTests)


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


include(${CMAKE_CURRENT_SOURCE_DIR}/SIRD3D12Tests.cmake)

# Native D3D11 registration remains owned by its local CMake fragment.
add_subdirectory(Renderer/D3D11)

# Native shader contract, material schema, GRDG source and pipeline fixtures.
add_executable(SIRShaderPipelineTests SIRShaderPipelineTests.cpp)
target_link_libraries(SIRShaderPipelineTests PRIVATE SIR::Graphics)
add_test(NAME renderer.shader.pipeline COMMAND SIRShaderPipelineTests)
sir_configure_owned_target(SIRShaderPipelineTests)

foreach(sirShaderUnit IN ITEMS SIRShaderContractsTests SIRShaderMaterialManifestTests SIRShaderGRDGTests)
    add_executable(${sirShaderUnit} Shaders/${sirShaderUnit}.cpp)
    target_link_libraries(${sirShaderUnit} PRIVATE SIR::Graphics)
    add_test(NAME renderer.shader.${sirShaderUnit} COMMAND ${sirShaderUnit})
    sir_configure_owned_target(${sirShaderUnit})
endforeach()

# Terrain3D Sparse VT V1 package contract and reference decoder.
add_executable(SIRVirtualTexturePortTests SIRVirtualTexturePortTests.cpp)
target_link_libraries(SIRVirtualTexturePortTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.terrain3d-v1 COMMAND SIRVirtualTexturePortTests)
sir_configure_owned_target(SIRVirtualTexturePortTests)

# Native Hi-Z reference and command validation.
include(${CMAKE_CURRENT_LIST_DIR}/HIZFoundationTests.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/WorldHLODTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/WorldPartitionTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingReconcileTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingProviderBridgeTests.cmake)

# Registration, Console and CVar admission and archive fixtures.
add_executable(SIRRegistrationContractTests SIRRegistrationContractTests.cpp)
target_link_libraries(SIRRegistrationContractTests PRIVATE SIR::Core)
add_test(NAME core.registration.contract COMMAND SIRRegistrationContractTests)
sir_configure_owned_target(SIRRegistrationContractTests)

add_executable(SIRRegistrationStringBridgeTests SIRRegistrationStringBridgeTests.cpp)
target_link_libraries(SIRRegistrationStringBridgeTests PRIVATE SIR::Core)
add_test(NAME core.registration.string-bridge COMMAND SIRRegistrationStringBridgeTests)
sir_configure_owned_target(SIRRegistrationStringBridgeTests)

add_executable(SIRRegistrationCookerTests SIRRegistrationCookerTests.cpp)
target_link_libraries(SIRRegistrationCookerTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.registration.cooker COMMAND SIRRegistrationCookerTests)
sir_configure_owned_target(SIRRegistrationCookerTests)

add_test(NAME core.registration.cooker-cli COMMAND "${CMAKE_COMMAND}"
    "-DSIR_COOKER=$<TARGET_FILE:SIRRegistrationCooker>"
    "-DSIR_FIXTURES=${PROJECT_SOURCE_DIR}/Tests/Unit/Registration"
    "-DSIR_OUT=${PROJECT_BINARY_DIR}/RegistrationCookerSmoke"
    -P "${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/CookerCLI.cmake")

add_executable(SIRCVarRegistryTests CVars/SIRCVarRegistryIntegrationTests.cpp)
target_link_libraries(SIRCVarRegistryTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.cvars.registry COMMAND SIRCVarRegistryTests)
sir_configure_owned_target(SIRCVarRegistryTests)

add_executable(SIRConsoleServiceTests Console/SIRConsoleServiceTests.cpp)
target_link_libraries(SIRConsoleServiceTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.console.service COMMAND SIRConsoleServiceTests)
sir_configure_owned_target(SIRConsoleServiceTests)

add_executable(SIRIniConfigTests Config/SIRIniTests.cpp)
target_link_libraries(SIRIniConfigTests PRIVATE SIR::Core)
add_test(NAME core.config.ini COMMAND SIRIniConfigTests)
sir_configure_owned_target(SIRIniConfigTests)

add_executable(SIRCVarConfigTests CVars/SIRCVarConfigTests.cpp)
target_link_libraries(SIRCVarConfigTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.cvars.config COMMAND SIRCVarConfigTests)
sir_configure_owned_target(SIRCVarConfigTests)

# Audio provider-neutral contracts and FMOD mock backend tests.
include(${CMAKE_CURRENT_SOURCE_DIR}/SIRAudioContract.cmake)

# Native Input regression and integration tests from the existing input branch.
add_executable(SIRApplicationInputTests SIRApplicationInputTests.cpp)
target_link_libraries(SIRApplicationInputTests PRIVATE SIR::Core)
add_test(NAME core.application.input-lifecycle COMMAND SIRApplicationInputTests)
set_tests_properties(core.application.input-lifecycle PROPERTIES LABELS "core;input;integration" TIMEOUT 60)
sir_configure_owned_target(SIRApplicationInputTests)

add_executable(SIRInputActionTests SIRInputActionTests.cpp)
target_link_libraries(SIRInputActionTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.actions COMMAND SIRInputActionTests)
set_tests_properties(core.platform.input.actions PROPERTIES LABELS "core;input;actions" TIMEOUT 60)
sir_configure_owned_target(SIRInputActionTests)

add_executable(SIRInputHapticNegativeTests SIRInputHapticNegativeTests.cpp)
target_link_libraries(SIRInputHapticNegativeTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.haptic-negative COMMAND SIRInputHapticNegativeTests)
set_tests_properties(core.platform.input.haptic-negative PROPERTIES LABELS "core;input;haptics;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputHapticNegativeTests)

add_executable(SIRInputProfileNegativeTests SIRInputProfileNegativeTests.cpp)
target_link_libraries(SIRInputProfileNegativeTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.profile-negative COMMAND SIRInputProfileNegativeTests)
set_tests_properties(core.platform.input.profile-negative PROPERTIES LABELS "core;input;profiles;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputProfileNegativeTests)

add_executable(SIRInputFrameNegativeTests SIRInputFrameNegativeTests.cpp)
target_link_libraries(SIRInputFrameNegativeTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.frame-negative COMMAND SIRInputFrameNegativeTests)
set_tests_properties(core.platform.input.frame-negative PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputFrameNegativeTests)

add_executable(SIRInputServiceBoundaryTests SIRInputServiceBoundaryTests.cpp)
target_link_libraries(SIRInputServiceBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.service-boundary COMMAND SIRInputServiceBoundaryTests)
set_tests_properties(core.platform.input.service-boundary PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputServiceBoundaryTests)

add_executable(SIRInputContractGapTests SIRInputContractGapTests.cpp)
target_link_libraries(SIRInputContractGapTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.contract-regressions COMMAND SIRInputContractGapTests)
set_tests_properties(core.platform.input.contract-regressions PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputContractGapTests)

add_executable(SIRInputTextValidationTests SIRInputTextValidationTests.cpp)
target_link_libraries(SIRInputTextValidationTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.text-validation COMMAND SIRInputTextValidationTests)
set_tests_properties(core.platform.input.text-validation PROPERTIES LABELS "core;input;text;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputTextValidationTests)

add_executable(SIRInputPhase45Tests SIRInputPhase45Tests.cpp)
target_link_libraries(SIRInputPhase45Tests PRIVATE SIR::Core)
add_test(NAME core.platform.input.ui-haptics-profiles COMMAND SIRInputPhase45Tests)
set_tests_properties(core.platform.input.ui-haptics-profiles PROPERTIES LABELS "core;input;ui;haptics;profiles" TIMEOUT 60)
sir_configure_owned_target(SIRInputPhase45Tests)

add_executable(SIRInputHistoryTests SIRInputHistoryTests.cpp)
target_link_libraries(SIRInputHistoryTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.history COMMAND SIRInputHistoryTests)
set_tests_properties(core.platform.input.history PROPERTIES LABELS "core;input;rollback" TIMEOUT 60)
sir_configure_owned_target(SIRInputHistoryTests)

add_executable(SIRInputReplayTests SIRInputReplayTests.cpp)
target_link_libraries(SIRInputReplayTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.replay COMMAND SIRInputReplayTests)
set_tests_properties(core.platform.input.replay PROPERTIES LABELS "core;input;replay" TIMEOUT 60)
sir_configure_owned_target(SIRInputReplayTests)

add_executable(SIRInputTests SIRInputTests.cpp)
target_link_libraries(SIRInputTests PRIVATE SIR::Core)
add_test(NAME core.platform.input COMMAND SIRInputTests)
sir_configure_owned_target(SIRInputTests)

# Deterministic headless stress suite: enabled in every Windows/Linux CTest run.
add_executable(SIRInputStressTests SIRInputStressTests.cpp)
target_link_libraries(SIRInputStressTests PRIVATE SIR::Core)
sir_configure_owned_target(SIRInputStressTests)
add_test(NAME core.platform.input.stress COMMAND SIRInputStressTests)
set_tests_properties(core.platform.input.stress PROPERTIES LABELS "core;input;stress" TIMEOUT 60)

# The longer 9.6M-request soak runs in nightly and sanitizer configurations.
option(SIR_INPUT_SOAK_TESTS "Include extended native input stress soak in CTest" OFF)
if(SIR_INPUT_SOAK_TESTS)
    add_test(NAME core.platform.input.soak COMMAND SIRInputStressTests --soak)
    set_tests_properties(core.platform.input.soak PROPERTIES LABELS "core;input;soak" TIMEOUT 180)
endif()

# Sparse/dense frame snapshot differential, portable across Windows and Linux.
add_executable(SIRInputSnapshotDeltaTests SIRInputSnapshotDeltaTests.cpp)
target_link_libraries(SIRInputSnapshotDeltaTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.snapshot-delta COMMAND SIRInputSnapshotDeltaTests)
set_tests_properties(core.platform.input.snapshot-delta PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputSnapshotDeltaTests)

# Native evdev report batching under an instrumented pipe; no device permissions needed.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRInputEvdevBatchTests SIRInputEvdevBatchTests.cpp)
    target_link_libraries(SIRInputEvdevBatchTests PRIVATE SIR::Core)
    target_link_options(SIRInputEvdevBatchTests PRIVATE "LINKER:--wrap=read" "LINKER:--wrap=ioctl")
    add_test(NAME core.platform.input.evdev-batch COMMAND SIRInputEvdevBatchTests)
    set_tests_properties(core.platform.input.evdev-batch PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
    sir_configure_owned_target(SIRInputEvdevBatchTests)
endif()

# Linux X11 window/event acceptance with an actual ephemeral Xvfb server.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_INPUT)
    find_package(X11 QUIET)
    find_program(SIR_XVFB_RUN NAMES xvfb-run)
    if(X11_FOUND AND SIR_XVFB_RUN)
        add_executable(SIRInputX11WindowTests SIRInputX11WindowTests.cpp)
        target_link_libraries(SIRInputX11WindowTests PRIVATE SIR::Core X11::X11)
        target_link_options(SIRInputX11WindowTests PRIVATE
            "LINKER:--wrap=XCreateSimpleWindow"
            "LINKER:--wrap=XCreateBitmapFromData"
            "LINKER:--wrap=XCreatePixmapCursor"
            "LINKER:--wrap=XGrabPointer"
            "LINKER:--wrap=XFilterEvent"
            "LINKER:--wrap=XNextEvent")
        add_test(NAME core.platform.input.x11-window COMMAND "${SIR_XVFB_RUN}" -a "$<TARGET_FILE:SIRInputX11WindowTests>")
        set_tests_properties(core.platform.input.x11-window PROPERTIES LABELS "core;input;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRInputX11WindowTests)
        add_executable(SIRInputX11IMEFallbackTests SIRInputX11IMEFallbackTests.cpp)
        target_link_libraries(SIRInputX11IMEFallbackTests PRIVATE SIR::Core X11::X11)
        target_link_options(SIRInputX11IMEFallbackTests PRIVATE "LINKER:--wrap=XCreateIC")
        add_test(NAME core.platform.input.x11-ime-fallback COMMAND "${SIR_XVFB_RUN}" -a "$<TARGET_FILE:SIRInputX11IMEFallbackTests>")
        set_tests_properties(core.platform.input.x11-ime-fallback PROPERTIES LABELS "core;input;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRInputX11IMEFallbackTests)
    endif()
endif()

# Win32 packet decoder regression: real MSVC / Windows SDK build, no hardware required.
if(WIN32)
    add_executable(SIRWindowsRawBatchTests SIRWindowsRawBatchTests.cpp)
    target_link_libraries(SIRWindowsRawBatchTests PRIVATE SIR::Core)
    add_test(NAME core.platform.input.windows-raw-batch COMMAND SIRWindowsRawBatchTests)
    set_tests_properties(core.platform.input.windows-raw-batch PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
    sir_configure_owned_target(SIRWindowsRawBatchTests)
endif()

# Portable deterministic GameInput decoder regression; uses no OS or SDK.
add_executable(SIRGameInputCodecTests
    SIRGameInputCodecTests.cpp
    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Input/Windows/SIRWindowsGameInputCodec.cpp")
target_link_libraries(SIRGameInputCodecTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.gameinput-codec COMMAND SIRGameInputCodecTests)
set_tests_properties(core.platform.input.gameinput-codec PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRGameInputCodecTests)


# Platform file-lock provider and full SIRLang compiler/runtime fixtures.
add_executable(SIRFileLockTests SIRFileLockTests.cpp)
target_link_libraries(SIRFileLockTests PRIVATE SIR::Core)
add_test(NAME core.platform.file-lock COMMAND SIRFileLockTests)
sir_configure_owned_target(SIRFileLockTests)
include(${CMAKE_CURRENT_SOURCE_DIR}/ScriptingTests.cmake)

# Native editor authoring and scene intake tests.
add_executable(SIREditorAuthoringTests SIREditorAuthoringTests.cpp)
target_link_libraries(SIREditorAuthoringTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.transactions-and-virtual-list COMMAND SIREditorAuthoringTests)
sir_configure_owned_target(SIREditorAuthoringTests)

# Native Godot-scene hierarchy intake for the editor-only authoring bridge.
add_executable(SIREditorTextSceneTests SIREditorTextSceneTests.cpp)
target_link_libraries(SIREditorTextSceneTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextSceneTests PRIVATE
    SIR_EDITOR_TEST_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotGetNode.tscn"
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.godot-text-scene COMMAND SIREditorTextSceneTests)
sir_configure_owned_target(SIREditorTextSceneTests)

# Read-only external and embedded resource-dependency validation.
add_executable(SIREditorTextResourcesTests SIREditorTextResourcesTests.cpp)
target_link_libraries(SIREditorTextResourcesTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextResourcesTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.godot-text-resources COMMAND SIREditorTextResourcesTests)
sir_configure_owned_target(SIREditorTextResourcesTests)

# Editor-only scene tree projection, source ordering and virtualized layout.
add_executable(SIREditorHierarchyModelTests SIREditorHierarchyModelTests.cpp)
target_link_libraries(SIREditorHierarchyModelTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorHierarchyModelTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.hierarchy-view-model COMMAND SIREditorHierarchyModelTests)
sir_configure_owned_target(SIREditorHierarchyModelTests)

# Native editor workspace and executable bootstrap over the Godot text-scene intake.
add_executable(SIREditorSceneWorkspaceTests SIREditorSceneWorkspaceTests.cpp)
target_link_libraries(SIREditorSceneWorkspaceTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorSceneWorkspaceTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.host.scene-workspace COMMAND SIREditorSceneWorkspaceTests)
sir_configure_owned_target(SIREditorSceneWorkspaceTests)

add_test(NAME editor.host.version COMMAND SIREditor --version)
set_tests_properties(editor.host.version PROPERTIES PASS_REGULAR_EXPRESSION "SIREditor native scene host")
add_test(NAME editor.host.inspect-scene COMMAND SIREditor --inspect-scene
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
set_tests_properties(editor.host.inspect-scene PROPERTIES PASS_REGULAR_EXPRESSION "Nodes: 5")
add_test(NAME editor.host.missing-scene COMMAND SIREditor --inspect-scene
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/does-not-exist.tscn")
set_tests_properties(editor.host.missing-scene PROPERTIES WILL_FAIL TRUE)

# Source-backed node-property index feeding the read-only native Details dock.
add_executable(SIREditorTextPropertiesTests SIREditorTextPropertiesTests.cpp)
target_link_libraries(SIREditorTextPropertiesTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextPropertiesTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.source-properties COMMAND SIREditorTextPropertiesTests)
sir_configure_owned_target(SIREditorTextPropertiesTests)


# SIREngine native object/type/property reflection and generation-safe handles.
add_executable(SIRObjectRegistryTests SIRObjectRegistryTests.cpp)
target_link_libraries(SIRObjectRegistryTests PRIVATE SIR::Core)
add_test(NAME core.objects.registry-and-lifetime COMMAND SIRObjectRegistryTests)
sir_configure_owned_target(SIRObjectRegistryTests)

# Typed authored property changes share the scene document's undo/redo journal.
add_executable(SIREditorTypedPropertyTests SIREditorTypedPropertyTests.cpp)
target_link_libraries(SIREditorTypedPropertyTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.typed-property-transactions COMMAND SIREditorTypedPropertyTests)
sir_configure_owned_target(SIREditorTypedPropertyTests)

# Deterministic differential property-state machine: 2500 mixed valid/invalid transactions.
add_executable(SIREditorPropertyStressTests SIREditorPropertyStressTests.cpp)
target_link_libraries(SIREditorPropertyStressTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.property-journal-stress COMMAND SIREditorPropertyStressTests)
sir_configure_owned_target(SIREditorPropertyStressTests)

# Native SIREngine resource identity and dependency DAG without a foreign loader.
add_executable(SIRResourceCatalogTests SIRResourceCatalogTests.cpp)
target_link_libraries(SIRResourceCatalogTests PRIVATE SIR::Resources)
add_test(NAME resources.catalog.dependency-validation COMMAND SIRResourceCatalogTests)
sir_configure_owned_target(SIRResourceCatalogTests)

# Allocation-free typed text-scene scalar conversion and precision roundtrips.
add_executable(SIREditorScalarCodecTests SIREditorScalarCodecTests.cpp)
target_link_libraries(SIREditorScalarCodecTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.typed-scalar-codec COMMAND SIREditorScalarCodecTests)
sir_configure_owned_target(SIREditorScalarCodecTests)

# Bound native typed-property edit import, undo and lossless text-scene export.
add_executable(SIREditorScalarBindingTests SIREditorScalarBindingTests.cpp)
target_link_libraries(SIREditorScalarBindingTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.typed-scalar-binding COMMAND SIREditorScalarBindingTests)
sir_configure_owned_target(SIREditorScalarBindingTests)

# Independent reference graph validates resource DAG, duplicate, and cycle checks.
add_executable(SIRResourceCatalogDifferentialTests SIRResourceCatalogDifferentialTests.cpp)
target_link_libraries(SIRResourceCatalogDifferentialTests PRIVATE SIR::Resources)
add_test(NAME resources.catalog.differential-dag COMMAND SIRResourceCatalogDifferentialTests)
sir_configure_owned_target(SIRResourceCatalogDifferentialTests)

# Full source fixture: reflect, import, edit, undo/redo, losslessly serialize and reopen.
add_executable(SIREditorRealSceneScalarRoundtripTests SIREditorRealSceneScalarRoundtripTests.cpp)
target_link_libraries(SIREditorRealSceneScalarRoundtripTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorRealSceneScalarRoundtripTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.scene.real-fixture-scalar-roundtrip COMMAND SIREditorRealSceneScalarRoundtripTests)
sir_configure_owned_target(SIREditorRealSceneScalarRoundtripTests)

# Real crash-conscious scene file replacement, staging collisions and failed rename.
add_executable(SIREditorFileSaveTests SIREditorFileSaveTests.cpp)
target_link_libraries(SIREditorFileSaveTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.atomic-file-save COMMAND SIREditorFileSaveTests)
sir_configure_owned_target(SIREditorFileSaveTests)

# Source-format scalar edit preservation and fail-closed writer.
add_executable(SIREditorTextWriterTests SIREditorTextWriterTests.cpp)
target_link_libraries(SIREditorTextWriterTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextWriterTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.scene.text-scalar-writer COMMAND SIREditorTextWriterTests)
sir_configure_owned_target(SIREditorTextWriterTests)

# Retained SIREngine-native control hierarchy, focus, layout, clipping and input.
add_executable(SIRControlTreeTests SIRControlTreeTests.cpp)
target_link_libraries(SIRControlTreeTests PRIVATE SIR::UI)
add_test(NAME ui.controls.layout-and-focus COMMAND SIRControlTreeTests)
sir_configure_owned_target(SIRControlTreeTests)

# Platform-owned bounded binary file reading, regular-file rejection and capacity gates.
add_executable(SIRFileSystemReadTests SIRFileSystemReadTests.cpp)
target_link_libraries(SIRFileSystemReadTests PRIVATE SIR::Core)
add_test(NAME core.platform.file-read COMMAND SIRFileSystemReadTests)
sir_configure_owned_target(SIRFileSystemReadTests)

# Source-backed resource references and embedded-resource dependency ownership.
add_executable(SIREditorResourceReferencesTests SIREditorResourceReferencesTests.cpp)
target_link_libraries(SIREditorResourceReferencesTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.resource-reference-index COMMAND SIREditorResourceReferencesTests)
sir_configure_owned_target(SIREditorResourceReferencesTests)

# Streaming coordinator positive/negative state-transition and validation coverage.
add_executable(SIRStreamingCoordinatorCoverageTests SIRStreamingCoordinatorCoverageTests.cpp)
target_link_libraries(SIRStreamingCoordinatorCoverageTests PRIVATE SIR::Streaming)
add_test(NAME streaming.coordinator.coverage COMMAND SIRStreamingCoordinatorCoverageTests)
sir_configure_owned_target(SIRStreamingCoordinatorCoverageTests)
