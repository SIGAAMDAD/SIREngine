# Self-contained native CPU foundation harness, independent of the engine.
cmake_minimum_required(VERSION 3.20)
project(SIRMeshCacheStandalone LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
enable_testing()
set(SIR_MESH_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../Source")
set(SIR_MESH_BUILD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake")
add_library(SIRMeshCache STATIC "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRMeshCache.cpp")
target_include_directories(SIRMeshCache PUBLIC "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_options(SIRMeshCache PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(SIRMeshCacheTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheTests.cpp")
target_link_libraries(SIRMeshCacheTests PRIVATE SIRMeshCache)
target_compile_options(SIRMeshCacheTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.cache.foundation COMMAND SIRMeshCacheTests)
add_executable(SIRMeshCacheStateTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheStateTests.cpp")
target_link_libraries(SIRMeshCacheStateTests PRIVATE SIRMeshCache)
target_compile_options(SIRMeshCacheStateTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.cache.state-model COMMAND SIRMeshCacheStateTests)
add_executable(SIRMeshCacheEdgeTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheEdgeTests.cpp")
target_link_libraries(SIRMeshCacheEdgeTests PRIVATE SIRMeshCache)
target_compile_options(SIRMeshCacheEdgeTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.cache.edge-contract COMMAND SIRMeshCacheEdgeTests)
# Build an isolated fixture of the same implementation to exercise the unreachable
# UINT32 generation exhaustion boundary without billions of real insertions.
add_executable(SIRMeshCacheGenerationTests
    "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRMeshCache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheGenerationTests.cpp")
target_include_directories(SIRMeshCacheGenerationTests PRIVATE "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_definitions(SIRMeshCacheGenerationTests PRIVATE SIR_MESH_TEST_GENERATION_SEED=4294967293U)
target_compile_options(SIRMeshCacheGenerationTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.cache.generation-exhaustion COMMAND SIRMeshCacheGenerationTests)

# Test the actual uint32 saturation guards with a bounded standalone-only
# threshold; never compile this test override into the production library.
add_executable(SIRMeshCacheBorrowLimitTests
    "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRMeshCache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheBorrowLimitTests.cpp")
target_include_directories(SIRMeshCacheBorrowLimitTests PRIVATE "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_definitions(SIRMeshCacheBorrowLimitTests PRIVATE SIR_MESH_TEST_MAX_BORROWS=2U)
target_compile_options(SIRMeshCacheBorrowLimitTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.cache.borrow-limit COMMAND SIRMeshCacheBorrowLimitTests)

# Allocation-accounting fixture: intercept direct and C++ heap calls.
add_executable(SIRMeshCacheAllocationTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheAllocationTests.cpp")
target_link_libraries(SIRMeshCacheAllocationTests PRIVATE SIRMeshCache)
target_compile_options(SIRMeshCacheAllocationTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(SIRMeshCacheAllocationTests PRIVATE "LINKER:--wrap=malloc" "LINKER:--wrap=calloc" "LINKER:--wrap=realloc")
endif()
add_test(NAME mesh.cache.allocation-free COMMAND SIRMeshCacheAllocationTests)
