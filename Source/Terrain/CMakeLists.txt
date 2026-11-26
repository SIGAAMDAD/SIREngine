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
add_library(SIRTerrain STATIC Data/SIRTerrainHeightfieldCodec.cpp Data/SIRTerrainRegionBundle.cpp Data/SIRTerrainRegionCatalog.cpp Data/SIRTerrainHeightfieldView.cpp Integration/SIRTerrainResourceBridge.cpp Integration/SIRTerrainRegionPersistence.cpp)
add_library(SIR::Terrain ALIAS SIRTerrain)
target_link_libraries(SIRTerrain PUBLIC SIR::Resources)
target_include_directories(SIRTerrain PUBLIC "${PROJECT_SOURCE_DIR}/Source")
target_compile_features(SIRTerrain PUBLIC cxx_std_17)
sir_configure_owned_target(SIRTerrain)
