# Repeated CTest runs must be idempotent; the standalone cooker intentionally
# refuses to overwrite an already-published target.
if (NOT EXISTS "${COOKER}" OR NOT EXISTS "${INPUT}" OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Missing SIRLangCooker, source fixture or output path")
endif()

file(REMOVE "${OUTPUT}" "${OUTPUT}.tmp")
# A stale temporary file must not be reused, truncated or removed.
file(WRITE "${OUTPUT}.tmp" "stale-temporary-sentinel")
execute_process(
    COMMAND "${COOKER}" "${INPUT}" "${OUTPUT}" 101 201 301 401 1 1
    RESULT_VARIABLE eResult
    OUTPUT_VARIABLE szOutput
    ERROR_VARIABLE szErrors
)
if (NOT eResult EQUAL 0)
    message(FATAL_ERROR "SIRLang cook failed: ${eResult}\n${szOutput}\n${szErrors}")
endif()
if (NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "Cooker returned success without publishing the artifact")
endif()
file(READ "${OUTPUT}" szHeader LIMIT 8 HEX)
if (NOT szHeader STREQUAL "5349524c42433031")
    message(FATAL_ERROR "Cooked artifact magic mismatched")
endif()
file(READ "${OUTPUT}.tmp" szSentinel)
if (NOT szSentinel STREQUAL "stale-temporary-sentinel")
    message(FATAL_ERROR "Cooker modified a pre-existing temporary file")
endif()

file(SHA256 "${OUTPUT}" szPublishedHash)
execute_process(
    COMMAND "${COOKER}" "${INPUT}" "${OUTPUT}" 101 201 301 401 1 1
    RESULT_VARIABLE eSecondCook
)
if (eSecondCook EQUAL 0)
    message(FATAL_ERROR "Cooker overwrote an already-published artifact")
endif()
file(SHA256 "${OUTPUT}" szRetainedHash)
if (NOT szPublishedHash STREQUAL szRetainedHash)
    message(FATAL_ERROR "Failed repeat cook altered the published artifact")
endif()
file(REMOVE "${OUTPUT}.tmp")

# Run independent concurrent publishers. A CMake execute_process COMMAND
# chain is a stdout pipeline and can induce SIGPIPE when one cooker exits.
file(REMOVE "${OUTPUT}.race")
find_program(SIR_PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
execute_process(
    COMMAND "${SIR_PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/SIRLangCookerRace.py"
        "${COOKER}" "${INPUT}" "${OUTPUT}.race"
    RESULT_VARIABLE eRace
    OUTPUT_VARIABLE szRaceOutput
    ERROR_VARIABLE szRaceErrors
)
if (NOT eRace EQUAL 0)
    message(FATAL_ERROR "Concurrent cook did not produce exactly one winner: ${eRace}\n${szRaceOutput}\n${szRaceErrors}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${OUTPUT}" "${OUTPUT}.race"
    RESULT_VARIABLE eRaceCompare
)
if (NOT eRaceCompare EQUAL 0)
    message(FATAL_ERROR "Concurrent cook published a corrupt or nondeterministic artifact")
endif()
file(REMOVE "${OUTPUT}.race")

# Empty input is unconditionally rejected. Failure must leave no artifact.
file(WRITE "${OUTPUT}.empty.sir" "")
file(REMOVE "${OUTPUT}.rejected")
execute_process(
    COMMAND "${COOKER}" "${OUTPUT}.empty.sir" "${OUTPUT}.rejected" 101 201 301 401 1 1
    RESULT_VARIABLE eEmptySource
    OUTPUT_QUIET ERROR_QUIET
)
if (eEmptySource EQUAL 0 OR EXISTS "${OUTPUT}.rejected")
    message(FATAL_ERROR "Rejected source published a bytecode artifact")
endif()
file(REMOVE "${OUTPUT}.empty.sir")
