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
add_library(SIRWorldHLOD STATIC HLOD/SIRWorldHLOD.cpp HLOD/SIRWorldHLODGRDG.cpp HLOD/SIRWorldHLODManifest.cpp HLOD/SIRWorldHLODPins.cpp HLOD/SIRWorldHLODDemand.cpp HLOD/SIRWorldHLODProjection.cpp)
add_library(SIR::WorldHLOD ALIAS SIRWorldHLOD)
target_include_directories(SIRWorldHLOD PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRWorldHLOD PUBLIC cxx_std_17)
target_link_libraries(SIRWorldHLOD PUBLIC SIR::Resources)
sir_configure_owned_target(SIRWorldHLOD)

# Spatial demand generation, independent of resource payload ownership.
add_library(SIRWorld STATIC Partition/SIRWorldPartition.cpp)
add_library(SIR::World ALIAS SIRWorld)
target_include_directories(SIRWorld PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRWorld PUBLIC cxx_std_17)
target_link_libraries(SIRWorld PUBLIC SIR::Streaming)
sir_configure_owned_target(SIRWorld)

# Independent world-space query consumer. Each instance owns one existing
# collision-source lease; BVH nodes remain a derived CPU cache, not GRDG data.
add_library(SIRWorldMeshQuery STATIC Queries/SIRWorldMeshQuery.cpp)
add_library(SIR::WorldMeshQuery ALIAS SIRWorldMeshQuery)
target_include_directories(SIRWorldMeshQuery PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRWorldMeshQuery PUBLIC cxx_std_17)
target_link_libraries(SIRWorldMeshQuery PUBLIC SIR::MeshCollisionBVH)
sir_configure_owned_target(SIRWorldMeshQuery)
