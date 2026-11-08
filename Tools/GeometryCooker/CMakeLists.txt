include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")

if(NOT SIR_WITH_MESHOPTIMIZER)
    message(STATUS "SIRGeometryCooker disabled: enable SIR_WITH_MESHOPTIMIZER with pinned submodule")
    return()
endif()

add_library(SIRGeometryCooker STATIC SIRGeometryCooker.cpp)
add_library(SIR::GeometryCooker ALIAS SIRGeometryCooker)
target_include_directories(SIRGeometryCooker PUBLIC "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/Source")
target_compile_features(SIRGeometryCooker PUBLIC cxx_std_17)
target_link_libraries(SIRGeometryCooker PUBLIC SIR::MeshPackSchema SIR::PackCooker SIR::MeshOptimizer)
sir_configure_owned_target(SIRGeometryCooker)

add_executable(sir-geometry-cooker SIRGeometryCookerCLI.cpp)
target_compile_features(sir-geometry-cooker PRIVATE cxx_std_17)
target_link_libraries(sir-geometry-cooker PRIVATE SIR::GeometryCooker)
sir_configure_owned_target(sir-geometry-cooker)
