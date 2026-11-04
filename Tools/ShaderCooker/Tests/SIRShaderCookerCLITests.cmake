file(MAKE_DIRECTORY "${SIR_OUTPUT_DIR}")
set(SIR_BIN "${SIR_OUTPUT_DIR}/test.bin")
execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}"
        --output "${SIR_BIN}" --define TEST=1
    RESULT_VARIABLE SIR_FIRST_RESULT
    OUTPUT_VARIABLE SIR_FIRST_STDOUT
    ERROR_VARIABLE SIR_FIRST_STDERR)
if(NOT SIR_FIRST_RESULT EQUAL 0)
    message(FATAL_ERROR "ShaderCooker Vulkan mock invocation failed: ${SIR_FIRST_STDERR}")
endif()
file(READ "${SIR_BIN}" SIR_FIRST_BINARY HEX)
if(NOT SIR_FIRST_BINARY MATCHES "^03022307")
    message(FATAL_ERROR "ShaderCooker did not invoke SPIR-V target")
endif()
file(READ "${SIR_BIN}.manifest" SIR_FIRST_MANIFEST)
if(NOT SIR_FIRST_MANIFEST MATCHES "PROFILE=cs_6_6")
    message(FATAL_ERROR "Missing versioned source-bound compiler manifest")
endif()
execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}"
        --output "${SIR_BIN}" --define TEST=1
    RESULT_VARIABLE SIR_SECOND_RESULT)
file(READ "${SIR_BIN}.manifest" SIR_SECOND_MANIFEST)
if(NOT SIR_SECOND_RESULT EQUAL 0 OR NOT SIR_FIRST_MANIFEST STREQUAL SIR_SECOND_MANIFEST)
    message(FATAL_ERROR "Deterministic compiler manifest mismatch")
endif()
execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend d3d12 --stage compute --compiler "${SIR_MOCK_COMPILER}"
        --output "${SIR_BIN}"
    RESULT_VARIABLE SIR_DXIL_RESULT)
file(READ "${SIR_BIN}" SIR_DXIL_BINARY HEX)
if(NOT SIR_DXIL_RESULT EQUAL 0 OR NOT SIR_DXIL_BINARY MATCHES "^44584243")
    message(FATAL_ERROR "ShaderCooker did not invoke DXIL target")
endif()
execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend opengl --stage compute --compiler "${SIR_MOCK_COMPILER}"
        --output "${SIR_BIN}"
    RESULT_VARIABLE SIR_UNSUPPORTED_RESULT)
if(SIR_UNSUPPORTED_RESULT EQUAL 0)
    message(FATAL_ERROR "GL target silently accepted unsupported compiler")
endif()
execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend d3d12 --stage compute --compiler "${SIR_FIXTURES}/missing-dxc"
        --output "${SIR_BIN}"
    RESULT_VARIABLE SIR_MISSING_COMPILER_RESULT)
if(SIR_MISSING_COMPILER_RESULT EQUAL 0)
    message(FATAL_ERROR "Missing compiler was accepted")
endif()

execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend d3d12 --stage compute --compiler "${SIR_MOCK_COMPILER}"
        --output "${SIR_BIN}" --define CORRUPT=1
    RESULT_VARIABLE SIR_CORRUPT_RESULT)
if(SIR_CORRUPT_RESULT EQUAL 0 OR EXISTS "${SIR_BIN}" OR EXISTS "${SIR_BIN}.manifest")
    message(FATAL_ERROR "Corrupt DXIL binary was accepted or stale output retained")
endif()
execute_process(
    COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}"
        --output "${SIR_BIN}" --define FAIL=1
    RESULT_VARIABLE SIR_FAILURE_RESULT)
if(SIR_FAILURE_RESULT EQUAL 0 OR EXISTS "${SIR_BIN}" OR EXISTS "${SIR_BIN}.manifest")
    message(FATAL_ERROR "Compiler failure was accepted or stale artifact retained")
endif()
