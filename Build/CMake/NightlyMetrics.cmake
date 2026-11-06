cmake_minimum_required(VERSION 3.25)
foreach(sirRequired SIR_REPO_ROOT SIR_REPORT_ROOT SIR_LABEL SIR_PRESET
    SIR_CONFIG_EXIT SIR_BUILD_EXIT SIR_TEST_EXIT SIR_PACKAGE_EXIT
    SIR_CONFIG_SECONDS SIR_BUILD_SECONDS SIR_TEST_SECONDS SIR_PACKAGE_SECONDS)
    if(NOT DEFINED ${sirRequired})
        message(FATAL_ERROR "Missing nightly metric: ${sirRequired}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${SIR_REPORT_ROOT}/metrics")
set(sirTests 0)
set(sirTestXml "${SIR_REPORT_ROOT}/tests/${SIR_LABEL}.xml")
if(EXISTS "${sirTestXml}")
    file(READ "${sirTestXml}" sirXml)
    string(REGEX MATCHALL "<testcase[ >]" sirCases "${sirXml}")
    list(LENGTH sirCases sirTests)
endif()
set(sirWarnings 0)
foreach(sirLog IN ITEMS "${SIR_REPORT_ROOT}/logs/${SIR_LABEL}-configure.log"
    "${SIR_REPORT_ROOT}/logs/${SIR_LABEL}-build.log")
    if(EXISTS "${sirLog}")
        file(STRINGS "${sirLog}" sirLines REGEX "[Ww]arning:|[Ww]arning C[0-9]+")
        list(LENGTH sirLines sirFound)
        math(EXPR sirWarnings "${sirWarnings} + ${sirFound}")
    endif()
endforeach()
set(sirBinaryBytes -1)
file(GLOB_RECURSE sirBinaries LIST_DIRECTORIES false
    "${SIR_REPO_ROOT}/out/*/${SIR_PRESET}/*")
foreach(sirFile IN LISTS sirBinaries)
    get_filename_component(sirName "${sirFile}" NAME)
    if(sirName STREQUAL "SIREngine" OR sirName STREQUAL "SIREngine.exe")
        file(SIZE "${sirFile}" sirBinaryBytes)
        break()
    endif()
endforeach()
string(TIMESTAMP sirTimestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
execute_process(COMMAND git -C "${SIR_REPO_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE sirSha OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET RESULT_VARIABLE sirGitStatus)
if(NOT sirGitStatus EQUAL 0 OR NOT sirSha MATCHES "^[0-9a-f]+$")
    set(sirSha "unknown")
endif()
set(sirOverall "pass")
if(NOT SIR_CONFIG_EXIT EQUAL 0 OR NOT SIR_BUILD_EXIT EQUAL 0 OR
   NOT SIR_TEST_EXIT EQUAL 0 OR
   (SIR_PACKAGE_REQUIRED AND SIR_PRESET STREQUAL "release" AND NOT SIR_PACKAGE_EXIT EQUAL 0))
    set(sirOverall "fail")
endif()
set(sirJson "{
  \"schema\": 1,
  \"label\": \"${SIR_LABEL}\",
  \"preset\": \"${SIR_PRESET}\",
  \"commit\": \"${sirSha}\",
  \"generated_utc\": \"${sirTimestamp}\",
  \"result\": \"${sirOverall}\",
  \"configure_exit\": ${SIR_CONFIG_EXIT},
  \"build_exit\": ${SIR_BUILD_EXIT},
  \"test_exit\": ${SIR_TEST_EXIT},
  \"package_exit\": ${SIR_PACKAGE_EXIT},
  \"configure_seconds\": ${SIR_CONFIG_SECONDS},
  \"build_seconds\": ${SIR_BUILD_SECONDS},
  \"test_seconds\": ${SIR_TEST_SECONDS},
  \"package_seconds\": ${SIR_PACKAGE_SECONDS},
  \"tests\": ${sirTests},
  \"warnings\": ${sirWarnings},
  \"binary_bytes\": ${sirBinaryBytes}
}
")
file(WRITE "${SIR_REPORT_ROOT}/metrics/${SIR_LABEL}.json" "${sirJson}")
message(STATUS "Nightly metrics: ${SIR_LABEL}, ${sirOverall}, ${sirTests} tests, ${sirWarnings} warnings, ${sirBinaryBytes} bytes")
