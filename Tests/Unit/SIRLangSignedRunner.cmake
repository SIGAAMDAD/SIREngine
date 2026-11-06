# Source-to-bytecode-to-VM smoke for narrow signed integer arguments.
if (NOT EXISTS "${COOKER}" OR NOT EXISTS "${RUNNER}" OR NOT EXISTS "${INPUT}" OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Missing SIRLang toolchain, signed fixture or output")
endif()
file(REMOVE "${OUTPUT}")
execute_process(
    COMMAND "${COOKER}" "${INPUT}" "${OUTPUT}" 101 201 301 401 1 1
    RESULT_VARIABLE eCook
    ERROR_VARIABLE szErrors
)
if (NOT eCook EQUAL 0)
    message(FATAL_ERROR "Signed cook failed: ${eCook}\n${szErrors}")
endif()
execute_process(
    COMMAND "${RUNNER}" "${OUTPUT}" 0 -123
    RESULT_VARIABLE eRun
    OUTPUT_VARIABLE szOutput
    ERROR_VARIABLE szErrors
)
if (NOT eRun EQUAL 0 OR NOT szOutput MATCHES "value_bits=18446744073709551493([ \r\n]|$)")
    message(FATAL_ERROR "Signed execution mismatch: ${eRun}\n${szOutput}\n${szErrors}")
endif()
execute_process(COMMAND "${RUNNER}" "${OUTPUT}" 0 -32768 RESULT_VARIABLE eMin)
execute_process(COMMAND "${RUNNER}" "${OUTPUT}" 0 -32769 RESULT_VARIABLE eUnderflow)
execute_process(COMMAND "${RUNNER}" "${OUTPUT}" 0 32768 RESULT_VARIABLE eOverflow)
if (NOT eMin EQUAL 0 OR eUnderflow EQUAL 0 OR eOverflow EQUAL 0)
    message(FATAL_ERROR "Signed i16 range enforcement failed")
endif()
