include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native Terrain3D Sparse VT V1 port. No prototype VT source is compiled.
target_sources(SIRGraphics PRIVATE
    SIRVirtualTextureFeedback.cpp
    SIRVirtualTexturePackage.cpp
    SIRVirtualTexturePackReader.cpp
    SIRVirtualTexturePackFileSource.cpp
    SIRVirtualTexturePackWriter.cpp
    SIRVirtualTextureClusterDecode.cpp
    Terrain3D/src/rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.cpp
    Terrain3D/src/authoring/vt/visibility/terrain_3d_vt_visibility_aggregator.cpp)
target_include_directories(SIRGraphics PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Terrain3D/src")
