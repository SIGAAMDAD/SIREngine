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

# Versioned mesh-owned GRDG/provider/graphics boundary checks; no native device ownership.
add_library(SIRVGContracts STATIC SIRVGContracts.cpp)
add_library(SIR::VGContracts ALIAS SIRVGContracts)
target_include_directories(SIRVGContracts PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRVGContracts PUBLIC cxx_std_17)
sir_configure_owned_target(SIRVGContracts)

# Immutable, versioned SIRG geometry manifest. Runtime-only reader/validator.
add_library(SIRMeshPackSchema STATIC SIRMeshPackSchema.cpp)
add_library(SIR::MeshPackSchema ALIAS SIRMeshPackSchema)
target_include_directories(SIRMeshPackSchema PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshPackSchema PUBLIC cxx_std_17)
target_link_libraries(SIRMeshPackSchema PUBLIC SIR::Resources)
sir_configure_owned_target(SIRMeshPackSchema)

# Bind immutable MeshCache leases directly to GRDG source epochs and provider pins.
# GRDG owns every physical payload allocation; this consumer owns no backing.
add_library(SIRMeshGRDGSource STATIC SIRMeshGRDGSource.cpp)
add_library(SIR::MeshGRDGSource ALIAS SIRMeshGRDGSource)
target_include_directories(SIRMeshGRDGSource PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshGRDGSource PUBLIC cxx_std_17)
target_link_libraries(SIRMeshGRDGSource PUBLIC SIR::MeshCache SIR::Resources)
sir_configure_owned_target(SIRMeshGRDGSource)
