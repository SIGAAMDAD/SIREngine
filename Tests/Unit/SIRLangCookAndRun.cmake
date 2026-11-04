if (NOT EXISTS "${COOKER}" OR NOT EXISTS "${RUNNER}" OR NOT EXISTS "${INPUT}" OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Missing compiler/runner, source or output")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
    COMMAND "${COOKER}" "${INPUT}" "${OUTPUT}" 101 201 301 401
    RESULT_VARIABLE eCook
    OUTPUT_VARIABLE szCookOutput
    ERROR_VARIABLE szCookErrors
)
if (NOT eCook EQUAL 0)
    message(FATAL_ERROR "Cook failed: ${eCook}\n${szCookErrors}")
endif()
execute_process(
    COMMAND "${RUNNER}" "${OUTPUT}" 0 42
    RESULT_VARIABLE eRun
    OUTPUT_VARIABLE szRunOutput
    ERROR_VARIABLE szRunErrors
)
if (NOT eRun EQUAL 0 OR NOT szRunOutput MATCHES "value_bits=42([ \r\n]|$)")
    message(FATAL_ERROR "Cooked execution failed: ${eRun}\n${szRunOutput}\n${szRunErrors}")
endif()
