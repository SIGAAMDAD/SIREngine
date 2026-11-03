# Every third-party dependency is opt-in. Core and Launcher do not
# inherit vendor libraries until the owning engine system needs them.
if(SIR_WITH_EASTL)
    foreach(sirDependency IN ITEMS EASTL EABase EAAssert)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/${sirDependency}/CMakeLists.txt")
            message(FATAL_ERROR
                "Missing ThirdParty/${sirDependency}; run git submodule update --init ThirdParty/${sirDependency}")
        endif()
    endforeach()

    set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "EA dependency compatibility" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_EABASE "${PROJECT_SOURCE_DIR}/ThirdParty/EABase"
        CACHE PATH "Pinned EABase" FORCE)
    set(EASTL_BUILD_TESTS OFF CACHE BOOL "EASTL upstream tests" FORCE)
    set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "EASTL upstream benchmarks" FORCE)
    add_subdirectory(ThirdParty/EASTL "${PROJECT_BINARY_DIR}/eastl")
endif()

if(SIR_WITH_MESHOPTIMIZER)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/meshoptimizer/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing meshoptimizer source; run git submodule update --init ThirdParty/meshoptimizer")
    endif()
    set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "meshoptimizer demo" FORCE)
    set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "meshoptimizer gltfpack" FORCE)
    set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "Static meshoptimizer" FORCE)
    set(MESHOPT_INSTALL OFF CACHE BOOL "Do not install vendored meshoptimizer" FORCE)
    add_subdirectory(ThirdParty/meshoptimizer "${PROJECT_BINARY_DIR}/meshoptimizer")
    add_library(SIRMeshOptimizer INTERFACE)
    add_library(SIR::MeshOptimizer ALIAS SIRMeshOptimizer)
    target_link_libraries(SIRMeshOptimizer INTERFACE meshoptimizer)
endif()
