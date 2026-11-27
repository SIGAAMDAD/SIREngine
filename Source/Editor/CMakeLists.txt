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
# Native SIREngine editor authoring services; source scene import is a codec.
add_library(SIREditorAuthoring STATIC
    Assets/SIRResourceCatalog.cpp
    Scene/SIREditorSceneDocument.cpp
    Scene/SIREditorTextScene.cpp
    Scene/SIREditorTextProperties.cpp
    Scene/SIREditorTextWriter.cpp
    Scene/SIREditorComplexWriter.cpp
    Scene/SIREditorScalarCodec.cpp
    Scene/SIREditorScalarBinding.cpp
    Scene/SIREditorResourceReferences.cpp
    Scene/SIREditorCatalogBridge.cpp
    Scene/SIREditorResourceCatalogKeys.cpp
    Scene/SIREditorResourceGraphBridge.cpp
    Scene/SIREditorFileSave.cpp
    Widgets/SIREditorDockLayout.cpp
    Widgets/SIREditorVirtualList.cpp
    Widgets/SIREditorHierarchyModel.cpp
    Host/SIREditorSceneWorkspace.cpp
    Host/SIREditorAuthoringSession.cpp
    Host/SIREditorApplication.cpp
    Host/SIREditorWorkflow.cpp
    Host/SIREditorObjectInspector.cpp)
add_library(SIR::EditorAuthoring ALIAS SIREditorAuthoring)
target_compile_features(SIREditorAuthoring PUBLIC cxx_std_17)
target_include_directories(SIREditorAuthoring PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_link_libraries(SIREditorAuthoring PUBLIC SIR::Core SIR::Resources)
sir_configure_owned_target(SIREditorAuthoring)

# Native SIREditor process; the graphical UI subsystem will be linked here.
add_executable(SIREditor Main.cpp)
target_link_libraries(SIREditor PRIVATE SIR::EditorAuthoring SIR::UI)
if(SIR_WITH_IMGUI_OPENGL)
    target_sources(SIREditor PRIVATE SIREditorOpenGL.cpp)
    target_link_libraries(SIREditor PRIVATE SIR::ImGuiOpenGL)
    target_compile_definitions(SIREditor PRIVATE SIR_WITH_IMGUI_OPENGL=1)
endif()
target_compile_definitions(SIREditor PRIVATE SIR_VERSION="${PROJECT_VERSION}")
sir_configure_owned_target(SIREditor)

