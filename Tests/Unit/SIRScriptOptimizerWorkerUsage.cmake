if(NOT DEFINED SIR_WORKER OR NOT EXISTS "${SIR_WORKER}")
    message(FATAL_ERROR "Optimizer worker executable is missing")
endif()
execute_process(
    COMMAND "${SIR_WORKER}"
    RESULT_VARIABLE sirExit
    OUTPUT_VARIABLE sirOutput
    ERROR_VARIABLE sirError
    TIMEOUT 10)
if(NOT "${sirExit}" STREQUAL "2")
    message(FATAL_ERROR "Optimizer worker usage must exit 2; got ${sirExit}: ${sirError}")
endif()
if(NOT sirError MATCHES "SIRScriptOptimizerWorker: usage <input-root> <cache-root> <backend-library>")
    message(FATAL_ERROR "Optimizer worker did not report the required CLI usage")
endif()
