add_subdirectory(Core)

add_executable(SIREngine Main.cpp)
target_link_libraries(SIREngine PRIVATE SIR::Core)
# The launcher privately instantiates the native GameInput backend.
if(WIN32 AND SIR_GAMEINPUT_INCLUDE_DIR)
    target_include_directories(SIREngine PRIVATE "${SIR_GAMEINPUT_INCLUDE_DIR}")
endif()
target_compile_definitions(SIREngine PRIVATE SIR_VERSION="${PROJECT_VERSION}")

sir_configure_owned_target(SIREngine)
