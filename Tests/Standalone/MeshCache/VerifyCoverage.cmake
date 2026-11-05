# The standalone harness runs all six CTest fixtures before this verifier.
# Require every source-based coverage metric on the production MeshCache TU.
foreach(sirSetting IN ITEMS SIR_MESH_BUILD_DIR SIR_MESH_COVERAGE_DIR SIR_MESH_LLVM_PROFDATA SIR_MESH_LLVM_COV)
    if(NOT DEFINED ${sirSetting} OR "${${sirSetting}}" STREQUAL "")
        message(FATAL_ERROR "Missing MeshCache coverage setting: ${sirSetting}")
    endif()
endforeach()

file(GLOB sirProfiles "${SIR_MESH_COVERAGE_DIR}/mesh-*.profraw")
list(LENGTH sirProfiles sirProfileCount)
if(sirProfileCount LESS 7)
    message(FATAL_ERROR "Expected coverage profiles from seven independent MeshCache tests, found ${sirProfileCount}")
endif()

set(sirProfdata "${SIR_MESH_COVERAGE_DIR}/combined.profdata")
execute_process(
    COMMAND "${SIR_MESH_LLVM_PROFDATA}" merge -sparse ${sirProfiles} -o "${sirProfdata}"
    RESULT_VARIABLE sirMergeResult
    ERROR_VARIABLE sirMergeErrors)
if(NOT sirMergeResult EQUAL 0)
    message(FATAL_ERROR "MeshCache coverage merge failed (${sirMergeResult}): ${sirMergeErrors}")
endif()

set(sirExecutableSuffix "")
if(CMAKE_HOST_WIN32)
    set(sirExecutableSuffix ".exe")
endif()
set(sirExportArgs -summary-only "-instr-profile=${sirProfdata}")
foreach(sirExecutable IN ITEMS SIRMeshCacheStateTests SIRMeshCacheEdgeTests
        SIRMeshCacheGenerationTests SIRMeshCacheBorrowLimitTests SIRMeshCacheAllocationTests SIRMeshCacheOwnerLimitTests)
    list(APPEND sirExportArgs "-object=${SIR_MESH_BUILD_DIR}/${sirExecutable}${sirExecutableSuffix}")
endforeach()
execute_process(
    COMMAND "${SIR_MESH_LLVM_COV}" export ${sirExportArgs} "${SIR_MESH_BUILD_DIR}/SIRMeshCacheTests${sirExecutableSuffix}"
    RESULT_VARIABLE sirExportResult
    OUTPUT_VARIABLE sirJson
    ERROR_VARIABLE sirExportErrors)
if(NOT sirExportResult EQUAL 0)
    message(FATAL_ERROR "MeshCache coverage export failed (${sirExportResult}): ${sirExportErrors}")
endif()

string(JSON sirFileCount ERROR_VARIABLE sirJsonError LENGTH "${sirJson}" data 0 files)
if(sirJsonError)
    message(FATAL_ERROR "Cannot decode LLVM coverage JSON: ${sirJsonError}")
endif()
set(sirMatchedFileCount 0)
math(EXPR sirLastFile "${sirFileCount} - 1")
foreach(sirFileIndex RANGE 0 ${sirLastFile})
    string(JSON sirFilename GET "${sirJson}" data 0 files ${sirFileIndex} filename)
    if(NOT sirFilename MATCHES "[/\\]Source[/\\]Mesh[/\\]SIRMeshCache[.]cpp$")
        continue()
    endif()
    math(EXPR sirMatchedFileCount "${sirMatchedFileCount} + 1")
    foreach(sirMetric IN ITEMS lines branches functions)
        string(JSON sirCount GET "${sirJson}" data 0 files ${sirFileIndex} summary ${sirMetric} count)
        string(JSON sirCovered GET "${sirJson}" data 0 files ${sirFileIndex} summary ${sirMetric} covered)
        if(sirCount LESS 1 OR NOT sirCovered EQUAL sirCount)
            message(FATAL_ERROR "MeshCache ${sirMetric} coverage failed: ${sirCovered}/${sirCount}")
        endif()
        message(STATUS "MeshCache ${sirMetric} source coverage: ${sirCovered}/${sirCount}")
    endforeach()
endforeach()
if(NOT sirMatchedFileCount EQUAL 1)
    message(FATAL_ERROR "Expected one canonical MeshCache source file in coverage JSON; found ${sirMatchedFileCount}")
endif()
message(STATUS "MeshCache coverage admission: PASS")
