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

# Direct editor-private integration boundary for a future Godot GDExtension/
# embedded Control host. C linkage, source-owned data, no process or JSON.
add_library(SIREditorNativeBridge STATIC Bridge/SIREditorNativeBridge.cpp)
add_library(SIR::EditorNativeBridge ALIAS SIREditorNativeBridge)
target_compile_features(SIREditorNativeBridge PUBLIC cxx_std_17)
target_include_directories(SIREditorNativeBridge PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_link_libraries(SIREditorNativeBridge PUBLIC SIR::EditorAuthoring)
sir_configure_owned_target(SIREditorNativeBridge)

# Optional Godot 4.x editor integration. The native CLI/CTest remains usable
# without external SDKs. Provide a godot-cpp checkout built from the matching
# fork extension API; never invoke SCons from the SIREngine build.
option(SIR_EDITOR_ENABLE_GDEXTENSION "Build the Godot editor-native bridge extension" OFF)
set(SIR_GODOT_CPP_ROOT "" CACHE PATH "Path to compatible godot-cpp checkout")
if(SIR_EDITOR_ENABLE_GDEXTENSION)
    if(NOT TARGET godot::cpp)
        if(NOT EXISTS "${SIR_GODOT_CPP_ROOT}/CMakeLists.txt")
            message(FATAL_ERROR "SIR_EDITOR_ENABLE_GDEXTENSION requires SIR_GODOT_CPP_ROOT or godot::cpp")
        endif()
        add_subdirectory("${SIR_GODOT_CPP_ROOT}" "${PROJECT_BINARY_DIR}/ExternalGodotCPP")
    endif()

    # The normal static engine libraries must be relocatable when linked into
    # a platform-shared GDExtension, including their transitive Core objects.
    get_target_property(_sir_editor_core_target SIR::Core ALIASED_TARGET)
    if(NOT _sir_editor_core_target)
        message(FATAL_ERROR "The editor extension requires a concrete SIR::Core target")
    endif()
    set_target_properties(${_sir_editor_core_target} SIREditorAuthoring SIREditorNativeBridge
        PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(SIREditorGodotExtension SHARED
        GodotExtension/SIREditorGodotSession.cpp
        GodotExtension/SIREditorGodotRegistration.cpp)
    target_compile_features(SIREditorGodotExtension PRIVATE cxx_std_17)
    target_include_directories(SIREditorGodotExtension PRIVATE
        "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_link_libraries(SIREditorGodotExtension PRIVATE
        SIR::EditorNativeBridge godot::cpp)
    set_target_properties(SIREditorGodotExtension PROPERTIES
        PREFIX "" OUTPUT_NAME "sir_editor_bridge")
    sir_configure_owned_target(SIREditorGodotExtension)
    install(TARGETS SIREditorGodotExtension
        LIBRARY DESTINATION "addons/sir_editor_bridge/bin" COMPONENT EditorGodotBridge
        RUNTIME DESTINATION "addons/sir_editor_bridge/bin" COMPONENT EditorGodotBridge)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/GodotExtension/sir_editor_bridge.gdextension"
        DESTINATION "addons/sir_editor_bridge" COMPONENT EditorGodotBridge)
endif()
