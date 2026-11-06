add_executable(SIRScriptFoundationTests SIRScriptFoundationTests.cpp)
target_link_libraries(SIRScriptFoundationTests PRIVATE SIR::Scripting)
add_test(NAME scripting.foundation COMMAND SIRScriptFoundationTests)
sir_configure_owned_target(SIRScriptFoundationTests)

add_executable(SIRScriptCodeManagerTests SIRScriptCodeManagerTests.cpp)
target_link_libraries(SIRScriptCodeManagerTests PRIVATE SIR::Scripting)
add_test(NAME scripting.code-manager COMMAND SIRScriptCodeManagerTests)
sir_configure_owned_target(SIRScriptCodeManagerTests)

add_executable(SIRScriptArtifactCacheTests SIRScriptArtifactCacheTests.cpp)
target_link_libraries(SIRScriptArtifactCacheTests PRIVATE SIR::Scripting)
add_test(NAME scripting.artifact-cache COMMAND SIRScriptArtifactCacheTests)
sir_configure_owned_target(SIRScriptArtifactCacheTests)

add_executable(SIRScriptOptimizationProtocolTests SIRScriptOptimizationProtocolTests.cpp)
target_link_libraries(SIRScriptOptimizationProtocolTests PRIVATE SIR::Scripting)
add_test(NAME scripting.optimization-protocol COMMAND SIRScriptOptimizationProtocolTests)
sir_configure_owned_target(SIRScriptOptimizationProtocolTests)

add_executable(SIRScriptCachedProgramSourceTests SIRScriptCachedProgramSourceTests.cpp)
target_link_libraries(SIRScriptCachedProgramSourceTests PRIVATE SIR::Scripting)
add_test(NAME scripting.cached-program-source COMMAND SIRScriptCachedProgramSourceTests)
sir_configure_owned_target(SIRScriptCachedProgramSourceTests)

add_executable(SIRScriptOptimizationCoordinatorTests SIRScriptOptimizationCoordinatorTests.cpp)
target_link_libraries(SIRScriptOptimizationCoordinatorTests PRIVATE SIR::Scripting)
add_test(NAME scripting.optimization-coordinator COMMAND SIRScriptOptimizationCoordinatorTests)
sir_configure_owned_target(SIRScriptOptimizationCoordinatorTests)

add_executable(SIRScriptOptimizerWorkerFixture SIRScriptOptimizerWorkerFixture.cpp)
target_link_libraries(SIRScriptOptimizerWorkerFixture PRIVATE SIR::Scripting)
sir_configure_owned_target(SIRScriptOptimizerWorkerFixture)

add_executable(SIRScriptOptimizerOutstandingTests SIRScriptOptimizerOutstandingTests.cpp)
target_link_libraries(SIRScriptOptimizerOutstandingTests PRIVATE SIR::Scripting)
add_dependencies(SIRScriptOptimizerOutstandingTests SIRScriptOptimizerWorkerFixture)
add_test(NAME scripting.optimizer-outstanding-window COMMAND SIRScriptOptimizerOutstandingTests $<TARGET_FILE:SIRScriptOptimizerWorkerFixture>)
sir_configure_owned_target(SIRScriptOptimizerOutstandingTests)

add_executable(SIRScriptOptimizerProcessTransportTests SIRScriptOptimizerProcessTransportTests.cpp)
target_link_libraries(SIRScriptOptimizerProcessTransportTests PRIVATE SIR::Scripting)
add_dependencies(SIRScriptOptimizerProcessTransportTests SIRScriptOptimizerWorkerFixture)
sir_configure_owned_target(SIRScriptOptimizerProcessTransportTests)
add_test(NAME scripting.optimizer-process-transport COMMAND SIRScriptOptimizerProcessTransportTests $<TARGET_FILE:SIRScriptOptimizerWorkerFixture>)

add_executable(SIRScriptOptimizerRestartTests SIRScriptOptimizerRestartTests.cpp)
target_link_libraries(SIRScriptOptimizerRestartTests PRIVATE SIR::Scripting)
add_dependencies(SIRScriptOptimizerRestartTests SIRScriptOptimizerWorkerFixture)
add_test(NAME scripting.optimizer-process-restart COMMAND SIRScriptOptimizerRestartTests $<TARGET_FILE:SIRScriptOptimizerWorkerFixture>)
sir_configure_owned_target(SIRScriptOptimizerRestartTests)

