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
add_library(SIRAudio STATIC SIRAudioSystem.cpp SIRAudioResources.cpp)
add_library(SIR::Audio ALIAS SIRAudio)
target_include_directories(SIRAudio PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRAudio PUBLIC cxx_std_17)
target_link_libraries(SIRAudio PUBLIC SIR::Core SIR::Resources)
sir_configure_owned_target(SIRAudio)

# FMOD context lifecycle controller is SDK-free; the licensed native bridge
# is compiled exclusively for SDK-enabled engine configurations.
add_library(SIRFMODContext STATIC FMOD/SIRFMODContext.cpp FMOD/SIRFMODVoiceBackend.cpp FMOD/SIRFMODBankStore.cpp FMOD/SIRFMODPackageLoader.cpp FMOD/SIRFMODMixerBackend.cpp FMOD/SIRFMODSnapshotBackend.cpp FMOD/SIRFMODRuntime.cpp FMOD/SIRFMODMemoryBudget.cpp FMOD/SIRFMODAudioThread.cpp)
add_library(SIR::FMODContext ALIAS SIRFMODContext)
target_include_directories(SIRFMODContext PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRFMODContext PUBLIC cxx_std_17)
target_link_libraries(SIRFMODContext PUBLIC SIR::Audio)
if(SIR_WITH_FMOD)
    target_sources(SIRFMODContext PRIVATE FMOD/SIRFMODNativeApi.cpp FMOD/SIRFMODVoiceNativeApi.cpp FMOD/SIRFMODBankNativeApi.cpp FMOD/SIRFMODMixerNativeApi.cpp FMOD/SIRFMODSnapshotNativeApi.cpp FMOD/SIRFMODRuntimeNativeApi.cpp FMOD/SIRFMODMemoryNativeApi.cpp)
    target_link_libraries(SIRFMODContext PRIVATE SIR::FMODStudio)
endif()
sir_configure_owned_target(SIRFMODContext)
