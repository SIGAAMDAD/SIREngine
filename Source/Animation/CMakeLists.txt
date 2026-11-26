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

add_library(SIRAnimation STATIC
    Contracts/SIRAnimationContractValidation.cpp
    Contracts/SIRAnimationContractLeasePool.cpp
    Cooked/SIRAnimationCooked.cpp
    Runtime/SIRAnimationSampler.cpp
    Runtime/SIRAnimationSIMD.cpp
    Runtime/SIRAnimationSamplingJobs.cpp)
add_library(SIR::Animation ALIAS SIRAnimation)
target_link_libraries(SIRAnimation PUBLIC SIR::Core SIR::Resources)
target_include_directories(SIRAnimation PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRAnimation PUBLIC cxx_std_17)
sir_configure_owned_target(SIRAnimation)
