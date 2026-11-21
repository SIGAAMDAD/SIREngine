if(NOT EXISTS "${COOKER}" OR NOT EXISTS "${RUNNER}" OR NOT SIR_CONTEXTUAL_TEST_ROOT)
    message(FATAL_ERROR "Missing cooker, runner, or scoped binary root")
endif()

set(sirRoot "${SIR_CONTEXTUAL_TEST_ROOT}/sir-contextual-cli")
file(REMOVE_RECURSE "${sirRoot}")
file(MAKE_DIRECTORY "${sirRoot}")

function(sirExerciseContextualSource sirName sirText sirExpectedType sirExpectedBits)
    set(sirInput "${sirRoot}/${sirName}.sir")
    set(sirPrimary "${sirRoot}/${sirName}.sirbc")
    set(sirSecondary "${sirRoot}/${sirName}-repeat.sirbc")
    file(WRITE "${sirInput}" "${sirText}\n")
    execute_process(
        COMMAND "${COOKER}" "${sirInput}" "${sirPrimary}" 101 201 301 401 501 601
        RESULT_VARIABLE sirCookResult OUTPUT_VARIABLE sirCookOutput ERROR_VARIABLE sirCookError)
    if(NOT sirCookResult EQUAL 0 OR NOT EXISTS "${sirPrimary}")
        message(FATAL_ERROR "Cook failed for ${sirName}: ${sirCookResult}\n${sirCookOutput}\n${sirCookError}")
    endif()
    execute_process(
        COMMAND "${RUNNER}" "${sirPrimary}" 0
        RESULT_VARIABLE sirRunResult OUTPUT_VARIABLE sirRunOutput ERROR_VARIABLE sirRunError)
    if(NOT sirRunResult EQUAL 0 OR NOT sirRunOutput MATCHES "value_type=${sirExpectedType} value_bits=${sirExpectedBits} ")
        message(FATAL_ERROR "Cooked execution mismatch for ${sirName}: ${sirRunResult}\n${sirRunOutput}\n${sirRunError}")
    endif()
    file(SHA256 "${sirPrimary}" sirPrimaryHash)
    execute_process(
        COMMAND "${COOKER}" "${sirInput}" "${sirSecondary}" 101 201 301 401 501 601
        RESULT_VARIABLE sirRepeatResult OUTPUT_VARIABLE sirRepeatOutput ERROR_VARIABLE sirRepeatError)
    if(NOT sirRepeatResult EQUAL 0 OR NOT EXISTS "${sirSecondary}")
        message(FATAL_ERROR "Repeated cook failed: ${sirName}: ${sirRepeatResult}\n${sirRepeatOutput}\n${sirRepeatError}")
    endif()
    file(SHA256 "${sirSecondary}" sirSecondaryHash)
    if(NOT sirPrimaryHash STREQUAL sirSecondaryHash OR NOT sirCookOutput STREQUAL sirRepeatOutput)
        message(FATAL_ERROR "Nondeterministic bytecode or cache key for ${sirName}")
    endif()
    execute_process(
        COMMAND "${COOKER}" "${sirInput}" "${sirPrimary}" 101 201 301 401 501 601
        RESULT_VARIABLE sirCollisionResult OUTPUT_VARIABLE sirCollisionOutput ERROR_VARIABLE sirCollisionError)
    if(sirCollisionResult EQUAL 0)
        message(FATAL_ERROR "Cooker overwrote its prior published artifact for ${sirName}")
    endif()
    file(SHA256 "${sirPrimary}" sirAfterCollisionHash)
    if(NOT sirAfterCollisionHash STREQUAL sirPrimaryHash)
        message(FATAL_ERROR "Published artifact changed after refused overwrite for ${sirName}")
    endif()
endfunction()

sirExerciseContextualSource("u16" "function f() returns u16 { return 100; }" 6 100)
sirExerciseContextualSource("i8-min" "function f() returns i8 { return -128; }" 3 18446744073709551488)
sirExerciseContextualSource("f64-neg" "function f() returns f64 { return -7.5; }" 13 13843502304583483392)

set(sirBadInput "${sirRoot}/overflow.sir")
set(sirBadOutput "${sirRoot}/overflow.sirbc")
file(WRITE "${sirBadInput}" "function f() returns u8 { return 256; }\n")
execute_process(COMMAND "${COOKER}" "${sirBadInput}" "${sirBadOutput}" 101 201 301 401 501 601
    RESULT_VARIABLE sirBadResult OUTPUT_VARIABLE sirBadStdout ERROR_VARIABLE sirBadStderr)
if(sirBadResult EQUAL 0 OR EXISTS "${sirBadOutput}")
    message(FATAL_ERROR "Overflowing contextual source produced a cooked artifact: ${sirBadStdout}\n${sirBadStderr}")
endif()
file(GLOB sirTemporaries "${sirRoot}/*.tmp-*")
if(sirTemporaries)
    message(FATAL_ERROR "Cooker left uncollected temporary artifacts: ${sirTemporaries}")
endif()
file(REMOVE_RECURSE "${sirRoot}")
