include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")

add_library(SIRAnimation STATIC
    Cooked/SIRAnimationCooked.cpp
    Runtime/SIRAnimationSampler.cpp
    Runtime/SIRAnimationSIMD.cpp
    Runtime/SIRAnimationSamplingJobs.cpp)
add_library(SIR::Animation ALIAS SIRAnimation)
target_link_libraries(SIRAnimation PUBLIC SIR::Core SIR::Resources)
target_include_directories(SIRAnimation PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRAnimation PUBLIC cxx_std_17)
sir_configure_owned_target(SIRAnimation)