add_library(SIRScriptOptimizerTestBackend SHARED SIRScriptOptimizerTestBackend.cpp)
target_include_directories(SIRScriptOptimizerTestBackend PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRScriptOptimizerTestBackend PRIVATE cxx_std_17)
sir_configure_owned_target(SIRScriptOptimizerTestBackend)

add_executable(SIRScriptOptimizerWorkerTests SIRScriptOptimizerWorkerTests.cpp)
target_link_libraries(SIRScriptOptimizerWorkerTests PRIVATE SIR::Scripting)
sir_configure_owned_target(SIRScriptOptimizerWorkerTests)
add_test(NAME scripting.optimizer-worker COMMAND SIRScriptOptimizerWorkerTests
    $<TARGET_FILE:SIRScriptOptimizerWorker> $<TARGET_FILE:SIRScriptOptimizerTestBackend>)

add_executable(SIRScriptOptimizerWorkerCoverageTests SIRScriptOptimizerWorkerCoverageTests.cpp)
target_link_libraries(SIRScriptOptimizerWorkerCoverageTests PRIVATE SIR::Scripting)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(SIRScriptOptimizerWorkerCoverageTests PRIVATE SIR_SCRIPT_TEST_MALLOC_FAULT_INJECTION=1)
    target_link_options(SIRScriptOptimizerWorkerCoverageTests PRIVATE "LINKER:--wrap=malloc")
endif()
add_test(NAME scripting.optimizer-worker-coverage COMMAND SIRScriptOptimizerWorkerCoverageTests)
sir_configure_owned_target(SIRScriptOptimizerWorkerCoverageTests)

add_executable(SIRScriptOptimizationRecipeTests SIRScriptOptimizationRecipeTests.cpp)
target_link_libraries(SIRScriptOptimizationRecipeTests PRIVATE SIR::Scripting)
sir_configure_owned_target(SIRScriptOptimizationRecipeTests)
add_test(NAME scripting.optimization-recipe COMMAND SIRScriptOptimizationRecipeTests)

add_executable(SIRLangLexerTests SIRLangLexerTests.cpp)
target_link_libraries(SIRLangLexerTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.lexer COMMAND SIRLangLexerTests)
sir_configure_owned_target(SIRLangLexerTests)

add_executable(SIRLangParserTests SIRLangParserTests.cpp)
target_link_libraries(SIRLangParserTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.parser COMMAND SIRLangParserTests)
sir_configure_owned_target(SIRLangParserTests)

add_executable(SIRLangParserAdversarialTests SIRLangParserAdversarialTests.cpp)
target_link_libraries(SIRLangParserAdversarialTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.parser-adversarial COMMAND SIRLangParserAdversarialTests)
sir_configure_owned_target(SIRLangParserAdversarialTests)

add_executable(SIRLangSemanticTests SIRLangSemanticTests.cpp)
target_link_libraries(SIRLangSemanticTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.semantic COMMAND SIRLangSemanticTests)
sir_configure_owned_target(SIRLangSemanticTests)

add_executable(SIRLangSignaturesTests SIRLangSignaturesTests.cpp)
target_link_libraries(SIRLangSignaturesTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.signatures COMMAND SIRLangSignaturesTests)
sir_configure_owned_target(SIRLangSignaturesTests)

add_executable(SIRLangTypeSystemTests SIRLangTypeSystemTests.cpp)
target_link_libraries(SIRLangTypeSystemTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.typesystem COMMAND SIRLangTypeSystemTests)
sir_configure_owned_target(SIRLangTypeSystemTests)

add_executable(SIRLangGraphVMTests SIRLangGraphVMTests.cpp)
target_link_libraries(SIRLangGraphVMTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.graphvm COMMAND SIRLangGraphVMTests)
sir_configure_owned_target(SIRLangGraphVMTests)

add_executable(SIRLangGraphVMAdversarialTests SIRLangGraphVMAdversarialTests.cpp)
target_link_libraries(SIRLangGraphVMAdversarialTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.graphvm-adversarial COMMAND SIRLangGraphVMAdversarialTests)
sir_configure_owned_target(SIRLangGraphVMAdversarialTests)

add_executable(SIRLangGraphVMNumericTests SIRLangGraphVMNumericTests.cpp)
target_link_libraries(SIRLangGraphVMNumericTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.graphvm-numeric COMMAND SIRLangGraphVMNumericTests)
sir_configure_owned_target(SIRLangGraphVMNumericTests)

