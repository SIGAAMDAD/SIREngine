# Self-contained native CPU foundation harness, independent of the engine.
cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
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

# Isolated owner-ID saturation fixture; production counter starts at one.
add_executable(SIRMeshCacheOwnerLimitTests
    "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRMeshCache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheOwnerLimitTests.cpp")
target_include_directories(SIRMeshCacheOwnerLimitTests PRIVATE "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_definitions(SIRMeshCacheOwnerLimitTests PRIVATE SIR_MESH_TEST_OWNER_ID_SEED=18446744073709551614ULL)
target_compile_options(SIRMeshCacheOwnerLimitTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.cache.owner-id-limit COMMAND SIRMeshCacheOwnerLimitTests)

# Concurrent owner identity must remain unique across independent SIRThread users.
add_executable(SIRMeshCacheOwnerConcurrencyTests
    "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshCacheOwnerConcurrencyTests.cpp"
    "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRMeshCache.cpp"
    "${SIR_MESH_SOURCE_ROOT}/Core/Platform/SIRThread.cpp")
target_include_directories(SIRMeshCacheOwnerConcurrencyTests PRIVATE "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
# Force one stale expected CAS per initialization only in this isolated fixture.
target_compile_definitions(SIRMeshCacheOwnerConcurrencyTests PRIVATE SIR_MESH_TEST_FORCE_CAS_RETRY=1)
target_compile_options(SIRMeshCacheOwnerConcurrencyTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
find_package(Threads REQUIRED)
target_link_libraries(SIRMeshCacheOwnerConcurrencyTests PRIVATE Threads::Threads)
add_test(NAME mesh.cache.owner-concurrency COMMAND SIRMeshCacheOwnerConcurrencyTests)

# Explicit opt-in source-based coverage admission for the standalone CPU cache.
# Only this standalone harness is instrumented; production engine targets are not.
option(SIR_MESH_ENABLE_COVERAGE "Enable LLVM coverage verification for the standalone mesh cache" OFF)
if(SIR_MESH_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "SIR_MESH_ENABLE_COVERAGE requires Clang's source-based coverage")
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "MeshCache full-coverage admission requires CMAKE_BUILD_TYPE=Release (NDEBUG)")
    endif()

    find_program(SIR_MESH_LLVM_PROFDATA NAMES llvm-profdata REQUIRED)
    find_program(SIR_MESH_LLVM_COV NAMES llvm-cov REQUIRED)
    foreach(sirTarget IN ITEMS SIRMeshCache SIRMeshCacheTests SIRMeshCacheStateTests SIRMeshCacheEdgeTests
            SIRMeshCacheGenerationTests SIRMeshCacheBorrowLimitTests SIRMeshCacheAllocationTests SIRMeshCacheOwnerLimitTests SIRMeshCacheOwnerConcurrencyTests)
        target_compile_options(${sirTarget} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        if(NOT sirTarget STREQUAL "SIRMeshCache")
            target_link_options(${sirTarget} PRIVATE -fprofile-instr-generate)
        endif()
    endforeach()

    set(SIR_MESH_COVERAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/MeshCacheCoverage")
    add_custom_target(mesh-cache-coverage
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${SIR_MESH_COVERAGE_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SIR_MESH_COVERAGE_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E env "LLVM_PROFILE_FILE=${SIR_MESH_COVERAGE_DIRECTORY}/mesh-%p.profraw"
                "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_CURRENT_BINARY_DIR}" --output-on-failure
        COMMAND "${CMAKE_COMMAND}"
                "-DSIR_MESH_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                "-DSIR_MESH_COVERAGE_DIR=${SIR_MESH_COVERAGE_DIRECTORY}"
                "-DSIR_MESH_LLVM_PROFDATA=${SIR_MESH_LLVM_PROFDATA}"
                "-DSIR_MESH_LLVM_COV=${SIR_MESH_LLVM_COV}"
                -P "${CMAKE_CURRENT_LIST_DIR}/VerifyCoverage.cmake"
        DEPENDS SIRMeshCacheTests SIRMeshCacheStateTests SIRMeshCacheEdgeTests
                SIRMeshCacheGenerationTests SIRMeshCacheBorrowLimitTests SIRMeshCacheAllocationTests
                SIRMeshCacheOwnerLimitTests SIRMeshCacheOwnerConcurrencyTests SIRVGFramePlanTests
        USES_TERMINAL
        COMMENT "Run all real CPU mesh tests and require 100% production-source line/branch/function coverage")
endif()

# Native graphics ABI is deliberately not linked into the portable CPU plan.
add_library(SIRVGFramePlan STATIC "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRVGFramePlan.cpp")
target_include_directories(SIRVGFramePlan PUBLIC "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_options(SIRVGFramePlan PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(SIRVGFramePlanTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRVGFramePlanTests.cpp")
target_link_libraries(SIRVGFramePlanTests PRIVATE SIRVGFramePlan)
target_compile_options(SIRVGFramePlanTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.vg.frame-plan COMMAND SIRVGFramePlanTests)
if(SIR_MESH_ENABLE_COVERAGE)
    target_compile_options(SIRVGFramePlan PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_compile_options(SIRVGFramePlanTests PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(SIRVGFramePlanTests PRIVATE -fprofile-instr-generate)
endif()

# Direct cross-system contract verification without GRDG/driver implementations.
add_library(SIRVGContracts STATIC "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRVGContracts.cpp")
target_include_directories(SIRVGContracts PUBLIC "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_options(SIRVGContracts PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(SIRVGContractsTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRVGContractsTests.cpp")
target_link_libraries(SIRVGContractsTests PRIVATE SIRVGContracts)
target_compile_options(SIRVGContractsTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.vg.contracts COMMAND SIRVGContractsTests)

add_library(SIRMeshPackSchema STATIC
    "${SIR_MESH_SOURCE_ROOT}/Mesh/SIRMeshPackSchema.cpp"
    "${SIR_MESH_SOURCE_ROOT}/Resources/SIRPackFormat.cpp")
target_include_directories(SIRMeshPackSchema PUBLIC "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}")
target_compile_options(SIRMeshPackSchema PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(SIRMeshPackSchemaTests "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRMeshPackSchemaTests.cpp")
target_link_libraries(SIRMeshPackSchemaTests PRIVATE SIRMeshPackSchema)
target_compile_options(SIRMeshPackSchemaTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.sirpack.schema COMMAND SIRMeshPackSchemaTests)

# Authoring-only adaptive displacement is independently linkable without
# meshoptimizer, a graphics device or engine runtime state.
add_executable(SIRDisplacementCookerTests
    "${CMAKE_CURRENT_LIST_DIR}/../../../Tools/GeometryCooker/SIRDisplacementCooker.cpp"
    "${SIR_MESH_SOURCE_ROOT}/Resources/SIRPackFormat.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../Unit/SIRDisplacementCookerTests.cpp")
target_include_directories(SIRDisplacementCookerTests PRIVATE "${SIR_MESH_SOURCE_ROOT}" "${SIR_MESH_BUILD_ROOT}" "${CMAKE_CURRENT_LIST_DIR}/../../..")
target_compile_options(SIRDisplacementCookerTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME mesh.geometry.displacement-bake COMMAND SIRDisplacementCookerTests)
