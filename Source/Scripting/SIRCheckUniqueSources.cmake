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

# Reject duplicate scripting translation units even when paths are spelled differently.
function(sir_check_unique_scripting_sources sirTarget)
    get_target_property(sirSources "${sirTarget}" SOURCES)
    get_target_property(sirSourceRoot "${sirTarget}" SOURCE_DIR)
    set(sirCanonicalSources "")
    foreach(sirSource IN LISTS sirSources)
        if(sirSource MATCHES "\\$<")
            message(FATAL_ERROR "Unsupported generator-expression SIRScripting source: ${sirSource}")
        endif()
        cmake_path(SET sirCanonicalSource NORMALIZE "${sirSource}")
        cmake_path(ABSOLUTE_PATH sirCanonicalSource BASE_DIRECTORY "${sirSourceRoot}" NORMALIZE)
        if(sirCanonicalSource IN_LIST sirCanonicalSources)
            message(FATAL_ERROR "Duplicate SIRScripting source entry: ${sirSource} (resolved to ${sirCanonicalSource})")
        endif()
        list(APPEND sirCanonicalSources "${sirCanonicalSource}")
    endforeach()
endfunction()
