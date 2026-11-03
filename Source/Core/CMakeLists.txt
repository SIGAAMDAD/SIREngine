add_library(SIRCore STATIC SIRApplication.cpp)
add_library(SIR::Core ALIAS SIRCore)
target_include_directories(SIRCore PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRCore PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(SIRCore PRIVATE /W4)
else()
    target_compile_options(SIRCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

sir_configure_owned_target(SIRCore)
