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
add_library(SIRResources STATIC SIRResourceGraph.cpp SIRResourceGraphSync.cpp SIRResourceReference.cpp SIRResourcePinPtr.cpp SIRResourceMemoryAdapter.cpp SIRResourceDerivedRegistry.cpp SIRResourceFileLoad.cpp)
add_library(SIR::Resources ALIAS SIRResources)
find_package(Threads REQUIRED)
target_link_libraries(SIRResources PUBLIC SIR::Core Threads::Threads)
target_include_directories(SIRResources PUBLIC "${PROJECT_SOURCE_DIR}/Source")
target_compile_features(SIRResources PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(SIRResources PRIVATE /W4)
else()
    target_compile_options(SIRResources PRIVATE -Wall -Wextra -Wpedantic)
endif()
sir_configure_owned_target(SIRResources)

