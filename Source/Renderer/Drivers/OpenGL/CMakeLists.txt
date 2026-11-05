# Native OpenGL 4.6 core-profile module; never adds GL types to SIR::Graphics.
add_library(SIRGL46Driver MODULE
    SIRGLDriver.cpp
    SIRGLContext.cpp
    SIRGLCapabilities.cpp
    SIRGLFunctionLoader.cpp
    SIRGLHandlePool.cpp)
target_include_directories(SIRGL46Driver PRIVATE
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRGL46Driver PRIVATE cxx_std_17)
if(WIN32)
    target_link_libraries(SIRGL46Driver PRIVATE opengl32 gdi32 user32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(SIRGL46Driver PRIVATE "${CMAKE_DL_LIBS}")
else()
    message(FATAL_ERROR "SIR_WITH_OPENGL currently supports Windows and Linux only")
endif()
sir_configure_owned_target(SIRGL46Driver)

