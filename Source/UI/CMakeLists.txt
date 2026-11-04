# SIREngine-owned retained UI tree; platform window and draw implementation
# are connected via the normal platform and rendering ownership contracts.
add_library(SIRUI STATIC SIRControlTree.cpp)
add_library(SIR::UI ALIAS SIRUI)
target_include_directories(SIRUI PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRUI PUBLIC cxx_std_17)
target_link_libraries(SIRUI PUBLIC SIR::Core)
sir_configure_owned_target(SIRUI)
