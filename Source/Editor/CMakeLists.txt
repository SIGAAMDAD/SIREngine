# Native authoring services. Godot's Control/SceneTree shell is ported separately.
add_library(SIREditorAuthoring STATIC
    Scene/SIREditorSceneDocument.cpp
    Scene/SIREditorTextScene.cpp
    Widgets/SIREditorVirtualList.cpp
    Widgets/SIREditorHierarchyModel.cpp)
add_library(SIR::EditorAuthoring ALIAS SIREditorAuthoring)
target_compile_features(SIREditorAuthoring PUBLIC cxx_std_17)
target_include_directories(SIREditorAuthoring PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_link_libraries(SIREditorAuthoring PUBLIC SIR::Core)
sir_configure_owned_target(SIREditorAuthoring)
