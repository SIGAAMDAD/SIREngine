# D3D12-only CPU validators compile on Windows and Linux without native APIs.
add_executable(SIRD3D12ValidationTests SIRD3D12ValidationTests.cpp)
target_link_libraries(SIRD3D12ValidationTests PRIVATE SIR::Graphics)
add_test(NAME renderer.d3d12.validation COMMAND SIRD3D12ValidationTests)
set_tests_properties(renderer.d3d12.validation PROPERTIES LABELS "renderer;d3d12;cpu;kind:unit;gate:pr" TIMEOUT 30)
sir_configure_owned_target(SIRD3D12ValidationTests)

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
    target_link_libraries(SIRD3D12HeadlessTests PRIVATE SIR::D3D12)
    add_test(NAME renderer.d3d12.warp-headless COMMAND SIRD3D12HeadlessTests)
    sir_configure_owned_target(SIRD3D12HeadlessTests)
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

    add_executable(SIRD3D12DiagnosticsTests SIRD3D12DiagnosticsTests.cpp)
    target_link_libraries(SIRD3D12DiagnosticsTests PRIVATE SIR::D3D12 SIR::Graphics)
    add_test(NAME renderer.d3d12.diagnostics COMMAND SIRD3D12DiagnosticsTests --cycles 64)
    set_tests_properties(renderer.d3d12.diagnostics PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRD3D12DiagnosticsTests)

    add_executable(SIRD3D12Benchmarks ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRD3D12Benchmarks.cpp)
    target_link_libraries(SIRD3D12Benchmarks PRIVATE SIR::D3D12 SIR::Graphics)
    add_test(NAME renderer.d3d12.bench-smoke COMMAND SIRD3D12Benchmarks --adapter warp --iterations 16 --warmup 4)
    set_tests_properties(renderer.d3d12.bench-smoke PROPERTIES TIMEOUT 120)
    sir_configure_owned_target(SIRD3D12Benchmarks)
    set(SIR_D3D12_BINDING_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/SIRD3D12BindingSmoke.hlsl")
    set(SIR_D3D12_BINDING_DXIL "${CMAKE_CURRENT_BINARY_DIR}/SIRD3D12BindingSmoke.dxil")
    add_custom_command(
        OUTPUT "${SIR_D3D12_BINDING_DXIL}"
        COMMAND "${SIR_D3D12_DXC_EXECUTABLE}" -nologo -T cs_6_0 -E main -O3
                -Fo "${SIR_D3D12_BINDING_DXIL}" "${SIR_D3D12_BINDING_SOURCE}"
        DEPENDS "${SIR_D3D12_BINDING_SOURCE}"
        COMMENT "Cook root-table UAV binding smoke shader"
        VERBATIM)
    add_custom_target(SIRD3D12BindingSmokeDXIL DEPENDS "${SIR_D3D12_BINDING_DXIL}")
    add_executable(SIRD3D12BindingTests SIRD3D12BindingTests.cpp)
    add_dependencies(SIRD3D12BindingTests SIRD3D12BindingSmokeDXIL)
    target_compile_definitions(SIRD3D12BindingTests PRIVATE
        SIR_D3D12_BINDING_DXIL_PATH="${SIR_D3D12_BINDING_DXIL}")
    target_link_libraries(SIRD3D12BindingTests PRIVATE SIR::D3D12 SIR::Graphics)
    add_test(NAME renderer.d3d12.bindings COMMAND SIRD3D12BindingTests)
    sir_configure_owned_target(SIRD3D12BindingTests)

    add_executable(SIRD3D12TransferTicketTests SIRD3D12TransferTicketTests.cpp)
    target_link_libraries(SIRD3D12TransferTicketTests PRIVATE SIR::D3D12)
    add_test(NAME renderer.d3d12.transfer-tickets COMMAND SIRD3D12TransferTicketTests)
    sir_configure_owned_target(SIRD3D12TransferTicketTests)
endif()
