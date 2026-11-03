if(NOT WIN32)
    message(FATAL_ERROR "SIR_WITH_D3D12 requires Windows")
endif()

add_library(SIRD3D12 STATIC SIRD3D12Device.cpp)
add_library(SIR::D3D12 ALIAS SIRD3D12)
target_include_directories(SIRD3D12 PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_link_libraries(SIRD3D12 PUBLIC d3d12 dxgi)
target_compile_features(SIRD3D12 PUBLIC cxx_std_17)
sir_configure_owned_target(SIRD3D12)
