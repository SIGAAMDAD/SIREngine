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

# CLI error contracts: distinguish malformed arguments, compiler I/O,
# source rejection, output staging, and corrupt/silent compiler results.
function(sir_expect_cooker_exit nExpected)
    execute_process(COMMAND "${SIR_COOKER}" ${ARGN}
        RESULT_VARIABLE SIR_ACTUAL
        ERROR_VARIABLE SIR_DIAGNOSTIC
        OUTPUT_QUIET)
    if(NOT "${SIR_ACTUAL}" STREQUAL "${nExpected}")
        message(FATAL_ERROR "Expected ShaderCooker exit ${nExpected}, received ${SIR_ACTUAL}: ${SIR_DIAGNOSTIC}")
    endif()
endfunction()

set(SIR_COMMON --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}")

sir_expect_cooker_exit(2 --bogus unused)
sir_expect_cooker_exit(2 --root)
sir_expect_cooker_exit(2 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}" --define INVALID)
sir_expect_cooker_exit(2 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}" --unknown x)
sir_expect_cooker_exit(2 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}")
sir_expect_cooker_exit(4 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend obsolete --stage compute --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}")
sir_expect_cooker_exit(4 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend vulkan --stage invalid --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}")
sir_expect_cooker_exit(4 --root "${SIR_FIXTURES}" --source missing.hlsl --entry main
    --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}")

foreach(SIR_STAGE vertex fragment geometry hull domain mesh task)
    # The mock compiler accepts only cs_6_6, so the return is a deliberate
    # compiler rejection after the corresponding CLI stage was parsed.
    sir_expect_cooker_exit(6 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
        --backend vulkan --stage "${SIR_STAGE}" --compiler "${SIR_MOCK_COMPILER}" --output "${SIR_BIN}")
endforeach()

file(WRITE "${SIR_OUTPUT_DIR}/empty-compiler" "")
sir_expect_cooker_exit(3 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend d3d12 --stage compute --compiler "${SIR_OUTPUT_DIR}/empty-compiler" --output "${SIR_BIN}")
file(WRITE "${SIR_OUTPUT_DIR}/not-an-executable.txt" "definitely not DXC")
sir_expect_cooker_exit(6 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend d3d12 --stage compute --compiler "${SIR_OUTPUT_DIR}/not-an-executable.txt" --output "${SIR_BIN}")

file(WRITE "${SIR_OUTPUT_DIR}/not-a-directory" "block parent")
sir_expect_cooker_exit(5 --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
    --backend vulkan --stage compute --compiler "${SIR_MOCK_COMPILER}"
    --output "${SIR_OUTPUT_DIR}/not-a-directory/shader.bin")

file(MAKE_DIRECTORY "${SIR_BIN}.input.hlsl")
file(WRITE "${SIR_BIN}.input.hlsl/keep" "occupied")
sir_expect_cooker_exit(5 ${SIR_COMMON})
file(REMOVE_RECURSE "${SIR_BIN}.input.hlsl")

file(REMOVE "${SIR_BIN}.manifest")
file(MAKE_DIRECTORY "${SIR_BIN}.manifest")
file(WRITE "${SIR_BIN}.manifest/keep" "occupied")
sir_expect_cooker_exit(8 ${SIR_COMMON})
if(EXISTS "${SIR_BIN}")
    message(FATAL_ERROR "Manifest-write failure left an uncommitted shader binary")
endif()
file(REMOVE_RECURSE "${SIR_BIN}.manifest")

foreach(SIR_MODE NO_OUTPUT SHORT UNALIGNED)
    sir_expect_cooker_exit(7 ${SIR_COMMON} --define "${SIR_MODE}=1")
    if(EXISTS "${SIR_BIN}" OR EXISTS "${SIR_BIN}.manifest")
        message(FATAL_ERROR "Invalid ${SIR_MODE} output left a stale artifact")
    endif()
endforeach()
