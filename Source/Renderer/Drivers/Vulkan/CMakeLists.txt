include("${CMAKE_CURRENT_LIST_DIR}/../../../../Build/CMake/SIRBuildTreeGuard.cmake")
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanHeaders/include/vulkan/vulkan.h")
    message(FATAL_ERROR "Missing pinned Vulkan-Headers; run git submodule update --init ThirdParty/VulkanHeaders")
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanMemoryAllocator/include/vk_mem_alloc.h")
    message(FATAL_ERROR "Missing pinned VMA; run git submodule update --init ThirdParty/VulkanMemoryAllocator")
endif()

add_library(SIRVulkan STATIC
    SIRVulkanDevice.cpp
    SIRVulkanPools.cpp
    SIRVulkanShaders.cpp
    SIRVulkanPipelines.cpp
    SIRVulkanRaster.cpp
    SIRVulkanDraw.cpp
    SIRVulkanDescriptors.cpp
    SIRVulkanBindings.cpp
    SIRVulkanCommands.cpp
    SIRVulkanRecordedCommands.cpp
    SIRVulkanGraphics.cpp
    SIRVulkanImages.cpp
    SIRVulkanTextures.cpp
    SIRVulkanTextureTransfer.cpp
    SIRVulkanBarriers.cpp
    SIRVulkanBufferBarriers.cpp
    SIRVulkanTransferLayout.cpp)
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
