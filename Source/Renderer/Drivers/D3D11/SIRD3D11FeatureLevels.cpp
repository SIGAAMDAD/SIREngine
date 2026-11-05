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

#include <Renderer/Drivers/D3D11/SIRD3D11FeatureLevels.h>
#include <string.h>

static bool sirD3D11ValidProbeFlags( const sirD3D11FeatureProbe_t *pProbe ) noexcept
{
	if ( pProbe->nD3D11_1InterfaceAvailable > 1 || pProbe->nComputeShader4xAvailable > 1 || pProbe->nIndirectDrawAvailable > 1 ) {
		return false;
	}
	if ( pProbe->nMultisampleAvailable > 1 || pProbe->nPresentationAvailable > 1 || pProbe->nReserved != 0 ) {
		return false;
	}
	return true;
}

static bool sirD3D11FillBaseLimits( uint32_t nLevel, sirD3D11FeatureProfile_t *pProfile ) noexcept
{
	switch ( nLevel ) {
	case SIR_D3D11_FEATURE_LEVEL_9_1:
	case SIR_D3D11_FEATURE_LEVEL_9_2:
		pProfile->nMaximumTexture2D = 2048;
		pProfile->nMaximumColorTargets = 1;
		pProfile->nMaximumShaderResourceSlots = 8;
		pProfile->nShaderProfile = SIR_D3D11_SHADER_PROFILE_4_0_LEVEL_9_1;
		return true;
	case SIR_D3D11_FEATURE_LEVEL_9_3:
		pProfile->nMaximumTexture2D = 4096;
		pProfile->nMaximumColorTargets = 4;
		pProfile->nMaximumShaderResourceSlots = 8;
		pProfile->nShaderProfile = SIR_D3D11_SHADER_PROFILE_4_0_LEVEL_9_3;
		pProfile->nSupportsIndexedInstancing = 1;
		return true;
	case SIR_D3D11_FEATURE_LEVEL_10_0:
	case SIR_D3D11_FEATURE_LEVEL_10_1:
		pProfile->nMaximumTexture2D = 8192;
		pProfile->nMaximumColorTargets = 8;
		pProfile->nMaximumShaderResourceSlots = 128;
		pProfile->nShaderProfile = nLevel == SIR_D3D11_FEATURE_LEVEL_10_0 ? SIR_D3D11_SHADER_PROFILE_4_0 : SIR_D3D11_SHADER_PROFILE_4_1;
		pProfile->nSupportsIndexedInstancing = 1;
		return true;
	case SIR_D3D11_FEATURE_LEVEL_11_0:
	case SIR_D3D11_FEATURE_LEVEL_11_1:
		pProfile->nMaximumTexture2D = 16384;
		pProfile->nMaximumColorTargets = 8;
		pProfile->nMaximumShaderResourceSlots = 128;
		pProfile->nMaximumUavSlots = 8;
		pProfile->nShaderProfile = SIR_D3D11_SHADER_PROFILE_5_0;
		pProfile->nSupportsIndexedInstancing = 1;
		return true;
	default:
		return false;
	}
}

sirGraphicsResult_t sirD3D11BuildFeatureProfile( const sirD3D11FeatureProbe_t *pProbe, sirD3D11FeatureProfile_t *pProfile ) noexcept
{
	if ( pProbe == nullptr || pProfile == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pProbe->nSize < sizeof( sirD3D11FeatureProbe_t ) || pProfile->nSize < sizeof( sirD3D11FeatureProfile_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirD3D11ValidProbeFlags( pProbe ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirD3D11FeatureProfile_t profile = {};
	profile.nSize = sizeof( profile );
	profile.nFeatureLevel = pProbe->nFeatureLevel;
	profile.nDirectQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	profile.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE - 1U );
	profile.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	profile.nD3D11_1InterfaceAvailable = pProbe->nD3D11_1InterfaceAvailable;
	if ( !sirD3D11FillBaseLimits( pProbe->nFeatureLevel, &profile ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	const bool bFeatureLevel10 =
	    pProbe->nFeatureLevel == SIR_D3D11_FEATURE_LEVEL_10_0 || pProbe->nFeatureLevel == SIR_D3D11_FEATURE_LEVEL_10_1;
	const bool bFeatureLevel11 =
	    pProbe->nFeatureLevel == SIR_D3D11_FEATURE_LEVEL_11_0 || pProbe->nFeatureLevel == SIR_D3D11_FEATURE_LEVEL_11_1;
	if ( pProbe->nComputeShader4xAvailable != 0 && !bFeatureLevel10 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pProbe->nIndirectDrawAvailable != 0 && !bFeatureLevel11 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	if ( profile.nMaximumColorTargets > 1 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_MRT;
	}
	if ( pProbe->nMultisampleAvailable != 0 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_MULTISAMPLE;
	}
	if ( pProbe->nPresentationAvailable != 0 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_PRESENT;
	}
	if ( bFeatureLevel10 || bFeatureLevel11 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER;
	}
	if ( bFeatureLevel11 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_TESSELLATION;
	}
	if ( bFeatureLevel11 || pProbe->nComputeShader4xAvailable != 0 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE;
		if ( bFeatureLevel10 ) {
			profile.nMaximumUavSlots = 1;
		}
	}
	if ( pProbe->nIndirectDrawAvailable != 0 ) {
		profile.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	}

	memcpy( pProfile, &profile, sizeof( profile ) );
	return SIR_GRAPHICS_SUCCESS;
}
