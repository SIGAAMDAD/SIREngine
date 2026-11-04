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

sirShaderCookResult_t sirShaderCookResolveTarget( sirGraphicsBackend_t eBackend, uint32_t nStage, std::string *pProfile, bool *pSpirv )
{
	if ( pProfile == nullptr || pSpirv == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	pProfile->clear();
	*pSpirv = false;
	if ( eBackend != SIR_GRAPHICS_BACKEND_VULKAN && eBackend != SIR_GRAPHICS_BACKEND_D3D12 ) {
		return SIR_SHADER_COOK_UNSUPPORTED;
	}
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
	*pProfile = std::string( pszPrefix ) + "_6_6";
	*pSpirv = eBackend == SIR_GRAPHICS_BACKEND_VULKAN;
	return SIR_SHADER_COOK_OK;
}
