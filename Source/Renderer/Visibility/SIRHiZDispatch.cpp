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

static bool sirHiZIsCanonicalLayout( const sirHiZLayout_t& layout ) noexcept
{
	sirHiZLayout_t expected {};
	if ( sirHiZCalculateLayout( layout.nWidth, layout.nHeight, &expected ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( layout.nMipCount != expected.nMipCount || layout.nTexelCount != expected.nTexelCount ) {
		return false;
	}
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		const sirHiZMip_t& actual = layout.szMips[iMip];
		const sirHiZMip_t& reference = expected.szMips[iMip];
		if ( actual.nOffset != reference.nOffset || actual.nWidth != reference.nWidth || actual.nHeight != reference.nHeight ) {
			return false;
		}
	}
	return true;
}

SIRGraphicsResult sirHiZPlanDispatches( const sirHiZLayout_t& layout, const sirHiZDepthInput_t& depth,
                                          sirHiZDispatchPlan_t *pPlan ) noexcept
{
	if ( pPlan == nullptr || !sirHiZIsCanonicalLayout( layout ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsTextureDesc_t& source = depth.texture;
	if ( source.nWidth == 0 || source.nHeight == 0 || source.nDepth != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( source.nArrayLayers != 1 || source.nMipLevels == 0 || source.eDimension != SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_2D ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( source.eFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM && source.eFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT &&
	     source.eFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( source.nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED ) == 0 || ( source.nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( depth.eConvention != SIRVisibilityDepthConvention::SIR_VISIBILITY_REVERSE_Z ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( source.nSampleCount != 1 ) {
		return source.nSampleCount == 0 ? SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT : SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsRect_t& rect = depth.viewport;
	if ( rect.nLeft < 0 || rect.nTop < 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( rect.nLeft >= rect.nRight || rect.nTop >= rect.nBottom ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nRectWidth = ( uint64_t )( static_cast<int64_t>(rect.nRight) - rect.nLeft );
	const uint64_t nRectHeight = ( uint64_t )( static_cast<int64_t>(rect.nBottom) - rect.nTop );
	if ( static_cast<uint64_t>(rect.nRight) > source.nWidth || static_cast<uint64_t>(rect.nBottom) > source.nHeight ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nRectWidth != layout.nWidth || nRectHeight != layout.nHeight ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirHiZDispatchPlan_t plan {};
	plan.pyramid = { layout.nWidth,
	                 layout.nHeight,
	                 1,
	                 1,
	                 layout.nMipCount,
	                 1,
	                 SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE,
	                 SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_2D,
	                 SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT,
	                 SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	plan.nPassCount = layout.nMipCount;
	constexpr uint32_t WORKGROUP_SIZE = 8;
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		sirHiZDispatch_t& pass = plan.szPasses[iMip];
		const sirHiZMip_t& destination = layout.szMips[iMip];
		pass.bCopyDepth = iMip == 0;
		pass.nSourceMip = pass.bCopyDepth ? 0 : iMip - 1;
		pass.nDestinationMip = iMip;
		pass.parameters.nSourceWidth = pass.bCopyDepth ? source.nWidth : layout.szMips[iMip - 1].nWidth;
		pass.parameters.nSourceHeight = pass.bCopyDepth ? source.nHeight : layout.szMips[iMip - 1].nHeight;
		pass.parameters.nDestinationWidth = destination.nWidth;
		pass.parameters.nDestinationHeight = destination.nHeight;
		pass.parameters.nSourceOriginX = pass.bCopyDepth ? static_cast<uint32_t>(rect.nLeft) : 0;
		pass.parameters.nSourceOriginY = pass.bCopyDepth ? static_cast<uint32_t>(rect.nTop) : 0;
		pass.nWorkgroupsX = destination.nWidth / WORKGROUP_SIZE + ( destination.nWidth % WORKGROUP_SIZE != 0 );
		pass.nWorkgroupsY = destination.nHeight / WORKGROUP_SIZE + ( destination.nHeight % WORKGROUP_SIZE != 0 );
	}
	*pPlan = plan;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
