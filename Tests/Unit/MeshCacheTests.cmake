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
