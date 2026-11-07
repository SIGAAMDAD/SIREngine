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

# Native Hi-Z reference and command validation.
include(${CMAKE_CURRENT_SOURCE_DIR}/HIZFoundationTests.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/WorldHLODTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/WorldPartitionTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingReconcileTests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/StreamingProviderBridgeTests.cmake)


# Direct upstream Terrain3D tests and source, preserved without algorithm rewrites.
set(sirVTDonorRoot "${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/Terrain3D/src")
foreach(sirVTDonorTest IN ITEMS test_vt_feedback_reducer test_vt_feedback_multi)
    add_executable(${sirVTDonorTest} "${sirVTDonorRoot}/diagnostics/tests/${sirVTDonorTest}.cpp")
    target_include_directories(${sirVTDonorTest} PRIVATE "${sirVTDonorRoot}")
    target_link_libraries(${sirVTDonorTest} PRIVATE SIR::Graphics)
    add_test(NAME renderer.virtual-texturing.donor.${sirVTDonorTest} COMMAND ${sirVTDonorTest})
    sir_configure_owned_target(${sirVTDonorTest})
endforeach()
add_executable(SIRTerrain3DDonorSpatialDemandTests SIRTerrain3DDonorSpatialDemandTests.cpp)
target_include_directories(SIRTerrain3DDonorSpatialDemandTests PRIVATE "${sirVTDonorRoot}")
target_link_libraries(SIRTerrain3DDonorSpatialDemandTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.donor.spatial-demand COMMAND SIRTerrain3DDonorSpatialDemandTests)
sir_configure_owned_target(SIRTerrain3DDonorSpatialDemandTests)

add_executable(SIRTerrain3DDonorVisibilityAggregationTests SIRTerrain3DDonorVisibilityAggregationTests.cpp)
target_include_directories(SIRTerrain3DDonorVisibilityAggregationTests PRIVATE "${sirVTDonorRoot}")
target_link_libraries(SIRTerrain3DDonorVisibilityAggregationTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.donor.visibility-aggregation COMMAND SIRTerrain3DDonorVisibilityAggregationTests)
sir_configure_owned_target(SIRTerrain3DDonorVisibilityAggregationTests)

# Unmodified Terrain3D input, feedback-pair and compact-tail contracts.
foreach(sirVTDonorTest IN ITEMS
        test_vt_feedback_epoch
        test_vt_upload_preflight
        test_vt_feedback_params
        test_vt_feedback_parity
        test_vt_feedback_shadow)
    add_executable(${sirVTDonorTest} "${sirVTDonorRoot}/diagnostics/tests/${sirVTDonorTest}.cpp")
    target_include_directories(${sirVTDonorTest} PRIVATE "${sirVTDonorRoot}")
    target_link_libraries(${sirVTDonorTest} PRIVATE SIR::Graphics)
    add_test(NAME renderer.virtual-texturing.donor.${sirVTDonorTest} COMMAND ${sirVTDonorTest})
    sir_configure_owned_target(${sirVTDonorTest})
endforeach()
add_executable(SIRTerrain3DDonorTailLayoutTests "${sirVTDonorRoot}/diagnostics/tests/superterrain/residency/test_tail_layout.cpp")
target_include_directories(SIRTerrain3DDonorTailLayoutTests PRIVATE "${sirVTDonorRoot}")
target_link_libraries(SIRTerrain3DDonorTailLayoutTests PRIVATE SIR::Graphics)
add_test(NAME renderer.virtual-texturing.donor.compact-tail COMMAND SIRTerrain3DDonorTailLayoutTests)
sir_configure_owned_target(SIRTerrain3DDonorTailLayoutTests)
