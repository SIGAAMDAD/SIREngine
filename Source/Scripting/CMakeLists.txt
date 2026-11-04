add_library(SIRScripting STATIC
    Core/SIRScriptBindingRegistry.cpp
    Core/SIRScriptCodeManager.cpp
    Core/SIRScriptIngress.cpp
    Core/SIRScriptMemoryRouter.cpp
    Core/SIRScriptRuntime.cpp
    Core/SIRScriptScheduler.cpp
    Core/SIRScriptServer.cpp
)
add_library(SIR::Scripting ALIAS SIRScripting)
target_link_libraries(SIRScripting PUBLIC SIR::Core)
target_include_directories(SIRScripting PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRScripting PUBLIC cxx_std_17)
sir_configure_owned_target(SIRScripting)
