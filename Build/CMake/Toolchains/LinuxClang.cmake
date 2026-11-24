# Clang can select an incomplete, newer GCC installation on Linux. Use the
# installed g++ runtime consistently for both headers and linking.
include_guard(GLOBAL)
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER clang)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++)
endif()
set(SIR_CLANG_GCC_INSTALL_DIR "" CACHE PATH "Optional GCC installation used by Clang")
if(NOT SIR_CLANG_GCC_INSTALL_DIR)
    find_program(_sir_gxx NAMES g++)
    if(_sir_gxx)
        execute_process(COMMAND "${_sir_gxx}" -print-file-name=libstdc++.so
            OUTPUT_VARIABLE _sir_stdcxx OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _sir_gxx_result)
        if(_sir_gxx_result EQUAL 0 AND IS_ABSOLUTE "${_sir_stdcxx}" AND EXISTS "${_sir_stdcxx}")
            get_filename_component(SIR_CLANG_GCC_INSTALL_DIR "${_sir_stdcxx}" DIRECTORY)
        endif()
    endif()
endif()
if(SIR_CLANG_GCC_INSTALL_DIR)
    string(APPEND CMAKE_CXX_FLAGS_INIT " --gcc-install-dir=\"${SIR_CLANG_GCC_INSTALL_DIR}\"")
endif()
