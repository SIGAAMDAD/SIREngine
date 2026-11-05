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

#include <Renderer/Visibility/SIRHiZDispatch.h>
#include <stdio.h>

static sirHiZDepthInput_t sirHiZTestSource( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	sirHiZDepthInput_t depth = {};
	depth.texture = { nWidth,
	                  nHeight,
	                  1,
	                  1,
	                  1,
	                  1,
	                  SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_DEPTH_STENCIL,
	                  SIR_GRAPHICS_TEXTURE_2D,
	                  SIR_GRAPHICS_FORMAT_D32_FLOAT,
	                  SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	depth.viewport = { 0, 0, ( int32_t )nWidth, ( int32_t )nHeight };
	depth.eConvention = SIR_VISIBILITY_REVERSE_Z;
	return depth;
}

static bool sirHiZTestPlan( void ) noexcept
{
	constexpr uint32_t szWidths[] = { 1, 1, 7, 13, 1920, 2560, 3840 };
	constexpr uint32_t szHeights[] = { 1, 17, 1, 9, 1080, 1440, 2160 };
	for ( uint32_t iCase = 0; iCase < sizeof( szWidths ) / sizeof( szWidths[0] ); ++iCase ) {
		sirHiZLayout_t layout = {};
		if ( sirHiZCalculateLayout( szWidths[iCase], szHeights[iCase], &layout ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
		const sirHiZDepthInput_t depth = sirHiZTestSource( szWidths[iCase], szHeights[iCase] );
		sirHiZDispatchPlan_t plan = {};
		if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nPassCount != layout.nMipCount ) {
			return false;
		}
		if ( plan.pyramid.eFormat != SIR_GRAPHICS_FORMAT_R32_FLOAT || plan.pyramid.nMipLevels != layout.nMipCount ||
		     plan.pyramid.nSampleCount != 1 || plan.pyramid.nUsage != ( SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE ) ) {
			return false;
		}
		for ( uint32_t iMip = 0; iMip < plan.nPassCount; ++iMip ) {
			const sirHiZDispatch_t *pPass = &plan.szPasses[iMip];
			if ( pPass->nDestinationMip != iMip || pPass->nSourceMip != ( iMip == 0 ? 0 : iMip - 1 ) ||
			     pPass->bCopyDepth != ( iMip == 0 ) ) {
				return false;
			}
			if ( pPass->parameters.nDestinationWidth != layout.szMips[iMip].nWidth ||
			     pPass->parameters.nDestinationHeight != layout.szMips[iMip].nHeight ) {
				return false;
			}
			if ( pPass->nWorkgroupsX != layout.szMips[iMip].nWidth / 8 + ( layout.szMips[iMip].nWidth % 8 != 0 ) ||
			     pPass->nWorkgroupsY != layout.szMips[iMip].nHeight / 8 + ( layout.szMips[iMip].nHeight % 8 != 0 ) ) {
				return false;
			}
			if ( iMip != 0 && ( pPass->parameters.nSourceWidth != layout.szMips[iMip - 1].nWidth ||
			                    pPass->parameters.nSourceHeight != layout.szMips[iMip - 1].nHeight ||
			                    pPass->parameters.nSourceOriginX != 0 || pPass->parameters.nSourceOriginY != 0 ) ) {
				return false;
			}
		}
	}
	return true;
}

static bool sirHiZTestInvalid( void ) noexcept
{
	sirHiZLayout_t layout = {};
	if ( sirHiZCalculateLayout( 13, 9, &layout ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirHiZDepthInput_t depth = sirHiZTestSource( 128, 64 );
	depth.viewport = { 27, 11, 40, 20 };
	sirHiZDispatchPlan_t plan = {};
	if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( plan.szPasses[0].parameters.nSourceOriginX != 27 || plan.szPasses[0].parameters.nSourceOriginY != 11 ||
	     plan.szPasses[0].parameters.nSourceWidth != 128 || plan.szPasses[1].parameters.nSourceOriginX != 0 ) {
		return false;
	}
	const sirHiZDispatchPlan_t expected = plan;
	if ( sirHiZPlanDispatches( layout, depth, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	layout.szMips[1].nWidth++;
	if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	layout.szMips[1].nWidth--;
	depth.texture.nSampleCount = 4;
	if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	depth.texture.nSampleCount = 1;
	depth.eConvention = SIR_VISIBILITY_FORWARD_Z;
	if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	depth.eConvention = SIR_VISIBILITY_REVERSE_Z;
	depth.texture.eFormat = SIR_GRAPHICS_FORMAT_R32_FLOAT;
	if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	depth.texture.eFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	depth.viewport.nRight++;
	if ( sirHiZPlanDispatches( layout, depth, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( memcmp( &plan, &expected, sizeof( plan ) ) != 0 ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirHiZTestPlan() || !sirHiZTestInvalid() ) {
		puts( "FAIL: Hi-Z dispatch planning" );
		return 1;
	}
	puts( "PASS: Hi-Z dispatch planning" );
	return 0;
}
