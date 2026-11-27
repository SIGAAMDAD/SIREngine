# ===========================================================================
# SIREngine MPLv2 Source Code
# Copyright (C) 2023-2027 Noah Van Til
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# This software is provided "as is", without warranty of any kind,
# express or implied, including but not limited to the warranties
# of merchantability, fitness for a particular purpose and noninfringement.
# ===========================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")
# SIREngine-owned retained UI tree; platform window and draw implementation
# are connected via the normal platform and rendering ownership contracts.
add_library(SIRUI STATIC SIRControlTree.cpp)
add_library(SIR::UI ALIAS SIRUI)
target_include_directories(SIRUI PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRUI PUBLIC cxx_std_17)
target_link_libraries(SIRUI PUBLIC SIR::Core)
sir_configure_owned_target(SIRUI)

if(SIR_WITH_IMGUI_VULKAN)
    if(NOT SIR_WITH_VULKAN)
        message(FATAL_ERROR "SIR_WITH_IMGUI_VULKAN requires SIR_WITH_VULKAN=ON")
    endif()
    set(SIR_IMGUI_ROOT "${PROJECT_SOURCE_DIR}/ThirdParty/imgui")
    if(NOT EXISTS "${SIR_IMGUI_ROOT}/imgui.h" OR
       NOT EXISTS "${SIR_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp")
        message(FATAL_ERROR "Missing pinned Dear ImGui; run git submodule update --init ThirdParty/imgui")
    endif()

    add_library(SIRImGuiVulkan STATIC
        "${SIR_IMGUI_ROOT}/imgui.cpp"
        "${SIR_IMGUI_ROOT}/imgui_draw.cpp"
        "${SIR_IMGUI_ROOT}/imgui_tables.cpp"
        "${SIR_IMGUI_ROOT}/imgui_widgets.cpp"
        "${SIR_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp"
        SIRImGuiWindowBridge.cpp)
    add_library(SIR::ImGuiVulkan ALIAS SIRImGuiVulkan)
    target_include_directories(SIRImGuiVulkan PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${SIR_IMGUI_ROOT}"
        "${SIR_IMGUI_ROOT}/backends")
    target_include_directories(SIRImGuiVulkan PRIVATE
        "${PROJECT_SOURCE_DIR}/ThirdParty/VulkanHeaders/include")
    target_link_libraries(SIRImGuiVulkan PUBLIC SIR::Core SIR::Vulkan)
    target_compile_features(SIRImGuiVulkan PUBLIC cxx_std_17)
    target_compile_definitions(SIRImGuiVulkan PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
endif()

if(SIR_WITH_IMGUI_OPENGL)
    if(NOT SIR_WITH_OPENGL)
        message(FATAL_ERROR "SIR_WITH_IMGUI_OPENGL requires SIR_WITH_OPENGL=ON")
    endif()
    set(SIR_IMGUI_ROOT "${PROJECT_SOURCE_DIR}/ThirdParty/imgui")
    if(NOT EXISTS "${SIR_IMGUI_ROOT}/imgui.h" OR
       NOT EXISTS "${SIR_IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp")
        message(FATAL_ERROR "Missing pinned Dear ImGui; run git submodule update --init ThirdParty/imgui")
    endif()
    add_library(SIRImGuiOpenGL STATIC
        "${SIR_IMGUI_ROOT}/imgui.cpp"
        "${SIR_IMGUI_ROOT}/imgui_draw.cpp"
        "${SIR_IMGUI_ROOT}/imgui_tables.cpp"
        "${SIR_IMGUI_ROOT}/imgui_widgets.cpp"
        "${SIR_IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp"
        SIRImGuiWindowBridge.cpp)
    add_library(SIR::ImGuiOpenGL ALIAS SIRImGuiOpenGL)
    target_include_directories(SIRImGuiOpenGL PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${SIR_IMGUI_ROOT}"
        "${SIR_IMGUI_ROOT}/backends")
    target_link_libraries(SIRImGuiOpenGL PUBLIC SIR::Core SIR::GLProgrammableRHI)
    target_compile_features(SIRImGuiOpenGL PUBLIC cxx_std_17)
endif()
