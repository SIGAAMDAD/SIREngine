# Core has no external dependency until the framework migration begins.
# The pinned container family is optional and independent of initial startup.
if(SIR_WITH_EASTL)
    foreach(sirDependency IN ITEMS EASTL EABase EAAssert)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/${sirDependency}/CMakeLists.txt")
            message(FATAL_ERROR "Missing ThirdParty/${sirDependency}; initialize the pinned submodule")
        endif()
    endforeach()

    set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "EA dependency compatibility" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_EABASE "${PROJECT_SOURCE_DIR}/ThirdParty/EABase" CACHE PATH "Pinned EABase" FORCE)
    set(EASTL_BUILD_TESTS OFF CACHE BOOL "EASTL upstream tests" FORCE)
    set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "EASTL upstream benchmark" FORCE)
    add_subdirectory(ThirdParty/EASTL "${PROJECT_BINARY_DIR}/eastl")
endif()
