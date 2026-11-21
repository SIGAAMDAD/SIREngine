/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Tools/ShaderCooker/Targets/SIRShaderTargets.h>

sirShaderCookResult_t sirShaderCookResolveTargetInfo( sirGraphicsBackend_t eBackend, uint32_t nStage,
                                                      sirShaderCookTarget_t *pTarget ) noexcept
{
	if ( pTarget == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	*pTarget = {};
	const char *pszPrefix = nullptr;
	switch ( nStage ) {
	case SIR_GRAPHICS_STAGE_VERTEX:
		pszPrefix = "vs";
		break;
	case SIR_GRAPHICS_STAGE_FRAGMENT:
		pszPrefix = "ps";
		break;
	case SIR_GRAPHICS_STAGE_COMPUTE:
		pszPrefix = "cs";
		break;
	case SIR_GRAPHICS_STAGE_GEOMETRY:
		pszPrefix = "gs";
		break;
	case SIR_GRAPHICS_STAGE_TESS_CONTROL:
		pszPrefix = "hs";
		break;
	case SIR_GRAPHICS_STAGE_TESS_EVALUATION:
		pszPrefix = "ds";
		break;
	case SIR_GRAPHICS_STAGE_MESH:
		pszPrefix = "ms";
		break;
	case SIR_GRAPHICS_STAGE_TASK:
		pszPrefix = "as";
		break;
	default:
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	const char *pszVersion = nullptr;
	switch ( eBackend ) {
	case SIR_GRAPHICS_BACKEND_VULKAN:
	case SIR_GRAPHICS_BACKEND_D3D12:
		pszVersion = "6_6";
		pTarget->eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
		pTarget->eCompiler = SIR_SHADER_COMPILER_DXC;
		pTarget->eFormat = eBackend == SIR_GRAPHICS_BACKEND_VULKAN ? SIR_GRAPHICS_SHADER_SPIRV : SIR_GRAPHICS_SHADER_DXIL;
		pTarget->bSpirv = eBackend == SIR_GRAPHICS_BACKEND_VULKAN;
		break;
	case SIR_GRAPHICS_BACKEND_D3D11:
		if ( nStage == SIR_GRAPHICS_STAGE_MESH || nStage == SIR_GRAPHICS_STAGE_TASK ) {
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		pszVersion = "5_0";
		pTarget->eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
		pTarget->eCompiler = SIR_SHADER_COMPILER_FXC;
		pTarget->eFormat = SIR_GRAPHICS_SHADER_DXBC;
		break;
	case SIR_GRAPHICS_BACKEND_D3D10:
		if ( nStage != SIR_GRAPHICS_STAGE_VERTEX && nStage != SIR_GRAPHICS_STAGE_FRAGMENT && nStage != SIR_GRAPHICS_STAGE_GEOMETRY ) {
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		pszVersion = "4_0";
		pTarget->eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
		pTarget->eCompiler = SIR_SHADER_COMPILER_FXC;
		pTarget->eFormat = SIR_GRAPHICS_SHADER_DXBC;
		break;
	case SIR_GRAPHICS_BACKEND_D3D9:
		if ( nStage != SIR_GRAPHICS_STAGE_VERTEX && nStage != SIR_GRAPHICS_STAGE_FRAGMENT ) {
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		pszVersion = "3_0";
		pTarget->eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
		pTarget->eCompiler = SIR_SHADER_COMPILER_FXC;
		pTarget->eFormat = SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS;
		break;
	case SIR_GRAPHICS_BACKEND_D3D8:
		if ( nStage != SIR_GRAPHICS_STAGE_VERTEX && nStage != SIR_GRAPHICS_STAGE_FRAGMENT ) {
			return SIR_SHADER_COOK_UNSUPPORTED;
		}
		pszVersion = "1_1";
		pTarget->eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
		pTarget->eCompiler = SIR_SHADER_COMPILER_FXC;
		pTarget->eFormat = SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS;
		break;
	default:
		// GL HLSL->GLSL and ARB assembly need a qualified cross-compiler;
		// D3D7 uses the separate shaderless fixed-function recipe cooker.
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
	pTarget->sProfile = std::string( pszPrefix ) + "_" + pszVersion;
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirShaderCookResolveTarget( sirGraphicsBackend_t eBackend, uint32_t nStage, std::string *pProfile, bool *pSpirv )
{
	if ( pProfile == nullptr || pSpirv == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	pProfile->clear();
	*pSpirv = false;
	sirShaderCookTarget_t target = {};
	const sirShaderCookResult_t eResult = sirShaderCookResolveTargetInfo( eBackend, nStage, &target );
	if ( eResult == SIR_SHADER_COOK_OK ) {
		*pProfile = target.sProfile;
		*pSpirv = target.bSpirv;
	}
	return eResult;
}
