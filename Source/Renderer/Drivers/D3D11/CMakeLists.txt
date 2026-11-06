include("${CMAKE_CURRENT_LIST_DIR}/../../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Portable feature-level admission stays testable without a Windows SDK.
add_library(SIRD3D11FeatureProfile STATIC EXCLUDE_FROM_ALL SIRD3D11FeatureLevels.cpp SIRD3D11FormatValidation.cpp)
add_library(SIR::D3D11FeatureProfile ALIAS SIRD3D11FeatureProfile)
target_include_directories(SIRD3D11FeatureProfile PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRD3D11FeatureProfile PUBLIC cxx_std_17)
sir_configure_owned_target(SIRD3D11FeatureProfile)

option(SIR_WITH_D3D11 "Build the native Windows Direct3D 11 backend" OFF)
if(SIR_WITH_D3D11)
    if(NOT WIN32)
        message(FATAL_ERROR "SIR_WITH_D3D11 requires Windows and the Windows SDK")
    endif()

    include(CheckIncludeFileCXX)
    check_include_file_cxx("d3d11.h" SIR_D3D11_SDK_HEADER_FOUND)
    check_include_file_cxx("dxgi.h" SIR_D3D11_DXGI_HEADER_FOUND)
    if(NOT SIR_D3D11_SDK_HEADER_FOUND OR NOT SIR_D3D11_DXGI_HEADER_FOUND)
        message(FATAL_ERROR "SIR_WITH_D3D11 requires Windows SDK d3d11.h and dxgi.h")
    endif()

    add_library(SIRD3D11 STATIC SIRD3D11Driver.cpp SIRD3D11Device.cpp SIRD3D11Formats.cpp)
    add_library(SIR::D3D11 ALIAS SIRD3D11)
    target_include_directories(SIRD3D11 PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRD3D11 PUBLIC cxx_std_17)
    target_link_libraries(SIRD3D11 PUBLIC SIR::Graphics SIR::D3D11FeatureProfile
        PRIVATE d3d11 dxgi)
    sir_configure_owned_target(SIRD3D11)
endif()
