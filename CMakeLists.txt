cmake_minimum_required(VERSION 4.4.5)

project(SIREngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SIR_BUILD_TESTS "Build SIREngine-owned unit/smoke tests" ON)
option(SIR_BUILD_GODOT "Expose the pinned Godot editor build through SCons" OFF)
option(SIR_WITH_FMOD "Resolve a locally installed FMOD Studio SDK" OFF)
option(SIR_WITH_WWISE "Resolve a locally installed Wwise SDK" OFF)
option(SIR_WITH_OPENAL "Resolve the system OpenAL dependency" OFF)

include(cmake/SIRDependencies.cmake)
include(cmake/SIRAudioDependencies.cmake)

add_library(sir_core INTERFACE)
add_library(SIR::Core ALIAS sir_core)
target_compile_features(sir_core INTERFACE cxx_std_17)
target_include_directories(sir_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(sir_core INTERFACE EASTL)

if(SIR_BUILD_TESTS)
    enable_testing()
    add_executable(sir_containers_smoke tests/containers_smoke.cpp)
    target_link_libraries(sir_containers_smoke PRIVATE SIR::Core)
    add_test(NAME sir.containers COMMAND sir_containers_smoke)
endif()

if(SIR_BUILD_GODOT)
    include(cmake/SIRGodot.cmake)
endif()
