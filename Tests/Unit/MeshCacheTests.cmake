# Standalone mesh cache tests deliberately do not link Core, GRDG or a GPU.
add_executable(SIRMeshCacheTests SIRMeshCacheTests.cpp)
target_link_libraries(SIRMeshCacheTests PRIVATE SIR::MeshCache)
add_test(NAME mesh.cache.foundation COMMAND SIRMeshCacheTests)
sir_configure_owned_target(SIRMeshCacheTests)
