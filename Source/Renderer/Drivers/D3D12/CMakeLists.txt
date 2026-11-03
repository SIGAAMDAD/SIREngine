if(NOT WIN32)
    message(FATAL_ERROR "SIR_WITH_D3D12 requires Windows")
endif()

set(SIR_D3D12MA_ROOT "${PROJECT_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator")
if(NOT EXISTS "${SIR_D3D12MA_ROOT}/src/D3D12MemAlloc.cpp" OR
   NOT EXISTS "${SIR_D3D12MA_ROOT}/include/D3D12MemAlloc.h")
    message(FATAL_ERROR "SIR_WITH_D3D12 requires the pinned ThirdParty/D3D12MemoryAllocator submodule")
endif()

add_library(SIRD3D12 STATIC
    SIRD3D12Device.cpp
    SIRD3D12Memory.cpp
    SIRD3D12Transfers.cpp
    "${SIR_D3D12MA_ROOT}/src/D3D12MemAlloc.cpp")
add_library(SIR::D3D12 ALIAS SIRD3D12)
target_include_directories(SIRD3D12 PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_include_directories(SIRD3D12 PUBLIC "${SIR_D3D12MA_ROOT}/include")
target_link_libraries(SIRD3D12 PUBLIC d3d12 dxgi)
target_compile_features(SIRD3D12 PUBLIC cxx_std_17)
sir_configure_owned_target(SIRD3D12)
