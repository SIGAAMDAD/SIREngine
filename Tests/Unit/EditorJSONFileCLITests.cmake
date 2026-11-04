# Native CLI integration: a completed inspection must publish a whole JSON file.
if(NOT DEFINED SIR_EDITOR_EXECUTABLE OR NOT DEFINED SIR_EDITOR_SCENE OR NOT DEFINED SIR_EDITOR_OUTPUT)
    message(FATAL_ERROR "Missing SIREditor JSON file test arguments")
endif()
file(REMOVE "${SIR_EDITOR_OUTPUT}" "${SIR_EDITOR_OUTPUT}.tmp")
execute_process(
    COMMAND "${SIR_EDITOR_EXECUTABLE}" --inspect-scene-json-file "${SIR_EDITOR_SCENE}" "${SIR_EDITOR_OUTPUT}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE diagnostic)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "JSON file export failed with ${status}: ${diagnostic}")
endif()
if(NOT EXISTS "${SIR_EDITOR_OUTPUT}")
    message(FATAL_ERROR "No JSON result was published")
endif()
if(EXISTS "${SIR_EDITOR_OUTPUT}.tmp")
    message(FATAL_ERROR "Staging file leaked after export")
endif()
file(READ "${SIR_EDITOR_OUTPUT}" contents)
string(FIND "${contents}" "\"schema\":1" schema_offset)
string(FIND "${contents}" "\"nodes\":[" node_offset)
string(FIND "${contents}" "\"resources\":[" resource_offset)
if(schema_offset EQUAL -1 OR node_offset EQUAL -1 OR resource_offset EQUAL -1)
    message(FATAL_ERROR "Native result missing expected JSON fields")
endif()
file(REMOVE "${SIR_EDITOR_OUTPUT}")
message(STATUS "PASS: SIREditor CLI published a complete JSON scene snapshot")

# The native host must refuse to replace the authored .tscn with JSON.
file(SHA256 "${SIR_EDITOR_SCENE}" before_hash)
execute_process(
    COMMAND "${SIR_EDITOR_EXECUTABLE}" --inspect-scene-json-file "${SIR_EDITOR_SCENE}" "${SIR_EDITOR_SCENE}"
    RESULT_VARIABLE alias_status
    OUTPUT_VARIABLE alias_output
    ERROR_VARIABLE alias_error)
if(alias_status EQUAL 0)
    message(FATAL_ERROR "Native CLI incorrectly accepted scene/output alias")
endif()
file(SHA256 "${SIR_EDITOR_SCENE}" after_hash)
if(NOT before_hash STREQUAL after_hash)
    message(FATAL_ERROR "Native CLI modified its source scene")
endif()
