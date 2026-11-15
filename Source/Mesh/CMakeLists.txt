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
add_library(SIRMeshGRDGIntegration STATIC SIRMeshGRDGIntegration.cpp)
add_library(SIR::MeshGRDGIntegration ALIAS SIRMeshGRDGIntegration)
target_include_directories(SIRMeshGRDGIntegration PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshGRDGIntegration PUBLIC cxx_std_17)
target_link_libraries(SIRMeshGRDGIntegration PUBLIC SIR::MeshCache SIR::Resources SIR::MeshPackSchema)
sir_configure_owned_target(SIRMeshGRDGIntegration)

# Immutable shared CPU collision source: backend-owned bodies reuse one pinned
# canonical MeshCache triangle allocation; physics retains acceleration ownership.
add_library(SIRMeshCollisionSource STATIC SIRMeshCollisionSource.cpp)
add_library(SIR::MeshCollisionSource ALIAS SIRMeshCollisionSource)
target_include_directories(SIRMeshCollisionSource PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshCollisionSource PUBLIC cxx_std_17)
target_link_libraries(SIRMeshCollisionSource PUBLIC SIR::MeshCache)
sir_configure_owned_target(SIRMeshCollisionSource)

# Caller-owned BVH metadata is a derived CPU query structure over shared pinned
# source triangle geometry; no duplicate vertex or index allocation.
add_library(SIRMeshCollisionBVH STATIC SIRMeshCollisionBVH.cpp)
add_library(SIR::MeshCollisionBVH ALIAS SIRMeshCollisionBVH)
target_include_directories(SIRMeshCollisionBVH PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshCollisionBVH PUBLIC cxx_std_17)
target_link_libraries(SIRMeshCollisionBVH PUBLIC SIR::MeshCollisionSource)
sir_configure_owned_target(SIRMeshCollisionBVH)

# Native, allocation-free logical residency and complete-group CPU LOD cut.
# The driver/GRDG own the single physical backing; neither target allocates it.
add_library(SIRVGPageDirectory STATIC SIRVGPageDirectory.cpp)
add_library(SIR::VGPageDirectory ALIAS SIRVGPageDirectory)
target_include_directories(SIRVGPageDirectory PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRVGPageDirectory PUBLIC cxx_std_17)
target_link_libraries(SIRVGPageDirectory PUBLIC SIR::MeshPackSchema)
sir_configure_owned_target(SIRVGPageDirectory)

# Legacy CPU-addressable page residency uses the identical cooked hierarchy.
# Client arrays and legacy native buffer uploads borrow decoded derivatives.
add_library(SIRVGCPUPageViews STATIC SIRVGCPUPageViews.cpp)
add_library(SIR::VGCPUPageViews ALIAS SIRVGCPUPageViews)
target_include_directories(SIRVGCPUPageViews PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRVGCPUPageViews PUBLIC cxx_std_17)
target_link_libraries(SIRVGCPUPageViews PUBLIC SIR::VGPageDirectory SIR::Resources)
sir_configure_owned_target(SIRVGCPUPageViews)

add_library(SIRVGFrameCompiler STATIC SIRVGFrameCompiler.cpp)
add_library(SIR::VGFrameCompiler ALIAS SIRVGFrameCompiler)
target_include_directories(SIRVGFrameCompiler PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRVGFrameCompiler PUBLIC cxx_std_17)
target_link_libraries(SIRVGFrameCompiler PUBLIC SIR::VGPageDirectory SIR::VGFramePlan)
sir_configure_owned_target(SIRVGFrameCompiler)

# One native read-only canonical BVH/material query for rendering, physics,
# audio rays, navigation and AI. No per-consumer source or BVH allocation.
add_library(SIRCommonGeometryQueries STATIC SIRCommonGeometryQueries.cpp)
add_library(SIR::CommonGeometryQueries ALIAS SIRCommonGeometryQueries)
target_include_directories(SIRCommonGeometryQueries PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRCommonGeometryQueries PUBLIC cxx_std_17)
target_link_libraries(SIRCommonGeometryQueries PUBLIC SIR::MeshCache SIR::MeshPackSchema)
sir_configure_owned_target(SIRCommonGeometryQueries)

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

# Immutable shared CPU collision source: backend-owned bodies reuse one pinned
# canonical MeshCache triangle allocation; physics retains acceleration ownership.
add_library(SIRMeshCollisionSource STATIC SIRMeshCollisionSource.cpp)
add_library(SIR::MeshCollisionSource ALIAS SIRMeshCollisionSource)
target_include_directories(SIRMeshCollisionSource PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshCollisionSource PUBLIC cxx_std_17)
target_link_libraries(SIRMeshCollisionSource PUBLIC SIR::MeshCache)
sir_configure_owned_target(SIRMeshCollisionSource)

# Caller-owned BVH metadata is a derived CPU query structure over shared pinned
# source triangle geometry; no duplicate vertex or index allocation.
add_library(SIRMeshCollisionBVH STATIC SIRMeshCollisionBVH.cpp)
add_library(SIR::MeshCollisionBVH ALIAS SIRMeshCollisionBVH)
target_include_directories(SIRMeshCollisionBVH PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRMeshCollisionBVH PUBLIC cxx_std_17)
target_link_libraries(SIRMeshCollisionBVH PUBLIC SIR::MeshCollisionSource)
sir_configure_owned_target(SIRMeshCollisionBVH)
