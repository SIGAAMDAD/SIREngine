include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native Terrain3D Sparse VT V1 port. No prototype VT source is compiled.
target_sources(SIRGraphics PRIVATE
    SIRVirtualTextureFeedback.cpp
    SIRVirtualTexturePackage.cpp
    SIRVirtualTexturePackReader.cpp
    SIRVirtualTexturePackFileSource.cpp
    SIRVirtualTexturePackWriter.cpp
    SIRVirtualTextureClusterDecode.cpp)
