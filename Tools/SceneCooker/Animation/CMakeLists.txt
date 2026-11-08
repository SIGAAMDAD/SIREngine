include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
add_library(SIRAnimationCook STATIC SIRAnimationCooker.cpp)
add_library(SIR::AnimationCook ALIAS SIRAnimationCook)
target_link_libraries(SIRAnimationCook PUBLIC SIR::Animation)
target_include_directories(SIRAnimationCook PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Tools"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRAnimationCook PUBLIC cxx_std_17)
sir_configure_owned_target(SIRAnimationCook)
