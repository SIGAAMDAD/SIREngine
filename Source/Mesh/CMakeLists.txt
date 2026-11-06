include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")
# Independent CPU-only shared-geometry ownership foundation.
# No GRDG, Jolt, Terrain3D, or graphics-driver link dependency.
add_library(SIRMeshCache STATIC SIRMeshCache.cpp)
add_library(SIR::MeshCache ALIAS SIRMeshCache)
target_include_directories(SIRMeshCache PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshCache PUBLIC cxx_std_17)
sir_configure_owned_target(SIRMeshCache)

# CPU-only, backend-neutral virtual-geometry frame-plan contract and validator.
add_library(SIRVGFramePlan STATIC SIRVGFramePlan.cpp)
add_library(SIR::VGFramePlan ALIAS SIRVGFramePlan)
target_include_directories(SIRVGFramePlan PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRVGFramePlan PUBLIC cxx_std_17)
sir_configure_owned_target(SIRVGFramePlan)
