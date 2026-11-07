/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureResourcePlan.h>

#include <limits.h>

static bool sirVTAddBytes( uint64_t nValue, uint64_t *pTotal ) noexcept
{
	if ( pTotal == nullptr || nValue > UINT64_MAX - *pTotal ) {
		return false;
	}
	*pTotal += nValue;
	return true;
}

static bool sirVTMultiplyBytes( uint64_t nLeft, uint64_t nRight, uint64_t *pResult ) noexcept
{
	if ( pResult == nullptr || ( nLeft != 0 && nRight > UINT64_MAX / nLeft ) ) {
		return false;
	}
	*pResult = nLeft * nRight;
	return true;
}

static bool sirVTComputeFamilyAtlasBytes( sirVirtualTexturePhysicalFamily_t eFamily, uint32_t nWidth, uint32_t nHeight,
                                          uint64_t *pBytes ) noexcept
{
	if ( nWidth == 0 || nHeight == 0 || pBytes == nullptr ) {
		return false;
	}
	uint64_t nBlocksX = nWidth;
	uint64_t nBlocksY = nHeight;
	uint64_t nBlockBytes = 1;
	switch ( eFamily ) {
	case SIR_VT_PHYSICAL_BC1:
		nBlocksX = ( nBlocksX + 3 ) / 4;
		nBlocksY = ( nBlocksY + 3 ) / 4;
		nBlockBytes = 8;
		break;
	case SIR_VT_PHYSICAL_BC7:
	case SIR_VT_PHYSICAL_BC5:
		nBlocksX = ( nBlocksX + 3 ) / 4;
		nBlocksY = ( nBlocksY + 3 ) / 4;
		nBlockBytes = 16;
		break;
	case SIR_VT_PHYSICAL_R8:
		break;
	case SIR_VT_PHYSICAL_R16:
		nBlockBytes = 2;
		break;
	default:
		return false;
	}
	uint64_t nBlocks = 0;
	return sirVTMultiplyBytes( nBlocksX, nBlocksY, &nBlocks ) && sirVTMultiplyBytes( nBlocks, nBlockBytes, pBytes );
}

bool sirVTComputeResourcePlan( const sirVirtualTextureResourceLimits_t& limits, sirVirtualTextureResourcePlan_t *pPlan ) noexcept
{
	if ( pPlan == nullptr || limits.nVirtualPages == 0 || limits.nAllocations == 0 || limits.nPhysicalPageSets == 0 ||
	     limits.nMaximumArrayLayers == 0 || limits.nPhysicalPageSets > limits.nMaximumArrayLayers || limits.nTailAtlasWidth == 0 ||
	     limits.nTailAtlasHeight == 0 || limits.nTailAtlasWidth % 4 != 0 || limits.nTailAtlasHeight % 4 != 0 ||
	     limits.nRuntimeAllocationBytes == 0 || limits.nRuntimePageTableBytes == 0 ) {
		return false;
	}
	sirVirtualTextureResourcePlan_t candidate = {};
	candidate.nPhysicalPageSets = limits.nPhysicalPageSets;
	candidate.nArrayBanks = SIR_VT_PHYSICAL_COUNT;
	if ( !sirVTMultiplyBytes( limits.nAllocations, limits.nRuntimeAllocationBytes, &candidate.nAllocationBytes ) ||
	     !sirVTMultiplyBytes( limits.nVirtualPages, limits.nRuntimePageTableBytes, &candidate.nPageTableBytes ) ||
	     !sirVTAddBytes( candidate.nAllocationBytes, &candidate.nTotalBufferBytes ) ||
	     !sirVTAddBytes( candidate.nPageTableBytes, &candidate.nTotalBufferBytes ) ) {
		return false;
	}
	for ( uint32_t iFamily = 0; iFamily < SIR_VT_PHYSICAL_COUNT; ++iFamily ) {
		const sirVirtualTexturePhysicalFamily_t eFamily = ( sirVirtualTexturePhysicalFamily_t )iFamily;
		const uint64_t nTileBytes = sirVTPackPhysicalTileBytes( eFamily );
		if ( nTileBytes == 0 || !sirVTMultiplyBytes( nTileBytes, limits.nPhysicalPageSets, &candidate.szPhysicalBytes[iFamily] ) ||
		     !sirVTComputeFamilyAtlasBytes( eFamily, limits.nTailAtlasWidth, limits.nTailAtlasHeight, &candidate.szTailBytes[iFamily] ) ||
		     !sirVTAddBytes( candidate.szPhysicalBytes[iFamily], &candidate.nTotalTextureBytes ) ||
		     !sirVTAddBytes( candidate.szTailBytes[iFamily], &candidate.nTotalTextureBytes ) ) {
			return false;
		}
	}
	candidate.nTotalBytes = candidate.nTotalBufferBytes;
	if ( !sirVTAddBytes( candidate.nTotalTextureBytes, &candidate.nTotalBytes ) ) {
		return false;
	}
	*pPlan = candidate;
	return true;
}

bool sirVTFitResourcePlanToBudget( const sirVirtualTextureResourceLimits_t& limits, uint64_t nBudgetBytes,
                                   sirVirtualTextureResourceLimits_t *pFittedLimits, sirVirtualTextureResourcePlan_t *pPlan ) noexcept
{
	if ( pFittedLimits == nullptr || pPlan == nullptr ) {
		return false;
	}
	sirVirtualTextureResourcePlan_t candidate = {};
	if ( !sirVTComputeResourcePlan( limits, &candidate ) ) {
		return false;
	}
	if ( candidate.nTotalBytes <= nBudgetBytes ) {
		*pFittedLimits = limits;
		*pPlan = candidate;
		return true;
	}
	sirVirtualTextureResourceLimits_t fitted = limits;
	fitted.nPhysicalPageSets = 1;
	if ( !sirVTComputeResourcePlan( fitted, &candidate ) || candidate.nTotalBytes > nBudgetBytes ) {
		return false;
	}
	uint32_t nLower = 1;
	uint32_t nUpper = limits.nPhysicalPageSets;
	while ( nLower < nUpper ) {
		const uint32_t nMiddle = nLower + ( nUpper - nLower ) / 2 + ( ( nUpper - nLower ) & 1U );
		fitted.nPhysicalPageSets = nMiddle;
		if ( !sirVTComputeResourcePlan( fitted, &candidate ) ) {
			return false;
		}
		if ( candidate.nTotalBytes <= nBudgetBytes ) {
			nLower = nMiddle;
		} else {
			nUpper = nMiddle - 1;
		}
	}
	fitted.nPhysicalPageSets = nLower;
	if ( !sirVTComputeResourcePlan( fitted, &candidate ) ) {
		return false;
	}
	*pFittedLimits = fitted;
	*pPlan = candidate;
	return true;
}
