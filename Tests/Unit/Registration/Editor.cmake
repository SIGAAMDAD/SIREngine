# Native editor authoring and scene intake tests.
add_executable(SIREditorAuthoringTests SIREditorAuthoringTests.cpp)
target_link_libraries(SIREditorAuthoringTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.authoring.transactions-and-virtual-list COMMAND SIREditorAuthoringTests)
sir_configure_owned_target(SIREditorAuthoringTests)

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

# Canonical scene resource identity and dependency import into SIRResourceCatalog.
add_executable(SIREditorCatalogBridgeTests SIREditorCatalogBridgeTests.cpp)
target_link_libraries(SIREditorCatalogBridgeTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.resource-catalog-bridge COMMAND SIREditorCatalogBridgeTests)
sir_configure_owned_target(SIREditorCatalogBridgeTests)

add_executable(SIREditorApplicationTests SIREditorApplicationTests.cpp)
target_link_libraries(SIREditorApplicationTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.host.application-loop COMMAND SIREditorApplicationTests)
sir_configure_owned_target(SIREditorApplicationTests)

add_test(NAME editor.host.headless-loop COMMAND SIREditor --headless-frames 3)
set_tests_properties(editor.host.headless-loop PROPERTIES PASS_REGULAR_EXPRESSION "SIREditor frames: 3")
add_test(NAME editor.host.reject-unbounded-loop COMMAND SIREditor --headless-frames 0)
set_tests_properties(editor.host.reject-unbounded-loop PROPERTIES WILL_FAIL TRUE)

add_executable(SIREditorDockLayoutTests SIREditorDockLayoutTests.cpp)
target_link_libraries(SIREditorDockLayoutTests PRIVATE SIR::EditorAuthoring SIR::UI)
add_test(NAME editor.ui.dock-layout COMMAND SIREditorDockLayoutTests)
sir_configure_owned_target(SIREditorDockLayoutTests)

add_executable(SIREditorWorkflowTests SIREditorWorkflowTests.cpp)
target_link_libraries(SIREditorWorkflowTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.workflow.standard-actions COMMAND SIREditorWorkflowTests)
sir_configure_owned_target(SIREditorWorkflowTests)

add_executable(SIREditorResourceGraphBridgeTests SIREditorResourceGraphBridgeTests.cpp SIREditorResourceGraphCatalogFixture.cpp)
target_link_libraries(SIREditorResourceGraphBridgeTests PRIVATE SIR::EditorAuthoring SIR::Resources)
add_test(NAME editor.resources.grdg-bridge COMMAND SIREditorResourceGraphBridgeTests)
sir_configure_owned_target(SIREditorResourceGraphBridgeTests)

add_executable(SIREditorObjectInspectorTests SIREditorObjectInspectorTests.cpp)
target_link_libraries(SIREditorObjectInspectorTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.objects.inherited-inspector COMMAND SIREditorObjectInspectorTests)
sir_configure_owned_target(SIREditorObjectInspectorTests)

add_executable(SIREditorComplexWriterTests SIREditorComplexWriterTests.cpp)
target_link_libraries(SIREditorComplexWriterTests PRIVATE SIR::EditorAuthoring)
add_test(NAME editor.scene.complex-source-writer COMMAND SIREditorComplexWriterTests)
sir_configure_owned_target(SIREditorComplexWriterTests)
