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

#include <Renderer/Drivers/D3D11/SIRD3D11TextureValidation.h>

#include <stdint.h>

static uint32_t sirD3D11PixelBlockBytes( SIRGraphicsFormat eFormat, bool *pCompressed ) noexcept
{
	*pCompressed = false;
	switch ( eFormat ) {
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R8_UNORM:
		return 1;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R16_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM:
		return 2;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_UINT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return 4;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return 8;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return 16;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC1_UNORM:
		*pCompressed = true;
		return 8;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC3_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC5_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC7_UNORM:
		*pCompressed = true;
		return 16;
	default:
		return 0;
	}
}

SIRGraphicsResult sirD3D11PlanTexture( const sirGraphicsTextureDesc_t *pDesc, const sirD3D11FormatSupport_t *pSupport,
                                         uint64_t nCommittedBytes, uint64_t nBudgetBytes, sirD3D11TexturePlan_t *pPlan ) noexcept
{
	if ( pPlan == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pPlan = {};
	const SIRGraphicsResult eValid = sirD3D11ValidateTextureSupport( pDesc, pSupport );
	if ( eValid != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( pDesc->eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
		// No hidden dynamic/staging allocation or incomplete mapping semantics.
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bDepth = pDesc->eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM || pDesc->eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ||
	                    pDesc->eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT;
	const bool bDepthUsage = ( pDesc->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0;
	if ( bDepth != bDepthUsage || ( bDepth && ( pDesc->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	bool bCompressed = false;
	const uint32_t nBlockBytes = sirD3D11PixelBlockBytes( pDesc->eFormat, &bCompressed );
	if ( nBlockBytes == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( bCompressed && ( pDesc->nUsage &
	                      ( SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( bCompressed || pDesc->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_CUBE ) && pDesc->nSampleCount != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nTotal = 0;
	for ( uint32_t nMip = 0; nMip < pDesc->nMipLevels; ++nMip ) {
		const uint64_t nWidth = pDesc->nWidth >> nMip ? pDesc->nWidth >> nMip : 1;
		const uint64_t nHeight = pDesc->nHeight >> nMip ? pDesc->nHeight >> nMip : 1;
		const uint64_t nDepth = pDesc->nDepth >> nMip ? pDesc->nDepth >> nMip : 1;
		const uint64_t nBlocksX = bCompressed ? ( nWidth + 3 ) / 4 : nWidth;
		const uint64_t nBlocksY = bCompressed ? ( nHeight + 3 ) / 4 : nHeight;
		if ( nBlocksX > UINT64_MAX / nBlocksY || nBlocksX * nBlocksY > UINT64_MAX / nDepth ) {
			return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
		}
		const uint64_t nCount = nBlocksX * nBlocksY * nDepth;
		if ( nCount > UINT64_MAX / nBlockBytes / pDesc->nArrayLayers / pDesc->nSampleCount ) {
			return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
		}
		const uint64_t nBytes = nCount * nBlockBytes * pDesc->nArrayLayers * pDesc->nSampleCount;
		if ( nBytes > UINT64_MAX - nTotal ) {
			return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
		}
		nTotal += nBytes;
	}
	if ( nTotal > UINT64_MAX - nCommittedBytes ||
	     ( nBudgetBytes != 0 && ( nCommittedBytes > nBudgetBytes || nTotal > nBudgetBytes - nCommittedBytes ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	pPlan->nLogicalBytes = nTotal;
	pPlan->nNativeBindUsage = pDesc->nUsage;
	pPlan->nArrayLayers = pDesc->nArrayLayers;
	pPlan->nMipLevels = pDesc->nMipLevels;
	pPlan->nSampleCount = pDesc->nSampleCount;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirD3D11ValidateTextureView( const sirGraphicsViewDesc_t *pView, const sirGraphicsTextureDesc_t *pTexture ) noexcept
{
	if ( pView == nullptr || pTexture == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pView->nUsage != SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED && pView->nUsage != SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT &&
	     pView->nUsage != SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL && pView->nUsage != SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pTexture->nUsage & pView->nUsage ) == 0 || pView->eFormat != pTexture->eFormat ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pView->nBufferOffsetBytes != 0 || pView->nBufferSizeBytes != 0 || pView->nMipLevels == 0 || pView->nLayers == 0 ||
	     pView->nBaseMip >= pTexture->nMipLevels || pView->nMipLevels > pTexture->nMipLevels - pView->nBaseMip ||
	     pView->nBaseLayer >= pTexture->nArrayLayers || pView->nLayers > pTexture->nArrayLayers - pView->nBaseLayer ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pView->nUsage != SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED && pView->nMipLevels != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pTexture->nSampleCount != 1 &&
	     ( pView->nBaseMip != 0 || pView->nMipLevels != 1 || pView->nUsage == SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pTexture->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ) {
		// The shared layout has no per-mip 3D W-slice view contract yet.
		if ( pView->nBaseLayer != 0 || pView->nLayers != 1 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
	} else if ( pTexture->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_CUBE && pView->nUsage == SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED ) {
		if ( pView->nBaseLayer % 6 != 0 || pView->nLayers % 6 != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( ( pView->nUsage == SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL || pView->nUsage == SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ||
	       pView->nUsage == SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) &&
	     pTexture->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
