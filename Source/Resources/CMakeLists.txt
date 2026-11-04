# Native resource metadata and dependency ownership. Loading and GPU residency
# are integrated separately with SIREngine resource and rendering systems.
add_library(SIRResources STATIC SIRResourceCatalog.cpp)
add_library(SIR::Resources ALIAS SIRResources)
target_include_directories(SIRResources PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRResources PUBLIC cxx_std_17)
target_link_libraries(SIRResources PUBLIC SIR::Core)
sir_configure_owned_target(SIRResources)
