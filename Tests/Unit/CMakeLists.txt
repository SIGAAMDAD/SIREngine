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

# Native JSON presentation protocol for the editor-private Godot Tree dock.
add_executable(SIREditorSceneJSONTests SIREditorSceneJSONTests.cpp)
target_link_libraries(SIREditorSceneJSONTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorSceneJSONTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.host.json-unit COMMAND SIREditorSceneJSONTests)
sir_configure_owned_target(SIREditorSceneJSONTests)
add_test(NAME editor.host.json-cli COMMAND SIREditor --inspect-scene-json
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
set_tests_properties(editor.host.json-cli PROPERTIES PASS_REGULAR_EXPRESSION "\"schema\":1")

# Nonblocking editor-dock transport: staged native scene snapshot publication.
add_executable(SIREditorSceneJSONFileTests SIREditorSceneJSONFileTests.cpp)
target_link_libraries(SIREditorSceneJSONFileTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorSceneJSONFileTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.host.json-file-unit COMMAND SIREditorSceneJSONFileTests)
sir_configure_owned_target(SIREditorSceneJSONFileTests)
add_test(NAME editor.host.json-file-cli COMMAND "${CMAKE_COMMAND}"
    "-DSIR_EDITOR_EXECUTABLE=$<TARGET_FILE:SIREditor>"
    "-DSIR_EDITOR_SCENE=${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn"
    "-DSIR_EDITOR_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sir-editor-cli-snapshot-$<CONFIG>.json"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/EditorJSONFileCLITests.cmake")

# Source-backed node-property index feeding the read-only native Details dock.
add_executable(SIREditorTextPropertiesTests SIREditorTextPropertiesTests.cpp)
target_link_libraries(SIREditorTextPropertiesTests PRIVATE SIR::EditorAuthoring)
target_compile_definitions(SIREditorTextPropertiesTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.authoring.source-properties COMMAND SIREditorTextPropertiesTests)
sir_configure_owned_target(SIREditorTextPropertiesTests)

# In-process ABI boundary for a future Godot-native Control/Inspector adapter.
add_executable(SIREditorNativeBridgeTests SIREditorNativeBridgeTests.cpp)
target_link_libraries(SIREditorNativeBridgeTests PRIVATE SIR::EditorNativeBridge)
target_compile_definitions(SIREditorNativeBridgeTests PRIVATE
    SIR_EDITOR_TEST_UI_SCENE="${PROJECT_SOURCE_DIR}/Tests/Fixtures/Editor/SourceGodotUI.tscn")
add_test(NAME editor.bridge.native-abi COMMAND SIREditorNativeBridgeTests)
sir_configure_owned_target(SIREditorNativeBridgeTests)
