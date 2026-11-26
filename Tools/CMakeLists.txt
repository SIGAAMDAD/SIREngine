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

# Tools and offline cookers own their target declarations here.

# -- Tools/SceneCooker/Animation --
add_library(SIRAnimationCook STATIC ${PROJECT_SOURCE_DIR}/Tools/SceneCooker/Animation/SIRAnimationCooker.cpp)
add_library(SIR::AnimationCook ALIAS SIRAnimationCook)
target_link_libraries(SIRAnimationCook PUBLIC SIR::Animation)
target_include_directories(SIRAnimationCook PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Tools"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRAnimationCook PUBLIC cxx_std_17)
sir_configure_owned_target(SIRAnimationCook)

# -- Tools/SIRPackCooker --
# Offline SIRPack V1 writer consumes the dedicated package target.
add_library(SIRPackCooker STATIC ${PROJECT_SOURCE_DIR}/Tools/SIRPackCooker/SIRPackCooker.cpp ${PROJECT_SOURCE_DIR}/Tools/SIRPackCooker/SIRPackShippingGuard.cpp)
add_library(SIR::PackCooker ALIAS SIRPackCooker)
target_include_directories(SIRPackCooker PUBLIC "${PROJECT_SOURCE_DIR}")
target_compile_features(SIRPackCooker PUBLIC cxx_std_17)
target_link_libraries(SIRPackCooker PUBLIC SIR::Pack SIR::Resources)
sir_configure_owned_target(SIRPackCooker)

# -- Tools/GeometryCooker --
add_library(SIRDisplacementCooker STATIC ${PROJECT_SOURCE_DIR}/Tools/GeometryCooker/SIRDisplacementCooker.cpp)
add_library(SIR::DisplacementCooker ALIAS SIRDisplacementCooker)
target_include_directories(SIRDisplacementCooker PUBLIC "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/Source")
target_compile_features(SIRDisplacementCooker PUBLIC cxx_std_17)
target_link_libraries(SIRDisplacementCooker PUBLIC SIR::Resources)
sir_configure_owned_target(SIRDisplacementCooker)

if(NOT SIR_WITH_MESHOPTIMIZER)
    message(STATUS "SIRGeometryCooker disabled: enable SIR_WITH_MESHOPTIMIZER with pinned submodule")
endif()

if(SIR_WITH_MESHOPTIMIZER)
add_library(SIRGeometryCooker STATIC ${PROJECT_SOURCE_DIR}/Tools/GeometryCooker/SIRGeometryCooker.cpp)
add_library(SIR::GeometryCooker ALIAS SIRGeometryCooker)
target_include_directories(SIRGeometryCooker PUBLIC "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/Source")
target_compile_features(SIRGeometryCooker PUBLIC cxx_std_17)
target_link_libraries(SIRGeometryCooker PUBLIC SIR::MeshPackSchema SIR::PackCooker SIR::MeshOptimizer SIR::DisplacementCooker)
sir_configure_owned_target(SIRGeometryCooker)

add_executable(sir-geometry-cooker ${PROJECT_SOURCE_DIR}/Tools/GeometryCooker/SIRGeometryCookerCLI.cpp)
target_compile_features(sir-geometry-cooker PRIVATE cxx_std_17)
target_link_libraries(sir-geometry-cooker PRIVATE SIR::GeometryCooker)
sir_configure_owned_target(sir-geometry-cooker)

endif()

# -- Tools/ShaderCooker --
# ShaderCooker is an offline host tool. No native renderer/device linkage.
add_library(SIRShaderCookFrontend STATIC
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Frontend/SIRShaderFrontend.cpp
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Targets/SIRShaderTargets.cpp
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/MaterialIR/SIRMaterialIR.cpp
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/MaterialIR/SIRMaterialIREmitter.cpp
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/MaterialIR/Derivatives/SIRMaterialDerivatives.cpp
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Archive/SIRShaderArchive.cpp
    ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Recipes/SIRFixedRecipeCooker.cpp)
add_library(SIR::ShaderCookFrontend ALIAS SIRShaderCookFrontend)
target_compile_features(SIRShaderCookFrontend PUBLIC cxx_std_17)
target_include_directories(SIRShaderCookFrontend PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
if(COMMAND sir_configure_owned_target)
    sir_configure_owned_target(SIRShaderCookFrontend)
endif()








add_executable(SIRShaderCooker ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/CLI/SIRShaderCookerMain.cpp)
target_link_libraries(SIRShaderCooker PRIVATE SIR::ShaderCookFrontend)
if(COMMAND sir_configure_owned_target)
    sir_configure_owned_target(SIRShaderCooker)
endif()



# Supply an explicit host DXC binary to activate genuine offline compiler tests.
# No DXC binary is bundled and no runtime compiler is added to the engine.

# Native FXC verification runs only when a separately provisioned, legally usable
# Windows compiler is explicitly configured. It never masquerades as mock QA.
