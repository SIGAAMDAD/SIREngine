add_executable(SIRApplicationTests SIRApplicationTests.cpp)
target_link_libraries(SIRApplicationTests PRIVATE SIR::Core)
add_test(NAME core.application.lifecycle COMMAND SIRApplicationTests)
add_test(NAME launcher.version COMMAND SIREngine --version)
set_tests_properties(launcher.version PROPERTIES PASS_REGULAR_EXPRESSION "SIREngine native foundation")
add_test(NAME launcher.help COMMAND SIREngine --help)
set_tests_properties(launcher.help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: SIREngine")
add_test(NAME launcher.unknown-option COMMAND SIREngine --unknown)
set_tests_properties(launcher.unknown-option PROPERTIES WILL_FAIL TRUE)

sir_configure_owned_target(SIRApplicationTests)

add_library(SIRPlatformTestModule SHARED SIRDynamicLibraryFixture.cpp)
sir_configure_owned_target(SIRPlatformTestModule)

add_executable(SIRSystemTests SIRSystemTests.cpp)
add_dependencies(SIRSystemTests SIRPlatformTestModule)
target_compile_definitions(SIRSystemTests PRIVATE SIR_TEST_MODULE_PATH="$<TARGET_FILE:SIRPlatformTestModule>")
target_link_libraries(SIRSystemTests PRIVATE SIR::Core)
add_test(NAME core.platform.lifecycle COMMAND SIRSystemTests)
sir_configure_owned_target(SIRSystemTests)

add_executable(SIRMemoryManagerTests SIRMemoryManagerTests.cpp)
target_link_libraries(SIRMemoryManagerTests PRIVATE SIR::Core)
add_test(NAME core.memory.manager COMMAND SIRMemoryManagerTests)
sir_configure_owned_target(SIRMemoryManagerTests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRVirtualMemoryRollbackTests SIRVirtualMemoryRollbackTests.cpp)
    target_link_libraries(SIRVirtualMemoryRollbackTests PRIVATE SIR::Core)
    target_link_options(SIRVirtualMemoryRollbackTests PRIVATE "LINKER:--wrap=madvise")
    add_test(NAME core.platform.decommit-failure COMMAND SIRVirtualMemoryRollbackTests)
    sir_configure_owned_target(SIRVirtualMemoryRollbackTests)
endif()

if(SIR_WITH_GOOGLETEST)
    add_executable(SIRGoogleTests GoogleApplicationTests.cpp)
    target_compile_features(SIRGoogleTests PRIVATE cxx_std_17)
    target_link_libraries(SIRGoogleTests PRIVATE SIR::Core gtest_main)
    add_test(NAME core.application.googletest COMMAND SIRGoogleTests)
    sir_configure_owned_target(SIRGoogleTests)
endif()

# Validate the standalone CMake profiler comparison without vendor libraries.
add_test(NAME profiling.benchmark-comparison
    COMMAND "${CMAKE_COMMAND}"
        "-DSIR_ROOT=${PROJECT_SOURCE_DIR}"
        "-DSIR_TEST_ROOT=${PROJECT_BINARY_DIR}/RegressionSmoke"
        -P "${PROJECT_SOURCE_DIR}/Tests/Benchmarks/GoogleBenchmarkCompareTests.cmake")

add_executable(SIRThreadingPrimitiveTests SIRThreadingPrimitiveTests.cpp)
target_link_libraries(SIRThreadingPrimitiveTests PRIVATE SIR::Core)
# Clang's libstdc++ implementation of std::atomic::is_lock_free() calls
# __atomic_is_lock_free, supplied by libatomic on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(SIRThreadingPrimitiveTests PRIVATE atomic)
endif()
add_test(NAME core.threading.primitives COMMAND SIRThreadingPrimitiveTests)
sir_configure_owned_target(SIRThreadingPrimitiveTests)

include(${CMAKE_CURRENT_SOURCE_DIR}/SIRAllocatorMigration.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/EventTests.cmake)
add_executable(SIRContainerTests SIRContainerTests.cpp)
target_link_libraries(SIRContainerTests PRIVATE SIR::Core)
add_test(NAME core.containers.basic COMMAND SIRContainerTests)
sir_configure_owned_target(SIRContainerTests)

add_executable(SIRContainerDifferentialTests SIRContainerDifferentialTests.cpp)
target_link_libraries(SIRContainerDifferentialTests PRIVATE SIR::Core)
add_test(NAME core.containers.differential COMMAND SIRContainerDifferentialTests)
sir_configure_owned_target(SIRContainerDifferentialTests)

add_executable(SIRContainerExtendedTests SIRContainerExtendedTests.cpp)
target_link_libraries(SIRContainerExtendedTests PRIVATE SIR::Core)
add_test(NAME core.containers.extended COMMAND SIRContainerExtendedTests)
sir_configure_owned_target(SIRContainerExtendedTests)

add_executable(SIRContainerOptimizationTests SIRContainerOptimizationTests.cpp)
target_link_libraries(SIRContainerOptimizationTests PRIVATE SIR::Core)
add_test(NAME core.containers.optimization COMMAND SIRContainerOptimizationTests)
sir_configure_owned_target(SIRContainerOptimizationTests)

add_executable(SIRContainerMemoryTests SIRContainerMemoryTests.cpp)
target_link_libraries(SIRContainerMemoryTests PRIVATE SIR::Core)
add_test(NAME core.containers.memory COMMAND SIRContainerMemoryTests)
sir_configure_owned_target(SIRContainerMemoryTests)

add_executable(SIRContainerHeapAdapterTests SIRContainerHeapAdapterTests.cpp)
target_link_libraries(SIRContainerHeapAdapterTests PRIVATE SIR::Core)
add_test(NAME core.containers.heap-adapter COMMAND SIRContainerHeapAdapterTests)
sir_configure_owned_target(SIRContainerHeapAdapterTests)

# Shared renderer contract tests run on Windows/Linux without a GPU or native driver.
add_executable(SIRGraphicsInterfaceTests SIRGraphicsInterfaceTests.cpp)
target_link_libraries(SIRGraphicsInterfaceTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.interface COMMAND SIRGraphicsInterfaceTests)
sir_configure_owned_target(SIRGraphicsInterfaceTests)

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
