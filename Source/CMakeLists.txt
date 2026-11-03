add_subdirectory(Core)
if(SIR_WITH_D3D12)
    add_subdirectory(Renderer)
endif()

add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core)
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
