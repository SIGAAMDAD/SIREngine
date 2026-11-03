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

# Google testing and profiling are opt-in host tooling. They must not
# become transitive requirements of the shipping Core or Launcher.
if(SIR_WITH_GOOGLETEST)
    if(NOT SIR_BUILD_TESTS)
        message(FATAL_ERROR "SIR_WITH_GOOGLETEST requires SIR_BUILD_TESTS=ON")
    endif()
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/GoogleTest/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing GoogleTest; run git submodule update --init ThirdParty/GoogleTest")
    endif()
    set(BUILD_GMOCK OFF CACHE BOOL "Do not build GoogleMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Do not install vendor test library" FORCE)
    set(gtest_build_tests OFF CACHE BOOL "Do not build vendor GoogleTest tests" FORCE)
    set(gtest_build_samples OFF CACHE BOOL "Do not build vendor GoogleTest samples" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "Match host MSVC runtime" FORCE)
    add_subdirectory(ThirdParty/GoogleTest "${PROJECT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
endif()

if(SIR_WITH_GOOGLE_BENCHMARK)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/GoogleBenchmark/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing Google Benchmark; run git submodule update --init ThirdParty/GoogleBenchmark")
    endif()
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable vendor benchmark tests" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable vendor GoogleTest dependency" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Disable vendor warning-as-error" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable vendor installation" FORCE)
    set(BENCHMARK_ENABLE_DOXYGEN OFF CACHE BOOL "Disable vendor docs" FORCE)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "Use pinned local sources only" FORCE)
    add_subdirectory(ThirdParty/GoogleBenchmark "${PROJECT_BINARY_DIR}/googlebenchmark" EXCLUDE_FROM_ALL)
endif()