add_executable(SIRLangLoweringTests SIRLangLoweringTests.cpp)
target_link_libraries(SIRLangLoweringTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.lowering COMMAND SIRLangLoweringTests)
sir_configure_owned_target(SIRLangLoweringTests)

add_executable(SIRLangBytecodeTests SIRLangBytecodeTests.cpp)
target_link_libraries(SIRLangBytecodeTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.bytecode COMMAND SIRLangBytecodeTests)
sir_configure_owned_target(SIRLangBytecodeTests)

add_executable(SIRLangEffectsTests SIRLangEffectsTests.cpp)
target_link_libraries(SIRLangEffectsTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.effects COMMAND SIRLangEffectsTests)
sir_configure_owned_target(SIRLangEffectsTests)

add_executable(SIRLangSourceCompilerTests SIRLangSourceCompilerTests.cpp)
target_link_libraries(SIRLangSourceCompilerTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.source-compiler COMMAND SIRLangSourceCompilerTests)
sir_configure_owned_target(SIRLangSourceCompilerTests)

add_test(NAME scripting.language.cooker-smoke
    COMMAND ${CMAKE_COMMAND}
        "-DCOOKER=$<TARGET_FILE:SIRLangCooker>"
        "-DINPUT=${CMAKE_CURRENT_LIST_DIR}/Fixtures/SIRLang/echo.sir"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sirlang-smoke.sirbc"
        -P "${CMAKE_CURRENT_LIST_DIR}/SIRLangCookerSmoke.cmake")

add_executable(SIRLangGraphVMBackendTests SIRLangGraphVMBackendTests.cpp)
target_link_libraries(SIRLangGraphVMBackendTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.graphvm-backend COMMAND SIRLangGraphVMBackendTests)
sir_configure_owned_target(SIRLangGraphVMBackendTests)

add_executable(SIRLangRuntimeIntegrationTests SIRLangRuntimeIntegrationTests.cpp)
target_link_libraries(SIRLangRuntimeIntegrationTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.runtime-integration COMMAND SIRLangRuntimeIntegrationTests)
sir_configure_owned_target(SIRLangRuntimeIntegrationTests)

add_test(NAME scripting.language.cook-and-run
    COMMAND ${CMAKE_COMMAND}
        "-DCOOKER=$<TARGET_FILE:SIRLangCooker>"
        "-DRUNNER=$<TARGET_FILE:SIRLangRunner>"
        "-DINPUT=${CMAKE_CURRENT_LIST_DIR}/Fixtures/SIRLang/echo.sir"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sirlang-execute.sirbc"
        -P "${CMAKE_CURRENT_LIST_DIR}/SIRLangCookAndRun.cmake")

add_executable(SIRLangDiagnosticsTests SIRLangDiagnosticsTests.cpp)
target_link_libraries(SIRLangDiagnosticsTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.diagnostics COMMAND SIRLangDiagnosticsTests)
sir_configure_owned_target(SIRLangDiagnosticsTests)

add_executable(SIRLangCookRecipeTests SIRLangCookRecipeTests.cpp)
target_link_libraries(SIRLangCookRecipeTests PRIVATE SIR::Scripting)
add_test(NAME scripting.language.cook-recipe COMMAND SIRLangCookRecipeTests)
sir_configure_owned_target(SIRLangCookRecipeTests)

add_test(NAME scripting.language.signed-runner
    COMMAND ${CMAKE_COMMAND}
        "-DCOOKER=$<TARGET_FILE:SIRLangCooker>"
        "-DRUNNER=$<TARGET_FILE:SIRLangRunner>"
        "-DINPUT=${CMAKE_CURRENT_LIST_DIR}/Fixtures/SIRLang/signed.sir"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/sirlang-signed.sirbc"
        -P "${CMAKE_CURRENT_LIST_DIR}/SIRLangSignedRunner.cmake")

# These tests use assert() for both setup and validation. Release/profile
# builds define NDEBUG globally; explicitly enable assertions for native
# language test targets so initialization and negative checks still execute.
get_property(sirScriptingUnitTargets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
foreach(sirScriptingUnitTarget IN LISTS sirScriptingUnitTargets)
    if(sirScriptingUnitTarget MATCHES "^SIRLang.*Tests$")
        if(MSVC)
            target_compile_options(${sirScriptingUnitTarget} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${sirScriptingUnitTarget} PRIVATE -UNDEBUG)
        endif()
    endif()
endforeach()
