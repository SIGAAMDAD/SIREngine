include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native Terrain3D Sparse VT V1 port. No prototype VT source is compiled.
target_sources(SIRGraphics PRIVATE
    SIRVirtualTextureFeedback.cpp
    SIRVirtualTexturePackage.cpp
    SIRVirtualTexturePackReader.cpp
    SIRVirtualTexturePackFileSource.cpp
    SIRVirtualTexturePackWriter.cpp
    SIRVirtualTextureClusterDecode.cpp
    SIRVirtualTexturePackLookup.cpp
    SIRVirtualTextureTileCooker.cpp
    SIRVirtualTextureFeedbackReducer.cpp
    SIRVirtualTextureFixedProfile.cpp
    SIRVirtualTextureDecodedPayload.cpp
    SIRVirtualTextureReferenceEncoder.cpp
    SIRVirtualTextureDemandQueue.cpp
    SIRVirtualTextureDemandMip.cpp
    SIRVirtualTextureDemandBundleCodec.cpp)

# Optional pinned Terrain3D V1 Zstd cluster codec. Production must explicitly
# enable this target and initialize the approved submodule; no system fallback.
option(SIR_WITH_VT_ZSTD "Enable pinned Terrain3D Sparse V1 Zstd cluster decompression" OFF)
if(SIR_WITH_VT_ZSTD)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/Zstd/build/cmake/CMakeLists.txt")
        message(FATAL_ERROR "Missing pinned ThirdParty/Zstd; initialize the approved submodule")
    endif()
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "No Zstd command-line tools" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "No vendor Zstd tests" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Static Zstd for engine VT" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "Enable static Zstd" FORCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/ThirdParty/Zstd/build/cmake" "${PROJECT_BINARY_DIR}/zstd" EXCLUDE_FROM_ALL)
    target_sources(SIRGraphics PRIVATE SIRVirtualTextureZstdCodec.cpp)
    target_link_libraries(SIRGraphics PRIVATE libzstd_static)
endif()
