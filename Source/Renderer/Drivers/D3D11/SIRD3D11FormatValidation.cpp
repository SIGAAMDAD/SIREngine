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

#include <Renderer/Drivers/D3D11/SIRD3D11Formats.h>

static constexpr uint32_t SIR_D3D11_TEXTURE_USAGE_MASK = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE |
                                                         SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL |
                                                         SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;

uint32_t sirD3D11GetMaxSampleCount( const sirD3D11FormatSupport_t *pSupport ) noexcept
{
	if ( pSupport == nullptr || pSupport->nSize < sizeof( *pSupport ) ) {
		return 0;
	}
	for ( uint32_t nExponent = 5; nExponent > 0; --nExponent ) {
		if ( ( pSupport->nSampleCountMask & ( 1U << nExponent ) ) != 0 ) {
			return 1U << nExponent;
		}
	}
	return ( pSupport->nSampleCountMask & 1U ) != 0 ? 1U : 0U;
}

sirGraphicsResult_t sirD3D11ValidateTextureSupport( const sirGraphicsTextureDesc_t *pDesc,
                                                    const sirD3D11FormatSupport_t *pSupport ) noexcept
{
	if ( pDesc == nullptr || pSupport == nullptr || pSupport->nSize < sizeof( *pSupport ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->nWidth == 0 || pDesc->nHeight == 0 || pDesc->nDepth == 0 || pDesc->nArrayLayers == 0 || pDesc->nMipLevels == 0 ||
	     pDesc->nSampleCount == 0 || pDesc->nUsage == 0 || ( pDesc->nUsage & ~SIR_D3D11_TEXTURE_USAGE_MASK ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eDimension < SIR_GRAPHICS_TEXTURE_1D || pDesc->eDimension > SIR_GRAPHICS_TEXTURE_CUBE || pDesc->nSampleCount > 32 ||
	     ( pDesc->nSampleCount & ( pDesc->nSampleCount - 1U ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eFormat == SIR_GRAPHICS_FORMAT_UNKNOWN || pDesc->eFormat != pSupport->eFormat ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pDesc->nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 &&
	     ( pDesc->nUsage & ( SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->nSampleCount != 1 && ( pDesc->eDimension != SIR_GRAPHICS_TEXTURE_2D || pDesc->nMipLevels != 1 ||
	                                   ( pDesc->nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
		if ( pDesc->nHeight != 1 || pDesc->nDepth != 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_3D ) {
		if ( pDesc->nArrayLayers != 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else {
		if ( pDesc->nDepth != 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pDesc->eDimension == SIR_GRAPHICS_TEXTURE_CUBE && ( pDesc->nWidth != pDesc->nHeight || pDesc->nArrayLayers % 6U != 0 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	uint32_t nLargestDimension = pDesc->nWidth > pDesc->nHeight ? pDesc->nWidth : pDesc->nHeight;
	if ( pDesc->nDepth > nLargestDimension ) {
		nLargestDimension = pDesc->nDepth;
	}
	uint32_t nAvailableMips = 0;
	for ( ; nLargestDimension != 0; nLargestDimension >>= 1 ) {
		++nAvailableMips;
	}
	if ( pDesc->nMipLevels > nAvailableMips ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	const uint32_t nDimensionBit = 1U << ( ( uint32_t )pDesc->eDimension - 1U );
	if ( ( pSupport->nDimensionMask & nDimensionBit ) == 0 || ( pDesc->nUsage & ~pSupport->nSupportedUsages ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nSampleExponent = 0;
	for ( uint32_t nSamples = pDesc->nSampleCount; nSamples > 1; nSamples >>= 1 ) {
		++nSampleExponent;
	}
	if ( ( pSupport->nSampleCountMask & ( 1U << nSampleExponent ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}
