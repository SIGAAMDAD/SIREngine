include_guard(GLOBAL)

# Explicit verification targets. CI must invoke sir-standards-check; it is kept
# outside ALL so configuring a standard development build does not require
# analysis tools or prematurely declare legacy sources qualified.
find_package(Python3 QUIET COMPONENTS Interpreter)
find_program(SIR_STANDARDS_CLANG NAMES clang++-23 clang-23 clang++ clang-cl clang)
find_program(SIR_STANDARDS_TIDY NAMES clang-tidy-23 clang-tidy)
find_program(SIR_STANDARDS_FORMAT NAMES clang-format-23 clang-format)

set(SIR_STANDARDS_COMPDB "${CMAKE_BINARY_DIR}/compile_commands.json")
set(SIR_STANDARDS_RUNNER "${PROJECT_SOURCE_DIR}/Tools/Standards/run_checks.py")

if(Python3_Interpreter_FOUND)
    set(SIR_STANDARDS_PYTHON "${Python3_EXECUTABLE}")
else()
    # Normal builds do not require Python. Standards gates fail if absent.
    find_program(SIR_STANDARDS_PYTHON NAMES python3 python)
    if(NOT SIR_STANDARDS_PYTHON)
        set(SIR_STANDARDS_PYTHON "python")
    endif()
endif()
if(NOT SIR_STANDARDS_CLANG)
    set(SIR_STANDARDS_CLANG "clang++")
endif()
if(NOT SIR_STANDARDS_TIDY)
    set(SIR_STANDARDS_TIDY "clang-tidy")
endif()
if(NOT SIR_STANDARDS_FORMAT)
    set(SIR_STANDARDS_FORMAT "clang-format")
endif()

function(sir_add_standards_gate targetName mode)
    add_custom_target(${targetName}
        COMMAND "${SIR_STANDARDS_PYTHON}" "${SIR_STANDARDS_RUNNER}"
            --repo "${PROJECT_SOURCE_DIR}"
            --compdb "${SIR_STANDARDS_COMPDB}"
            --mode "${mode}"
            --clang "${SIR_STANDARDS_CLANG}"
            --clang-tidy "${SIR_STANDARDS_TIDY}"
            --clang-format "${SIR_STANDARDS_FORMAT}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
        COMMENT "SIREngine ${mode} standards gate")
endfunction()

sir_add_standards_gate(sir-format-check format)
sir_add_standards_gate(sir-tidy-check tidy)
sir_add_standards_gate(sir-policy-check policy)
sir_add_standards_gate(sir-policy-regression-check regression)
add_custom_target(sir-header-check
    COMMAND "${SIR_STANDARDS_PYTHON}" "${PROJECT_SOURCE_DIR}/Tools/Standards/header_check.py"
        --repo "${PROJECT_SOURCE_DIR}" --compdb "${SIR_STANDARDS_COMPDB}"
        --clang "${SIR_STANDARDS_CLANG}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL VERBATIM
    COMMENT "SIREngine isolated public/owned header compilation gate")

# Explicit C++17 fallback qualification. Does not alter the current C++23 build.
add_custom_target(sir-compatibility-check
    COMMAND "${CMAKE_COMMAND}" --preset tooling-cxx17
    COMMAND "${CMAKE_COMMAND}" --build --preset tooling-cxx17
    COMMAND "${CMAKE_CTEST_COMMAND}" --preset tooling-cxx17 --no-tests=error --output-on-failure
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL VERBATIM
    COMMENT "SIREngine separate C++17 compatibility build and tests")
add_custom_target(sir-standards-check DEPENDS
    sir-format-check sir-tidy-check sir-policy-check sir-policy-regression-check sir-header-check)

# This is never an implicit 'build everything' dependency unless explicitly requested.
option(SIR_STANDARDS_REQUIRED_IN_ALL "Run standards targets in the default build" OFF)
if(SIR_STANDARDS_REQUIRED_IN_ALL)
    add_custom_target(sir-standards-required ALL DEPENDS sir-standards-check)
endif()

# Publish an authentic Ninja/Make compile database for clangd on both OSes.
# clangd searches ancestors of edited files, so the root copy is deliberate.
sir_add_standards_gate(sir-clangd-db publish-compdb)
