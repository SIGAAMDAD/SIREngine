include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")
# Shared, platform-independent rendering ABI. Always compiled, even when no
# native driver is enabled, so backends cannot silently fork its contracts.
add_library(SIRGraphics STATIC
    Graph/SIRFrameGraph.cpp
    Graph/SIRFrameGraphTransient.cpp
    Graph/SIRFrameGraphSubmission.cpp
    Graph/SIRFrameGraphCompletion.cpp
    Graph/SIRFrameGraphNative.cpp
    Graph/SIRFrameGraphDeviceBridge.cpp
    SIRShaderPipeline.cpp
    Shaders/SIRShaderContracts.cpp
    Shaders/SIRShaderMaterial.cpp
    Shaders/SIRShaderMaterialManifest.cpp
    Shaders/SIRShaderGRDG.cpp
    Shaders/SIRShaderLibrary.cpp
    Visibility/SIRVisibilityFoundation.cpp
    Visibility/SIRHiZDispatch.cpp
    Visibility/SIRHiZHistory.cpp
    Visibility/SIRHiZCommands.cpp
    Visibility/SIRHiZBindings.cpp
    SIRGraphicsDriverABI.cpp
    SIRWindowSurfaceAdapter.cpp
    SIRWindowABISurface.cpp
    SIRGraphicsModernRHI.cpp
    SIRGraphicsEraRHI.cpp
    SIRModernFrameGraphExecution.cpp
    SIRGraphicsModule.cpp
    SIRGraphicsPipelineABI.cpp
    SIRGraphicsPipelineBridge.cpp
    SIRGraphicsRegistry.cpp
    SIRSharedGPUBackingOwner.cpp
    SIRGraphicsValidation.cpp)
add_library(SIR::Graphics ALIAS SIRGraphics)
target_include_directories(SIRGraphics PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRGraphics PUBLIC cxx_std_17)
target_link_libraries(SIRGraphics PUBLIC SIR::Core SIR::Resources)
sir_configure_owned_target(SIRGraphics)


# -- VirtualTexturing --
# Native Terrain3D Sparse VT V1 port. No prototype VT source is compiled.
target_sources(SIRGraphics PRIVATE
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/SIRVirtualTextureFeedback.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/SIRVirtualTexturePackage.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/SIRVirtualTexturePackReader.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/SIRVirtualTexturePackFileSource.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/SIRVirtualTexturePackWriter.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/SIRVirtualTextureClusterDecode.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/Terrain3D/src/rendering/rd/vt/streaming/terrain_3d_vt_spatial_demand.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/Terrain3D/src/authoring/vt/visibility/terrain_3d_vt_visibility_aggregator.cpp)
target_include_directories(SIRGraphics PRIVATE "${PROJECT_SOURCE_DIR}/Source/Renderer/VirtualTexturing/Terrain3D/src")


# Each native driver implements the same SIRRenderingDevice contract.
# Their opt-in flags and targets are owned by the driver branches.
if(SIR_WITH_D3D12)
    if(NOT WIN32)
    message(FATAL_ERROR "SIR_WITH_D3D12 requires Windows")
endif()

set(SIR_D3D12MA_ROOT "${PROJECT_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator")
if(NOT EXISTS "${SIR_D3D12MA_ROOT}/src/D3D12MemAlloc.cpp" OR
   NOT EXISTS "${SIR_D3D12MA_ROOT}/include/D3D12MemAlloc.h")
    message(FATAL_ERROR "SIR_WITH_D3D12 requires the pinned ThirdParty/D3D12MemoryAllocator submodule")
endif()

add_library(SIRD3D12 STATIC
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Device.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Diagnostics.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Driver.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Memory.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Resources.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Pipelines.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Bindings.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Commands.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Surfaces.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D12/SIRD3D12Transfers.cpp
    "${SIR_D3D12MA_ROOT}/src/D3D12MemAlloc.cpp")
