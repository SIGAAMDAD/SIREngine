add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core)
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
