# The native module is opt-in. The CPU ABI stub is built only for tests.
option(SIR_WITH_D3D10 "Build the native Windows Direct3D 10 backend" OFF)
if(SIR_WITH_D3D10)
    if(NOT WIN32)
        message(FATAL_ERROR "SIR_WITH_D3D10 requires Windows and the Windows SDK")
    endif()
    include(CheckIncludeFileCXX)
    check_include_file_cxx("d3d10.h" SIR_D3D10_HEADER_FOUND)
    check_include_file_cxx("d3d10_1.h" SIR_D3D10_1_HEADER_FOUND)
    check_include_file_cxx("dxgi.h" SIR_D3D10_DXGI_HEADER_FOUND)
    if(NOT SIR_D3D10_HEADER_FOUND OR NOT SIR_D3D10_1_HEADER_FOUND OR NOT SIR_D3D10_DXGI_HEADER_FOUND)
        message(FATAL_ERROR "SIR_WITH_D3D10 requires Windows SDK d3d10.h, d3d10_1.h and dxgi.h")
    endif()
    add_library(SIRD3D10 MODULE SIRD3D10Driver.cpp)
    target_compile_definitions(SIRD3D10 PRIVATE SIR_D3D10_EXPORTS=1)
    target_include_directories(SIRD3D10 PRIVATE
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRD3D10 PRIVATE cxx_std_17)
    target_link_libraries(SIRD3D10 PRIVATE SIR::Graphics d3d10 d3d10_1 dxgi)
    sir_configure_owned_target(SIRD3D10)
endif()

if(SIR_BUILD_TESTS)
    add_library(SIRD3D10ABIStub STATIC SIRD3D10Driver.cpp)
    target_include_directories(SIRD3D10ABIStub PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRD3D10ABIStub PUBLIC cxx_std_17)
    target_link_libraries(SIRD3D10ABIStub PUBLIC SIR::Graphics)
    sir_configure_owned_target(SIRD3D10ABIStub)

    add_executable(SIRD3D10ABITests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D10/SIRD3D10ABITests.cpp")
    target_link_libraries(SIRD3D10ABITests PRIVATE SIRD3D10ABIStub)
    sir_configure_owned_target(SIRD3D10ABITests)
    add_test(NAME renderer.d3d10.abi-scaffold COMMAND SIRD3D10ABITests)
endif()
