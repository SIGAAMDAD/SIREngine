# Spatial demand generation, independent of resource payload ownership.
add_library(SIRWorld STATIC Partition/SIRWorldPartition.cpp)
add_library(SIR::World ALIAS SIRWorld)
target_include_directories(SIRWorld PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRWorld PUBLIC cxx_std_17)
target_link_libraries(SIRWorld PUBLIC SIR::Streaming)
sir_configure_owned_target(SIRWorld)
