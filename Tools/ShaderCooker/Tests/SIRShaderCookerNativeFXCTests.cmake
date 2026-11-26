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

file(MAKE_DIRECTORY "${SIR_OUTPUT_DIR}")
foreach(SIR_STAGE vertex fragment)
    if(SIR_STAGE STREQUAL "vertex")
        set(SIR_ENTRY VSMain)
        set(SIR_MAGIC "0002feff")
    else()
        set(SIR_ENTRY PSMain)
        set(SIR_MAGIC "0002ffff")
    endif()
    foreach(SIR_MODEL 2 3)
        set(SIR_BINARY "${SIR_OUTPUT_DIR}/${SIR_STAGE}-${SIR_MODEL}.bin")
        execute_process(
            COMMAND "${SIR_COOKER}" --root "${SIR_FIXTURES}" --source d3d9.hlsl
                --entry "${SIR_ENTRY}" --backend d3d9 --stage "${SIR_STAGE}"
                --shader-model "${SIR_MODEL}" --compiler "${SIR_FXC}" --output "${SIR_BINARY}"
            RESULT_VARIABLE SIR_STATUS ERROR_VARIABLE SIR_DIAGNOSTIC)
        if(NOT SIR_STATUS EQUAL 0)
            message(FATAL_ERROR "Native FXC ${SIR_STAGE} SM${SIR_MODEL} failed: ${SIR_DIAGNOSTIC}")
        endif()
        file(READ "${SIR_BINARY}" SIR_TOKENS HEX LIMIT 4)
        if(SIR_STAGE STREQUAL "vertex")
            set(SIR_EXPECTED "00${SIR_MODEL}feff")
        else()
            set(SIR_EXPECTED "00${SIR_MODEL}ffff")
        endif()
        if(NOT SIR_TOKENS STREQUAL SIR_EXPECTED)
            message(FATAL_ERROR "FXC emitted wrong ${SIR_STAGE} SM${SIR_MODEL} token header: ${SIR_TOKENS}")
        endif()
        file(READ "${SIR_BINARY}.manifest" SIR_MANIFEST)
        if(NOT SIR_MANIFEST MATCHES "BINARY_FORMAT=D3D9_TOKENS")
            message(FATAL_ERROR "FXC manifest missing D3D9 token identity")
        endif()
    endforeach()
endforeach()
