add_subdirectory(Core)
add_subdirectory(Renderer)
add_subdirectory(Streaming)
add_subdirectory(World)

add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core SIR::Graphics SIR::Streaming SIR::World)
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
