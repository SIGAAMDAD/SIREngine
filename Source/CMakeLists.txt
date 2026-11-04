add_subdirectory(Core)
add_subdirectory(Resources)
add_subdirectory(Renderer)
add_subdirectory(Mesh)

add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core SIR::Resources SIR::Graphics)
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
