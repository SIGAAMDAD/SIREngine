include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")

# Offline SIRPack V1 writer; runtime reads remain in SIR::Resources.
add_library(SIRPackCooker STATIC SIRPackCooker.cpp)
add_library(SIR::PackCooker ALIAS SIRPackCooker)
target_include_directories(SIRPackCooker PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_features(SIRPackCooker PUBLIC cxx_std_17)
target_link_libraries(SIRPackCooker PUBLIC SIR::Resources)
sir_configure_owned_target(SIRPackCooker)
