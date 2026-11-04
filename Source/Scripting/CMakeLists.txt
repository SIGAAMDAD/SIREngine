add_library(SIRScripting STATIC
    Core/SIRScriptArtifactCache.cpp
    Core/SIRScriptCachedProgramSource.cpp
    Core/SIRScriptBindingRegistry.cpp
    Core/SIRScriptCodeManager.cpp
    Core/SIRScriptIngress.cpp
    Core/SIRScriptMemoryRouter.cpp
    Core/SIRScriptOptimizationRecipe.cpp
    Core/SIRScriptOptimizationProtocol.cpp
    Core/SIRScriptOptimizationCoordinator.cpp
    Core/SIRScriptOptimizerProcessTransport.cpp
    Core/SIRScriptOptimizerInputStore.cpp
    Core/SIRScriptOptimizerWorker.cpp
    Core/SIRScriptRuntime.cpp
    Core/SIRScriptScheduler.cpp
    Core/SIRScriptServer.cpp
    Core/SIRScriptSHA256.cpp
    Language/SIRLangLexer.cpp
    Language/SIRLangParser.cpp
    Language/SIRLangSemantic.cpp
    Language/SIRLangSignatures.cpp
)
find_package(Threads REQUIRED)
add_library(SIR::Scripting ALIAS SIRScripting)
target_link_libraries(SIRScripting PUBLIC SIR::Core Threads::Threads)
target_include_directories(SIRScripting PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRScripting PUBLIC cxx_std_17)
sir_configure_owned_target(SIRScripting)

# Native worker process. Compiler backends are loaded dynamically; no LLVM
# or game scripting language dependency is linked into the engine runtime.
add_executable(SIRScriptOptimizerWorker Worker/SIRScriptOptimizerWorkerMain.cpp)
target_link_libraries(SIRScriptOptimizerWorker PRIVATE SIR::Scripting ${CMAKE_DL_LIBS})
sir_configure_owned_target(SIRScriptOptimizerWorker)
