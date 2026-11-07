# Native editor authoring and scene intake tests.
add_executable(SIREditorAuthoringTests SIREditorAuthoringTests.cpp)
target_link_libraries(SIREditorAuthoringTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.transactions-and-virtual-list COMMAND SIREditorAuthoringTests)
sir_configure_owned_target(SIREditorAuthoringTests)

# Platform-owned editor native host: close, failure and shutdown sequencing.
add_executable(SIREditorWindowHostTests
    SIREditorWindowHostTests.cpp
    "${PROJECT_SOURCE_DIR}/Source/Editor/Host/SIREditorWindowHost.cpp")
target_link_libraries(SIREditorWindowHostTests PRIVATE SIR::Core)
add_test(NAME editor.host.native-window-lifecycle COMMAND SIREditorWindowHostTests)
set_tests_properties(editor.host.native-window-lifecycle PROPERTIES LABELS "editor;window;core;integration" TIMEOUT 60)
sir_configure_owned_target(SIREditorWindowHostTests)

# Native Godot-scene hierarchy intake for the editor-only authoring bridge.
add_executable(SIREditorTextSceneTests SIREditorTextSceneTests.cpp)
target_link_libraries(SIREditorTextSceneTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextSceneTests PRIVATE
    SIR_EDITOR_TEST_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotGetNode.tscn"
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.godot-text-scene COMMAND SIREditorTextSceneTests)
sir_configure_owned_target(SIREditorTextSceneTests)

# Read-only external and embedded resource-dependency validation.
add_executable(SIREditorTextResourcesTests SIREditorTextResourcesTests.cpp)
target_link_libraries(SIREditorTextResourcesTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextResourcesTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.godot-text-resources COMMAND SIREditorTextResourcesTests)
sir_configure_owned_target(SIREditorTextResourcesTests)

# Editor-only scene tree projection, source ordering and virtualized layout.
add_executable(SIREditorHierarchyModelTests SIREditorHierarchyModelTests.cpp)
target_link_libraries(SIREditorHierarchyModelTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorHierarchyModelTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.hierarchy-view-model COMMAND SIREditorHierarchyModelTests)
sir_configure_owned_target(SIREditorHierarchyModelTests)

# Native editor workspace and executable bootstrap over the Godot text-scene intake.
add_executable(SIREditorSceneWorkspaceTests SIREditorSceneWorkspaceTests.cpp)
target_link_libraries(SIREditorSceneWorkspaceTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorSceneWorkspaceTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.host.scene-workspace COMMAND SIREditorSceneWorkspaceTests)
sir_configure_owned_target(SIREditorSceneWorkspaceTests)

