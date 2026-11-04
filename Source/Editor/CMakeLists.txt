# Native authoring services. Godot's Control/SceneTree shell is ported separately.
add_library(SIREditorAuthoring STATIC
    Scene/SIREditorSceneDocument.cpp
    Scene/SIREditorTextScene.cpp
    Scene/SIREditorTextProperties.cpp
    Widgets/SIREditorVirtualList.cpp
    Widgets/SIREditorHierarchyModel.cpp
    Host/SIREditorSceneWorkspace.cpp
    Host/SIREditorSceneJSON.cpp)
add_library(SIR::EditorAuthoring ALIAS SIREditorAuthoring)
target_compile_features(SIREditorAuthoring PUBLIC cxx_std_17)
target_include_directories(SIREditorAuthoring PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_link_libraries(SIREditorAuthoring PUBLIC SIR::Core)
sir_configure_owned_target(SIREditorAuthoring)

# First native editor-host executable: source intake and diagnostics only.
# The Godot Control/Tree graphical host is a distinct, later target.
add_executable(SIREditor Main.cpp)
target_link_libraries(SIREditor PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditor PRIVATE SIR_VERSION="${PROJECT_VERSION}")
sir_configure_owned_target(SIREditor)

# Single-process, versioned POD/C-linkage interface to the native authoring
# workspace. The supplied editor-domain segment stays owned by SIRMemoryManager.
add_library(SIREditorNativeBridge STATIC Bridge/SIREditorNativeBridge.cpp)
add_library(SIR::EditorNativeBridge ALIAS SIREditorNativeBridge)
target_compile_features(SIREditorNativeBridge PUBLIC cxx_std_17)
target_include_directories(SIREditorNativeBridge PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_link_libraries(SIREditorNativeBridge PUBLIC SIR::EditorAuthoring)
sir_configure_owned_target(SIREditorNativeBridge)
