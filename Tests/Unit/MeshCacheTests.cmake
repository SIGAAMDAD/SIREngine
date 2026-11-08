# Standalone mesh cache tests deliberately do not link Core, GRDG or a GPU.
add_executable(SIRMeshCacheTests SIRMeshCacheTests.cpp)
target_link_libraries(SIRMeshCacheTests PRIVATE SIR::MeshCache)
add_test(NAME mesh.cache.foundation COMMAND SIRMeshCacheTests)
sir_configure_owned_target(SIRMeshCacheTests)

add_executable(SIRMeshCacheStateTests SIRMeshCacheStateTests.cpp)
target_link_libraries(SIRMeshCacheStateTests PRIVATE SIR::MeshCache)
add_test(NAME mesh.cache.state-model COMMAND SIRMeshCacheStateTests)
sir_configure_owned_target(SIRMeshCacheStateTests)

add_executable(SIRMeshCacheEdgeTests SIRMeshCacheEdgeTests.cpp)
target_link_libraries(SIRMeshCacheEdgeTests PRIVATE SIR::MeshCache)
add_test(NAME mesh.cache.edge-contract COMMAND SIRMeshCacheEdgeTests)
sir_configure_owned_target(SIRMeshCacheEdgeTests)

add_executable(SIRMeshCacheGenerationTests
    ${PROJECT_SOURCE_DIR}/Source/Mesh/SIRMeshCache.cpp
    SIRMeshCacheGenerationTests.cpp)
target_include_directories(SIRMeshCacheGenerationTests PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_definitions(SIRMeshCacheGenerationTests PRIVATE SIR_MESH_TEST_GENERATION_SEED=4294967293U)
add_test(NAME mesh.cache.generation-exhaustion COMMAND SIRMeshCacheGenerationTests)
sir_configure_owned_target(SIRMeshCacheGenerationTests)

add_executable(SIRMeshCacheBorrowLimitTests
    ${PROJECT_SOURCE_DIR}/Source/Mesh/SIRMeshCache.cpp
    SIRMeshCacheBorrowLimitTests.cpp)
target_include_directories(SIRMeshCacheBorrowLimitTests PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_definitions(SIRMeshCacheBorrowLimitTests PRIVATE SIR_MESH_TEST_MAX_BORROWS=2U)
add_test(NAME mesh.cache.borrow-limit COMMAND SIRMeshCacheBorrowLimitTests)
sir_configure_owned_target(SIRMeshCacheBorrowLimitTests)

add_executable(SIRMeshCacheAllocationTests SIRMeshCacheAllocationTests.cpp)
target_link_libraries(SIRMeshCacheAllocationTests PRIVATE SIR::MeshCache)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(SIRMeshCacheAllocationTests PRIVATE "LINKER:--wrap=malloc" "LINKER:--wrap=calloc" "LINKER:--wrap=realloc")
endif()
add_test(NAME mesh.cache.allocation-free COMMAND SIRMeshCacheAllocationTests)
sir_configure_owned_target(SIRMeshCacheAllocationTests)

add_executable(SIRMeshCacheOwnerLimitTests
    ${PROJECT_SOURCE_DIR}/Source/Mesh/SIRMeshCache.cpp
    SIRMeshCacheOwnerLimitTests.cpp)
target_include_directories(SIRMeshCacheOwnerLimitTests PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_definitions(SIRMeshCacheOwnerLimitTests PRIVATE SIR_MESH_TEST_OWNER_ID_SEED=18446744073709551614ULL)
add_test(NAME mesh.cache.owner-id-limit COMMAND SIRMeshCacheOwnerLimitTests)
sir_configure_owned_target(SIRMeshCacheOwnerLimitTests)

add_executable(SIRMeshCacheOwnerConcurrencyTests SIRMeshCacheOwnerConcurrencyTests.cpp)
target_link_libraries(SIRMeshCacheOwnerConcurrencyTests PRIVATE SIR::MeshCache SIR::Core)
add_test(NAME mesh.cache.owner-concurrency COMMAND SIRMeshCacheOwnerConcurrencyTests)
sir_configure_owned_target(SIRMeshCacheOwnerConcurrencyTests)

add_executable(SIRVGFramePlanTests SIRVGFramePlanTests.cpp)
target_link_libraries(SIRVGFramePlanTests PRIVATE SIR::VGFramePlan)
add_test(NAME mesh.vg.frame-plan COMMAND SIRVGFramePlanTests)
sir_configure_owned_target(SIRVGFramePlanTests)

# Canonical MeshCache key identity, source-revision overlap and shared geometry leases.
add_executable(SIRMeshCacheIdentityTests SIRMeshCacheIdentityTests.cpp)
target_link_libraries(SIRMeshCacheIdentityTests PRIVATE SIR::MeshCache)
add_test(NAME mesh.cache.identity-revision COMMAND SIRMeshCacheIdentityTests)
sir_configure_owned_target(SIRMeshCacheIdentityTests)

add_executable(SIRVGContractsTests SIRVGContractsTests.cpp)
target_link_libraries(SIRVGContractsTests PRIVATE SIR::VGContracts)
add_test(NAME mesh.vg.contracts COMMAND SIRVGContractsTests)
sir_configure_owned_target(SIRVGContractsTests)