add_test(NAME editor.host.version COMMAND SIREditor --version)
set_tests_properties(editor.host.version PROPERTIES PASS_REGULAR_EXPRESSION "SIREditor native scene host")
add_test(NAME editor.host.inspect-scene COMMAND SIREditor --inspect-scene
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
set_tests_properties(editor.host.inspect-scene PROPERTIES PASS_REGULAR_EXPRESSION "Nodes: 5")
add_test(NAME editor.host.missing-scene COMMAND SIREditor --inspect-scene
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/does-not-exist.tscn")
set_tests_properties(editor.host.missing-scene PROPERTIES WILL_FAIL TRUE)

# Source-backed node-property index feeding the read-only native Details dock.
add_executable(SIREditorTextPropertiesTests SIREditorTextPropertiesTests.cpp)
target_link_libraries(SIREditorTextPropertiesTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextPropertiesTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.source-properties COMMAND SIREditorTextPropertiesTests)
sir_configure_owned_target(SIREditorTextPropertiesTests)


# SIREngine native object/type/property reflection and generation-safe handles.
add_executable(SIRObjectRegistryTests SIRObjectRegistryTests.cpp)
target_link_libraries(SIRObjectRegistryTests PRIVATE SIR::Core)
add_test(NAME core.objects.registry-and-lifetime COMMAND SIRObjectRegistryTests)
sir_configure_owned_target(SIRObjectRegistryTests)

# Typed authored property changes share the scene document's undo/redo journal.
add_executable(SIREditorTypedPropertyTests SIREditorTypedPropertyTests.cpp)
target_link_libraries(SIREditorTypedPropertyTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.typed-property-transactions COMMAND SIREditorTypedPropertyTests)
sir_configure_owned_target(SIREditorTypedPropertyTests)

# Deterministic differential property-state machine: 2500 mixed valid/invalid transactions.
add_executable(SIREditorPropertyStressTests SIREditorPropertyStressTests.cpp)
target_link_libraries(SIREditorPropertyStressTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.property-journal-stress COMMAND SIREditorPropertyStressTests)
sir_configure_owned_target(SIREditorPropertyStressTests)

# Native SIREngine resource identity and dependency DAG without a foreign loader.
add_executable(SIRResourceCatalogTests SIRResourceCatalogTests.cpp)
target_link_libraries(SIRResourceCatalogTests PRIVATE SIR::Resources)
add_test(NAME resources.catalog.dependency-validation COMMAND SIRResourceCatalogTests)
sir_configure_owned_target(SIRResourceCatalogTests)

# Allocation-free typed text-scene scalar conversion and precision roundtrips.
add_executable(SIREditorScalarCodecTests SIREditorScalarCodecTests.cpp)
target_link_libraries(SIREditorScalarCodecTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.typed-scalar-codec COMMAND SIREditorScalarCodecTests)
sir_configure_owned_target(SIREditorScalarCodecTests)

# Bound native typed-property edit import, undo and lossless text-scene export.
add_executable(SIREditorScalarBindingTests SIREditorScalarBindingTests.cpp)
target_link_libraries(SIREditorScalarBindingTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.typed-scalar-binding COMMAND SIREditorScalarBindingTests)
sir_configure_owned_target(SIREditorScalarBindingTests)

# Independent reference graph validates resource DAG, duplicate, and cycle checks.
add_executable(SIRResourceCatalogDifferentialTests SIRResourceCatalogDifferentialTests.cpp)
target_link_libraries(SIRResourceCatalogDifferentialTests PRIVATE SIR::Resources)
add_test(NAME resources.catalog.differential-dag COMMAND SIRResourceCatalogDifferentialTests)
sir_configure_owned_target(SIRResourceCatalogDifferentialTests)

# Full source fixture: reflect, import, edit, undo/redo, losslessly serialize and reopen.
add_executable(SIREditorRealSceneScalarRoundtripTests SIREditorRealSceneScalarRoundtripTests.cpp)
target_link_libraries(SIREditorRealSceneScalarRoundtripTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorRealSceneScalarRoundtripTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.scene.real-fixture-scalar-roundtrip COMMAND SIREditorRealSceneScalarRoundtripTests)
sir_configure_owned_target(SIREditorRealSceneScalarRoundtripTests)

# Real crash-conscious scene file replacement, staging collisions and failed rename.
add_executable(SIREditorFileSaveTests SIREditorFileSaveTests.cpp)
target_link_libraries(SIREditorFileSaveTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.atomic-file-save COMMAND SIREditorFileSaveTests)
sir_configure_owned_target(SIREditorFileSaveTests)

# Source-format scalar edit preservation and fail-closed writer.
add_executable(SIREditorTextWriterTests SIREditorTextWriterTests.cpp)
target_link_libraries(SIREditorTextWriterTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextWriterTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.scene.text-scalar-writer COMMAND SIREditorTextWriterTests)
sir_configure_owned_target(SIREditorTextWriterTests)

# Retained SIREngine-native control hierarchy, focus, layout, clipping and input.
add_executable(SIRControlTreeTests SIRControlTreeTests.cpp)
target_link_libraries(SIRControlTreeTests PRIVATE SIR::UI)
add_test(NAME ui.controls.layout-and-focus COMMAND SIRControlTreeTests)
sir_configure_owned_target(SIRControlTreeTests)

# Platform-owned bounded binary file reading, regular-file rejection and capacity gates.
add_executable(SIRFileSystemReadTests SIRFileSystemReadTests.cpp)
target_link_libraries(SIRFileSystemReadTests PRIVATE SIR::Core)
add_test(NAME core.platform.file-read COMMAND SIRFileSystemReadTests)
sir_configure_owned_target(SIRFileSystemReadTests)

# Source-backed resource references and embedded-resource dependency ownership.
add_executable(SIREditorResourceReferencesTests SIREditorResourceReferencesTests.cpp)
target_link_libraries(SIREditorResourceReferencesTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.resource-reference-index COMMAND SIREditorResourceReferencesTests)
sir_configure_owned_target(SIREditorResourceReferencesTests)
