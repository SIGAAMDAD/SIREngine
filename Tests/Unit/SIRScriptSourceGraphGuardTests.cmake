if(NOT EXISTS "${SIR_SCRIPT_SOURCE_GUARD}")
    message(FATAL_ERROR "Missing scripting source guard: ${SIR_SCRIPT_SOURCE_GUARD}")
endif()
if(NOT SIR_SCRIPT_TEST_BINARY_ROOT OR NOT SIR_SCRIPT_TEST_GENERATOR)
    message(FATAL_ERROR "Missing scripting source test build root or generator")
endif()

set(sirFixtureRoot "${SIR_SCRIPT_TEST_BINARY_ROOT}/scripting-source-guard")
file(MAKE_DIRECTORY "${sirFixtureRoot}/subdir")
file(WRITE "${sirFixtureRoot}/one.cpp" "int sirSourceOne() { return 1; }\n")
file(WRITE "${sirFixtureRoot}/other.cpp" "int sirSourceOther() { return 2; }\n")
file(WRITE "${sirFixtureRoot}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 4.4.5)\n"
    "project(ScriptingSourceGuard LANGUAGES CXX)\n"
    "include(\"${SIR_SCRIPT_SOURCE_GUARD}\")\n"
    "add_library(SIRScripting STATIC one.cpp other.cpp)\n"
    "if(SIR_SCRIPT_DUPLICATE_VARIANT STREQUAL \"exact\")\n"
    "  target_sources(SIRScripting PRIVATE one.cpp)\n"
    "elseif(SIR_SCRIPT_DUPLICATE_VARIANT STREQUAL \"dot\")\n"
    "  target_sources(SIRScripting PRIVATE ./one.cpp)\n"
    "elseif(SIR_SCRIPT_DUPLICATE_VARIANT STREQUAL \"parent\")\n"
    "  target_sources(SIRScripting PRIVATE subdir/../one.cpp)\n"
    "endif()\n"
    "sir_check_unique_scripting_sources(SIRScripting)\n")

foreach(sirVariant IN ITEMS clean exact dot parent)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${sirFixtureRoot}" -B "${sirFixtureRoot}/build-${sirVariant}"
            -G "${SIR_SCRIPT_TEST_GENERATOR}" "-DSIR_SCRIPT_DUPLICATE_VARIANT=${sirVariant}"
        RESULT_VARIABLE sirConfigureResult
        OUTPUT_VARIABLE sirConfigureStdout
        ERROR_VARIABLE sirConfigureStderr)
    if(sirVariant STREQUAL "clean")
        if(NOT sirConfigureResult EQUAL 0)
            message(FATAL_ERROR "Unique scripting sources did not configure: ${sirConfigureStdout}\n${sirConfigureStderr}")
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" --build "${sirFixtureRoot}/build-${sirVariant}"
            RESULT_VARIABLE sirBuildResult OUTPUT_VARIABLE sirBuildStdout ERROR_VARIABLE sirBuildStderr)
        if(NOT sirBuildResult EQUAL 0)
            message(FATAL_ERROR "Unique scripting sources did not compile: ${sirBuildStdout}\n${sirBuildStderr}")
        endif()
    else()
        if(sirConfigureResult EQUAL 0)
            message(FATAL_ERROR "Accepted duplicate scripting source variant '${sirVariant}'")
        endif()
        if(NOT "${sirConfigureStdout}\n${sirConfigureStderr}" MATCHES "Duplicate SIRScripting source entry")
            message(FATAL_ERROR "Wrong rejection reason for '${sirVariant}': ${sirConfigureStdout}\n${sirConfigureStderr}")
        endif()
    endif()
endforeach()