add_library(SIR::D3D12 ALIAS SIRD3D12)
target_include_directories(SIRD3D12 PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_include_directories(SIRD3D12 PUBLIC "${SIR_D3D12MA_ROOT}/include")
target_link_libraries(SIRD3D12 PUBLIC SIR::Graphics SIR::Core d3d12 dxgi d3dcompiler)
target_compile_features(SIRD3D12 PUBLIC cxx_std_17)
sir_configure_owned_target(SIRD3D12)
endif()
if(SIR_WITH_VULKAN)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanHeaders/include/vulkan/vulkan.h")
    message(FATAL_ERROR "Missing pinned Vulkan-Headers; run git submodule update --init ThirdParty/VulkanHeaders")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanMemoryAllocator/include/vk_mem_alloc.h")
    message(FATAL_ERROR "Missing pinned VMA; run git submodule update --init ThirdParty/VulkanMemoryAllocator")
endif()

add_library(SIRVulkan STATIC
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanDevice.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanPools.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanShaders.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanPipelines.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanRaster.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanDraw.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanDescriptors.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanBindings.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanCommands.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanRecordedCommands.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanGraphics.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanImages.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanTextures.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanTextureTransfer.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanBarriers.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanBufferBarriers.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/Vulkan/SIRVulkanTransferLayout.cpp)
add_library(SIR::Vulkan ALIAS SIRVulkan)
target_include_directories(SIRVulkan PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanHeaders/include"
    "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanMemoryAllocator/include")
target_link_libraries(SIRVulkan PUBLIC SIR::Core SIR::Graphics)
target_compile_features(SIRVulkan PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(SIRVulkan PRIVATE /W4)
else()
    target_compile_options(SIRVulkan PRIVATE -Wall -Wextra -Wpedantic)
endif()
sir_configure_owned_target(SIRVulkan)
endif()

# Opt-in legacy drivers and OpenGL 4.6 consume the same shared renderer contract.

# -- Drivers/D3D9 --
add_library(SIRD3D9Portable STATIC ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9UploadRing.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Validation.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Resources.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9FixedPipelines.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Mapping.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Barriers.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Attachments.cpp)
target_include_directories(SIRD3D9Portable PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRD3D9Portable PUBLIC cxx_std_17)
target_link_libraries(SIRD3D9Portable PUBLIC SIR::Graphics)
sir_configure_owned_target(SIRD3D9Portable)
if(SIR_WITH_D3D9)
    if(NOT WIN32)
        message(FATAL_ERROR "SIR_WITH_D3D9 requires Windows and the Windows SDK")
    endif()
    include(CheckIncludeFileCXX)
    check_include_file_cxx("d3d9.h" SIR_D3D9_HEADER_FOUND)
    if(NOT SIR_D3D9_HEADER_FOUND)
        message(FATAL_ERROR "SIR_WITH_D3D9 requires Windows SDK d3d9.h")
    endif()
    add_library(SIRD3D9 MODULE ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Driver.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9ABIBridge.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Native.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Formats.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Targets.cpp)
    target_compile_definitions(SIRD3D9 PRIVATE SIR_D3D9_EXPORTS=1)
    target_include_directories(SIRD3D9 PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_link_libraries(SIRD3D9 PRIVATE SIR::Graphics SIRD3D9Portable d3d9 dxguid user32)
    target_compile_features(SIRD3D9 PRIVATE cxx_std_17)
    sir_configure_owned_target(SIRD3D9)

endif()


# -- Drivers/D3D10 --
# The native module is opt-in. The CPU ABI stub is built only for tests.
if(SIR_WITH_D3D10)
    if(NOT WIN32)
        message(FATAL_ERROR "SIR_WITH_D3D10 requires Windows and the Windows SDK")
    endif()
    include(CheckIncludeFileCXX)
    check_include_file_cxx("d3d10.h" SIR_D3D10_HEADER_FOUND)
    check_include_file_cxx("d3d10_1.h" SIR_D3D10_1_HEADER_FOUND)
    check_include_file_cxx("dxgi.h" SIR_D3D10_DXGI_HEADER_FOUND)
    if(NOT SIR_D3D10_HEADER_FOUND OR NOT SIR_D3D10_1_HEADER_FOUND OR NOT SIR_D3D10_DXGI_HEADER_FOUND)
        message(FATAL_ERROR "SIR_WITH_D3D10 requires Windows SDK d3d10.h, d3d10_1.h and dxgi.h")
    endif()
    add_library(SIRD3D10 MODULE ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D10/SIRD3D10Driver.cpp)
    target_compile_definitions(SIRD3D10 PRIVATE SIR_D3D10_EXPORTS=1)
    target_include_directories(SIRD3D10 PRIVATE
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRD3D10 PRIVATE cxx_std_17)
    target_link_libraries(SIRD3D10 PRIVATE SIR::Graphics d3d10 d3d10_1 dxgi)
    sir_configure_owned_target(SIRD3D10)
endif()


# -- Drivers/D3D11 --
# Portable feature-level admission stays testable without a Windows SDK.
add_library(SIRD3D11FeatureProfile STATIC EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11FeatureLevels.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11FormatValidation.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11Handles.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11BufferValidation.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11TextureValidation.cpp)
add_library(SIR::D3D11FeatureProfile ALIAS SIRD3D11FeatureProfile)
target_include_directories(SIRD3D11FeatureProfile PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRD3D11FeatureProfile PUBLIC cxx_std_17)
sir_configure_owned_target(SIRD3D11FeatureProfile)
if(SIR_WITH_D3D11)
    if(NOT WIN32)
        message(FATAL_ERROR "SIR_WITH_D3D11 requires Windows and the Windows SDK")
    endif()

    include(CheckIncludeFileCXX)
    check_include_file_cxx("d3d11.h" SIR_D3D11_SDK_HEADER_FOUND)
    check_include_file_cxx("dxgi.h" SIR_D3D11_DXGI_HEADER_FOUND)
    if(NOT SIR_D3D11_SDK_HEADER_FOUND OR NOT SIR_D3D11_DXGI_HEADER_FOUND)
        message(FATAL_ERROR "SIR_WITH_D3D11 requires Windows SDK d3d11.h and dxgi.h")
    endif()

    add_library(SIRD3D11 STATIC ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11Driver.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11Device.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11Buffers.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11Formats.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D11/SIRD3D11Textures.cpp)
    add_library(SIR::D3D11 ALIAS SIRD3D11)
    target_include_directories(SIRD3D11 PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRD3D11 PUBLIC cxx_std_17)
    target_link_libraries(SIRD3D11 PUBLIC SIR::Graphics SIR::D3D11FeatureProfile
        PRIVATE d3d11 dxgi)
    sir_configure_owned_target(SIRD3D11)
endif()

if(SIR_WITH_OPENGL)
    # Native OpenGL 4.6 core-profile module; never adds GL types to SIR::Graphics.
add_library(SIRGL46Driver MODULE
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLDriver.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLContext.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLCapabilities.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLHandlePool.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLBuffers.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLSamplers.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLTextures.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFormats.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFramebuffers.cpp
    ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFramebufferCache.cpp)
target_include_directories(SIRGL46Driver PRIVATE
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRGL46Driver PRIVATE cxx_std_17)
if(WIN32)
    target_link_libraries(SIRGL46Driver PRIVATE SIR::Core opengl32 gdi32 user32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(SIRGL46Driver PRIVATE "${CMAKE_DL_LIBS}")
else()
    message(FATAL_ERROR "SIR_WITH_OPENGL currently supports Windows and Linux only")
endif()
sir_configure_owned_target(SIRGL46Driver)
endif()
