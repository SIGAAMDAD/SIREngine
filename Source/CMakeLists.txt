add_subdirectory(Core)
add_subdirectory(Renderer)
add_subdirectory(Resources)
add_subdirectory(UI)
add_subdirectory(Editor)

add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core SIR::Graphics)
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
