# Repeated CTest runs must be idempotent; the standalone cooker intentionally
# refuses to overwrite an already-published target.
if (NOT EXISTS "${COOKER}" OR NOT EXISTS "${INPUT}" OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Missing SIRLangCooker, source fixture or output path")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
    COMMAND "${COOKER}" "${INPUT}" "${OUTPUT}" 101 201 301 401
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
