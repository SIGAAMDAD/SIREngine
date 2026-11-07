# Native shader contract, material schema, GRDG source and pipeline fixtures.
add_executable(SIRShaderPipelineTests SIRShaderPipelineTests.cpp)
target_link_libraries(SIRShaderPipelineTests PRIVATE SIR::Graphics)
add_test(NAME renderer.shader.pipeline COMMAND SIRShaderPipelineTests)
sir_configure_owned_target(SIRShaderPipelineTests)

foreach(sirShaderUnit IN ITEMS SIRShaderContractsTests SIRShaderMaterialManifestTests SIRShaderGRDGTests)
    add_executable(${sirShaderUnit} Shaders/${sirShaderUnit}.cpp)
    target_link_libraries(${sirShaderUnit} PRIVATE SIR::Graphics)
    add_test(NAME renderer.shader.${sirShaderUnit} COMMAND ${sirShaderUnit})
    sir_configure_owned_target(${sirShaderUnit})
endforeach()

# Terrain3D Sparse VT V1 package contract and reference decoder.
add_executable(SIRVirtualTexturePortTests SIRVirtualTexturePortTests.cpp)
target_link_libraries(SIRVirtualTexturePortTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.terrain3d-v1 COMMAND SIRVirtualTexturePortTests)
sir_configure_owned_target(SIRVirtualTexturePortTests)

# Bounded metadata-only V1 lookup and malformed-directory rejection.
add_executable(SIRVirtualTexturePackLookupTests SIRVirtualTexturePackLookupTests.cpp)
target_link_libraries(SIRVirtualTexturePackLookupTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.sparse-v1-metadata COMMAND SIRVirtualTexturePackLookupTests)
sir_configure_owned_target(SIRVirtualTexturePackLookupTests)

# Native Hi-Z reference and command validation.
include(${CMAKE_CURRENT_SOURCE_DIR}/HIZFoundationTests.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/WorldHLODTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/WorldPartitionTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingReconcileTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingProviderBridgeTests.cmake)

