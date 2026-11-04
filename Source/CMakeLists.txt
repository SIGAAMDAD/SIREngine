add_subdirectory(Core)

# The GRDG/Core Jobs integration is owned by its independent feature branch.
# The streaming GRDG adapter is enabled only once its canonical target exists.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/CMakeLists.txt")
    add_subdirectory(Resources)
endif()

add_subdirectory(Renderer)
add_subdirectory(Streaming)
add_subdirectory(World)

add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core SIR::Graphics SIR::Streaming SIR::World)
if(TARGET SIR::StreamingGRDG)
    target_link_libraries(SIREngine PRIVATE SIR::StreamingGRDG)
endif()
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
