# CMake 4 disallows very old policy minima by default; EABase 2.09.13
# still declares cmake_minimum_required(VERSION 3.1).
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING
    "Support pinned EA dependency CMake policy declarations" FORCE)

# Use fixed, recorded Git submodules. EASTL's own FetchContent call is
# redirected to the locally pinned EABase checkout.
foreach(sirDependency IN ITEMS EASTL EABase EAAssert)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/${sirDependency}/CMakeLists.txt")
        message(FATAL_ERROR "Missing third_party/${sirDependency}; run git submodule update --init --recursive")
    endif()
endforeach()

set(FETCHCONTENT_SOURCE_DIR_EABASE "${PROJECT_SOURCE_DIR}/third_party/EABase"
    CACHE PATH "Pinned EABase source" FORCE)
set(EASTL_BUILD_TESTS OFF CACHE BOOL "External EASTL tests" FORCE)
set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "External EASTL benchmark" FORCE)

add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/EASTL"
    "${PROJECT_BINARY_DIR}/third_party/EASTL")
