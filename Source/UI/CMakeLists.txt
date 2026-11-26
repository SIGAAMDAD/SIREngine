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

include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")
# SIREngine-owned retained UI tree; platform window and draw implementation
# are connected via the normal platform and rendering ownership contracts.
add_library(SIRUI STATIC SIRControlTree.cpp)
add_library(SIR::UI ALIAS SIRUI)
target_include_directories(SIRUI PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRUI PUBLIC cxx_std_17)
target_link_libraries(SIRUI PUBLIC SIR::Core)
sir_configure_owned_target(SIRUI)
