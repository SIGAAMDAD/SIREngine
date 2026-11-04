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

if(SIR_WITH_D3D12)
    # DXIL is cooked offline; the runtime driver never invokes DXC.
    find_program(SIR_D3D12_DXC_EXECUTABLE NAMES dxc.exe dxc REQUIRED)
    set(SIR_D3D12_SMOKE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SIRD3D12ComputeSmoke.hlsl")
    set(SIR_D3D12_SMOKE_DXIL "${CMAKE_CURRENT_BINARY_DIR}/SIRD3D12ComputeSmoke.dxil")
    add_custom_command(
        OUTPUT "${SIR_D3D12_SMOKE_DXIL}"
        COMMAND "${SIR_D3D12_DXC_EXECUTABLE}" -nologo -T cs_6_0 -E main -O3
                -Fo "${SIR_D3D12_SMOKE_DXIL}" "${SIR_D3D12_SMOKE_SOURCE}"
        DEPENDS "${SIR_D3D12_SMOKE_SOURCE}"
        COMMENT "Cook the D3D12 WARP compute smoke DXIL"
        VERBATIM)
    add_custom_target(SIRD3D12ComputeSmokeDXIL DEPENDS "${SIR_D3D12_SMOKE_DXIL}")
    set(SIR_D3D12_GRAPHICS_SMOKE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SIRD3D12GraphicsSmoke.hlsl")
    set(SIR_D3D12_VS_DXIL "${CMAKE_CURRENT_BINARY_DIR}/SIRD3D12GraphicsSmokeVS.dxil")
    set(SIR_D3D12_PS_DXIL "${CMAKE_CURRENT_BINARY_DIR}/SIRD3D12GraphicsSmokePS.dxil")
    add_custom_command(
        OUTPUT "${SIR_D3D12_VS_DXIL}"
        COMMAND "${SIR_D3D12_DXC_EXECUTABLE}" -nologo -T vs_6_0 -E vsMain -O3
                -Fo "${SIR_D3D12_VS_DXIL}" "${SIR_D3D12_GRAPHICS_SMOKE_SOURCE}"
        DEPENDS "${SIR_D3D12_GRAPHICS_SMOKE_SOURCE}"
        COMMENT "Cook the D3D12 WARP offscreen vertex DXIL"
        VERBATIM)
    add_custom_command(
        OUTPUT "${SIR_D3D12_PS_DXIL}"
        COMMAND "${SIR_D3D12_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E psMain -O3
                -Fo "${SIR_D3D12_PS_DXIL}" "${SIR_D3D12_GRAPHICS_SMOKE_SOURCE}"
        DEPENDS "${SIR_D3D12_GRAPHICS_SMOKE_SOURCE}"
        COMMENT "Cook the D3D12 WARP offscreen pixel DXIL"
        VERBATIM)
    add_custom_target(SIRD3D12GraphicsSmokeDXIL DEPENDS "${SIR_D3D12_VS_DXIL}" "${SIR_D3D12_PS_DXIL}")
    add_executable(SIRD3D12HeadlessTests SIRD3D12HeadlessTests.cpp)
    add_dependencies(SIRD3D12HeadlessTests SIRD3D12ComputeSmokeDXIL SIRD3D12GraphicsSmokeDXIL)
    target_compile_definitions(SIRD3D12HeadlessTests PRIVATE
        SIR_D3D12_SMOKE_DXIL_PATH="${SIR_D3D12_SMOKE_DXIL}"
        SIR_D3D12_VS_DXIL_PATH="${SIR_D3D12_VS_DXIL}"
        SIR_D3D12_PS_DXIL_PATH="${SIR_D3D12_PS_DXIL}")
    target_link_libraries(SIRD3D12HeadlessTests PRIVATE SIR::D3D12)
    add_test(NAME renderer.d3d12.warp-headless COMMAND SIRD3D12HeadlessTests)
    sir_configure_owned_target(SIRD3D12HeadlessTests)
endif()

# The portable contract runs independently of the native graphics driver.
add_executable(SIRGraphicsInterfaceTests SIRGraphicsInterfaceTests.cpp)
target_link_libraries(SIRGraphicsInterfaceTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.interface COMMAND SIRGraphicsInterfaceTests)
sir_configure_owned_target(SIRGraphicsInterfaceTests)

if(SIR_WITH_D3D12)
    add_executable(SIRD3D12SharedApiTests SIRD3D12SharedApiTests.cpp)
    target_link_libraries(SIRD3D12SharedApiTests PRIVATE SIR::D3D12 SIR::Graphics)
    add_test(NAME renderer.d3d12.shared-api COMMAND SIRD3D12SharedApiTests)
    sir_configure_owned_target(SIRD3D12SharedApiTests)
    add_executable(SIRD3D12PipelineTests SIRD3D12PipelineTests.cpp)
    add_dependencies(SIRD3D12PipelineTests SIRD3D12ComputeSmokeDXIL SIRD3D12GraphicsSmokeDXIL)
    target_compile_definitions(SIRD3D12PipelineTests PRIVATE
        SIR_D3D12_SMOKE_DXIL_PATH="${SIR_D3D12_SMOKE_DXIL}"
        SIR_D3D12_VS_DXIL_PATH="${SIR_D3D12_VS_DXIL}"
        SIR_D3D12_PS_DXIL_PATH="${SIR_D3D12_PS_DXIL}")
    target_link_libraries(SIRD3D12PipelineTests PRIVATE SIR::D3D12 SIR::Graphics)
    add_test(NAME renderer.d3d12.pipelines COMMAND SIRD3D12PipelineTests)
    sir_configure_owned_target(SIRD3D12PipelineTests)
    set(SIR_D3D12_COMMANDS_SMOKE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SIRD3D12CommandsSmoke.hlsl")
    set(SIR_D3D12_COMMANDS_DXIL "${CMAKE_CURRENT_BINARY_DIR}/SIRD3D12CommandsSmoke.dxil")
    add_custom_command(
        OUTPUT "${SIR_D3D12_COMMANDS_DXIL}"
        COMMAND "${SIR_D3D12_DXC_EXECUTABLE}" -nologo -T cs_6_0 -E main -O3
                -Fo "${SIR_D3D12_COMMANDS_DXIL}" "${SIR_D3D12_COMMANDS_SMOKE_SOURCE}"
        DEPENDS "${SIR_D3D12_COMMANDS_SMOKE_SOURCE}"
        COMMENT "Cook no-descriptor D3D12 command submission DXIL"
        VERBATIM)
    add_custom_target(SIRD3D12CommandsSmokeDXIL DEPENDS "${SIR_D3D12_COMMANDS_DXIL}")
    add_executable(SIRD3D12CommandsTests SIRD3D12CommandsTests.cpp)
    add_dependencies(SIRD3D12CommandsTests SIRD3D12CommandsSmokeDXIL SIRD3D12GraphicsSmokeDXIL)
    target_compile_definitions(SIRD3D12CommandsTests PRIVATE
        SIR_D3D12_COMMANDS_DXIL_PATH="${SIR_D3D12_COMMANDS_DXIL}"
        SIR_D3D12_VS_DXIL_PATH="${SIR_D3D12_VS_DXIL}"
        SIR_D3D12_PS_DXIL_PATH="${SIR_D3D12_PS_DXIL}")
    target_link_libraries(SIRD3D12CommandsTests PRIVATE SIR::D3D12 SIR::Graphics)
    add_test(NAME renderer.d3d12.commands COMMAND SIRD3D12CommandsTests)
    sir_configure_owned_target(SIRD3D12CommandsTests)
    add_executable(SIRD3D12SurfaceTests SIRD3D12SurfaceTests.cpp)
    target_link_libraries(SIRD3D12SurfaceTests PRIVATE SIR::D3D12 SIR::Graphics user32)
    add_test(NAME renderer.d3d12.surface COMMAND SIRD3D12SurfaceTests)
    sir_configure_owned_target(SIRD3D12SurfaceTests)
    add_executable(SIRD3D12MemoryTests SIRD3D12MemoryTests.cpp)
    target_link_libraries(SIRD3D12MemoryTests PRIVATE SIR::D3D12 SIR::Graphics SIR::Core)
    add_test(NAME renderer.d3d12.memory COMMAND SIRD3D12MemoryTests)
    sir_configure_owned_target(SIRD3D12MemoryTests)
endif()
