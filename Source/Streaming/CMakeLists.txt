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
# Demand and lifecycle authority only. Physical payloads remain in GRDG.
add_library(SIRStreaming STATIC
    SIRStreamingCoordinator.cpp
    SIRStreamingDemandReconciler.cpp SIRStreamingProviderBridge.cpp
    SIRStreamingUnitResolver.cpp SIRPredictionSystem.cpp SIRResourceStreamingCoordinator.cpp SIRStreamingRuntime.cpp)
add_library(SIR::Streaming ALIAS SIRStreaming)
target_include_directories(SIRStreaming PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRStreaming PUBLIC cxx_std_17)
target_link_libraries(SIRStreaming PUBLIC SIR::Core SIR::Resources)
sir_configure_owned_target(SIRStreaming)
add_subdirectory(Pack)
target_link_libraries(SIRStreaming PUBLIC SIR::Pack)
