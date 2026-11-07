include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native SIREngine editor authoring services; source scene import is a codec.
add_library(SIREditorAuthoring STATIC
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
target_compile_definitions(SIREditor PRIVATE SIR_VERSION="${PROJECT_VERSION}")
sir_configure_owned_target(SIREditor)

