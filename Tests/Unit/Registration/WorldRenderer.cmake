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

add_executable(SIRVirtualTextureReferenceEncoderTests SIRVirtualTextureReferenceEncoderTests.cpp)
target_link_libraries(SIRVirtualTextureReferenceEncoderTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.reference-encoder COMMAND SIRVirtualTextureReferenceEncoderTests)
sir_configure_owned_target(SIRVirtualTextureReferenceEncoderTests)

# Terrain3D four-slot baked visibility bundle scheduling and stale completion protection.
add_executable(SIRVirtualTextureDemandQueueTests SIRVirtualTextureDemandQueueTests.cpp)
target_link_libraries(SIRVirtualTextureDemandQueueTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.demand-queue COMMAND SIRVirtualTextureDemandQueueTests)
sir_configure_owned_target(SIRVirtualTextureDemandQueueTests)

# Original Terrain3D baked-visibility percentile and runtime mip projection.
add_executable(SIRVirtualTextureDemandMipTests SIRVirtualTextureDemandMipTests.cpp)
target_link_libraries(SIRVirtualTextureDemandMipTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.demand-mip COMMAND SIRVirtualTextureDemandMipTests)
sir_configure_owned_target(SIRVirtualTextureDemandMipTests)

# Donor V2 stable-material spatial demand, externally owned four-cell hot cache.
add_executable(SIRVirtualTextureSpatialDemandTests SIRVirtualTextureSpatialDemandTests.cpp)
target_link_libraries(SIRVirtualTextureSpatialDemandTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.spatial-demand COMMAND SIRVirtualTextureSpatialDemandTests)
sir_configure_owned_target(SIRVirtualTextureSpatialDemandTests)

# Bounded baked-visibility PackBits/raw bundle decoding, with negative-path tests.
add_executable(SIRVirtualTextureDemandBundleSmokeTests SIRVirtualTextureDemandBundleSmokeTests.cpp)
target_link_libraries(SIRVirtualTextureDemandBundleSmokeTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.demand-bundle-codec COMMAND SIRVirtualTextureDemandBundleSmokeTests)
sir_configure_owned_target(SIRVirtualTextureDemandBundleSmokeTests)

if(SIR_WITH_VT_ZSTD)
    add_executable(SIRVirtualTextureZstdCodecTests SIRVirtualTextureZstdCodecTests.cpp)
    target_link_libraries(SIRVirtualTextureZstdCodecTests PRIVATE SIR::Graphics libzstd_static)
    add_test(NAME renderer.virtual-texturing.zstd-cluster COMMAND SIRVirtualTextureZstdCodecTests)
    sir_configure_owned_target(SIRVirtualTextureZstdCodecTests)
endif()

# Bounded metadata-only V1 lookup and malformed-directory rejection.
add_executable(SIRVirtualTexturePackLookupTests SIRVirtualTexturePackLookupTests.cpp)
target_link_libraries(SIRVirtualTexturePackLookupTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.sparse-v1-metadata COMMAND SIRVirtualTexturePackLookupTests)
sir_configure_owned_target(SIRVirtualTexturePackLookupTests)

# Donor V1 mip-chain and 136x136 physical-tile gutter cooker parity.
add_executable(SIRVirtualTextureTileCookerTests SIRVirtualTextureTileCookerTests.cpp)
target_link_libraries(SIRVirtualTextureTileCookerTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.sparse-v1-cooker COMMAND SIRVirtualTextureTileCookerTests)
sir_configure_owned_target(SIRVirtualTextureTileCookerTests)

# Terrain3D V2/V3 atomic feedback reducer and priority/parent promotion tests.
add_executable(SIRVirtualTextureFeedbackReducerTests SIRVirtualTextureFeedbackReducerTests.cpp)
target_link_libraries(SIRVirtualTextureFeedbackReducerTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.feedback-reducer COMMAND SIRVirtualTextureFeedbackReducerTests)
sir_configure_owned_target(SIRVirtualTextureFeedbackReducerTests)

# Terrain3D Sparse V1 fixed quality profiles, authored extents and base-mip selection.
add_executable(SIRVirtualTextureFixedProfileTests SIRVirtualTextureFixedProfileTests.cpp)
target_link_libraries(SIRVirtualTextureFixedProfileTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.fixed-profile COMMAND SIRVirtualTextureFixedProfileTests)
sir_configure_owned_target(SIRVirtualTextureFixedProfileTests)

# Decoded V1 per-tile payload bounds, physical bytes and corruption checks.
add_executable(SIRVirtualTextureDecodedPayloadTests SIRVirtualTextureDecodedPayloadTests.cpp)
target_link_libraries(SIRVirtualTextureDecodedPayloadTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.decoded-payload COMMAND SIRVirtualTextureDecodedPayloadTests)
sir_configure_owned_target(SIRVirtualTextureDecodedPayloadTests)

# Native Hi-Z reference and command validation.
include(${CMAKE_CURRENT_SOURCE_DIR}/HIZFoundationTests.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/WorldHLODTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/WorldPartitionTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingReconcileTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingProviderBridgeTests.cmake)

