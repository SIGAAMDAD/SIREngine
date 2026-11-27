# ===========================================================================
# SIREngine MPLv2 Source Code
# Copyright (C) 2023-2027 Noah Van Til
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
# 
# This software is provided "as is", without warranty of any kind,
# express or implied, including but not limited to the warranties
# of merchantability, fitness for a particular purpose and noninfringement.
# ===========================================================================

# This test must use a real pinned DXC executable.
# The mock compiler is deliberately insufficient for native acceptance.
foreach(SIR_REQUIRED SIR_COOKER SIR_DXC SIR_FIXTURES SIR_OUTPUT_DIR)
    if(NOT DEFINED ${SIR_REQUIRED} OR "${${SIR_REQUIRED}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${SIR_REQUIRED} for native shader compiler verification")
    endif()
endforeach()
file(MAKE_DIRECTORY "${SIR_OUTPUT_DIR}")
foreach(SIR_BACKEND d3d12)
    set(SIR_FIRST "${SIR_OUTPUT_DIR}/${SIR_BACKEND}-first.bin")
    set(SIR_SECOND "${SIR_OUTPUT_DIR}/${SIR_BACKEND}-second.bin")
    foreach(SIR_OUTPUT "${SIR_FIRST}" "${SIR_SECOND}")
        execute_process(
            COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source valid.hlsl --entry main
                --backend "${SIR_BACKEND}" --stage compute --compiler "${SIR_DXC}"
                --output "${SIR_OUTPUT}" --define QUALITY=2
            RESULT_VARIABLE SIR_RESULT ERROR_VARIABLE SIR_ERROR OUTPUT_VARIABLE SIR_STDOUT)
        if(NOT SIR_RESULT EQUAL 0)
            message(FATAL_ERROR "Real DXC ${SIR_BACKEND} compilation failed: ${SIR_ERROR}")
        endif()
    endforeach()
    file(SHA256 "${SIR_FIRST}" SIR_FIRST_HASH)
    file(SHA256 "${SIR_SECOND}" SIR_SECOND_HASH)
    file(READ "${SIR_FIRST}.manifest" SIR_FIRST_MANIFEST)
    file(READ "${SIR_SECOND}.manifest" SIR_SECOND_MANIFEST)
    if(NOT SIR_FIRST_HASH STREQUAL SIR_SECOND_HASH OR NOT SIR_FIRST_MANIFEST STREQUAL SIR_SECOND_MANIFEST)
        message(FATAL_ERROR "DXC output is not deterministic for ${SIR_BACKEND}")
    endif()
    execute_process(COMMAND "${SIR_DXC}" -dumpbin "${SIR_FIRST}"
            RESULT_VARIABLE SIR_REFLECT_RESULT OUTPUT_VARIABLE SIR_REFLECTION ERROR_VARIABLE SIR_REFLECT_ERROR)
    if(NOT SIR_REFLECT_RESULT EQUAL 0 OR NOT SIR_REFLECTION MATCHES "Resource Bindings" OR
            NOT SIR_REFLECTION MATCHES "outputData")
        message(FATAL_ERROR "DXIL reflection missing resource binding: ${SIR_REFLECT_ERROR}")
    endif()
endforeach()
message(STATUS "Real DXC DXIL compilation, reflection and determinism verified")
