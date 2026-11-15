include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native Terrain3D Sparse VT V1 port. No prototype VT source is compiled.
target_sources(SIRGraphics PRIVATE
    SIRVirtualTextureFeedback.cpp
    SIRVirtualTexturePackage.cpp
    SIRVirtualTexturePackReader.cpp
    SIRVirtualTexturePackFileSource.cpp
    SIRVirtualTexturePackWriter.cpp
    SIRVirtualTextureClusterDecode.cpp
    SIRVirtualTextureCooker.cpp
    SIRVirtualTextureRegionCooker.cpp
    SIRVirtualTextureZstdCodec.cpp
    # Source-preserved Terrain3D Sparse V1 cooking and disk-layout implementation.
    Terrain3D/src/data/formats/terrain_3d_vt_pack_reader.cpp
    Terrain3D/src/rendering/rd/vt/io/terrain_3d_vt_file_source.cpp
    Terrain3D/src/rendering/rd/vt/io/terrain_3d_vt_pack_runtime.cpp
    Terrain3D/src/authoring/vt/io/terrain_3d_vt_pack_writer.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_vt_page_builder.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_vt_reference_encoder.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_vt_cooker.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_composed_surface_cooker.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_composed_surface_static_baseline.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_composed_surface_pack_sampler.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_composed_surface_reference_decode.cpp
    Terrain3D/src/authoring/vt/build/terrain_3d_composed_surface_cold_pipeline.cpp
    Terrain3D/src/authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.cpp
    Terrain3D/src/authoring/vt/integration/terrain_3d_composed_surface_edit_journal.cpp
    Terrain3D/src/authoring/vt/integration/terrain_3d_composed_surface_recook_bridge.cpp
    Terrain3D/src/authoring/vt/integration/terrain_3d_composed_surface_editor_cook.cpp
    Terrain3D/src/rendering/rd/vt/runtime/terrain_3d_vt_fixed_profile.cpp
    Terrain3D/src/rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.cpp
    Terrain3D/src/authoring/vt/visibility/terrain_3d_vt_visibility_aggregator.cpp)
target_include_directories(SIRGraphics PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Terrain3D/src")

# Cold native cook uses the donor's original codec ABI and the approved Zstd
# dependency when provided by the platform toolchain. The wire bytes remain V1.
find_path(SIR_VT_ZSTD_INCLUDE_DIR NAMES zstd.h)
find_library(SIR_VT_ZSTD_LIBRARY NAMES zstd)
if(SIR_VT_ZSTD_INCLUDE_DIR AND SIR_VT_ZSTD_LIBRARY)
    target_compile_definitions(SIRGraphics PRIVATE SIR_VT_WITH_ZSTD=1)
    target_include_directories(SIRGraphics PRIVATE "${SIR_VT_ZSTD_INCLUDE_DIR}")
    target_link_libraries(SIRGraphics PRIVATE "${SIR_VT_ZSTD_LIBRARY}")
endif()
