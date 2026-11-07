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

#include <Renderer/VirtualTexturing/SIRVirtualTextureTileCooker.h>

#include <string.h>

// Ported from Terrain3D src/authoring/vt/build/terrain_3d_vt_page_builder.cpp.
// Keep the donor's floor-half mip dimensions and exact gutter addressing.
uint32_t sirVTCountCookedPages( uint32_t nDimension ) noexcept
{
	return nDimension == 0 ? 0 : 1U + ( nDimension - 1U ) / SIR_VT_PAGE_INTERIOR;
}

bool sirVTMeasureCookedMips( uint32_t nWidth, uint32_t nHeight, uint32_t nComponents, sirVirtualTextureCookMipLayout_t *pLayout ) noexcept
{
	if ( pLayout == nullptr ) {
		return false;
	}
	*pLayout = {};
	if ( nWidth == 0 || nHeight == 0 || nComponents == 0 || nComponents > 4 ) {
		return false;
	}
	uint64_t nOffset = 0;
	while ( true ) {
		if ( pLayout->nMipCount == SIR_VT_COOK_MAX_MIPS ) {
			return true;
		}
		const uint64_t nPixels = ( uint64_t )nWidth * nHeight;
		if ( nPixels > UINT64_MAX / nComponents ) {
			*pLayout = {};
			return false;
		}
		const uint64_t nBytes = nPixels * nComponents;
		if ( nOffset > UINT64_MAX - nBytes ) {
			*pLayout = {};
			return false;
		}
		sirVirtualTextureCookMipLevel_t& level = pLayout->szLevels[pLayout->nMipCount++];
		level.nWidth = nWidth;
		level.nHeight = nHeight;
		level.nOffset = nOffset;
		level.nBytes = nBytes;
		nOffset += nBytes;
		pLayout->nComponents = nComponents;
		pLayout->nTotalBytes = nOffset;
		if ( nWidth == 1 && nHeight == 1 ) {
			return true;
		}
		nWidth = nWidth > 1 ? nWidth >> 1U : 1U;
		nHeight = nHeight > 1 ? nHeight >> 1U : 1U;
	}
}

bool sirVTBuildCookedMips( const sirVirtualTextureCookMipLayout_t& layout, uint8_t *pImageBytes, uint64_t nImageBytes ) noexcept
{
	if ( pImageBytes == nullptr || layout.nMipCount == 0 || layout.nMipCount > SIR_VT_COOK_MAX_MIPS || layout.nComponents == 0 ||
	     layout.nComponents > 4 || nImageBytes < layout.nTotalBytes || layout.nTotalBytes > SIZE_MAX ) {
		return false;
	}
	sirVirtualTextureCookMipLayout_t expected = {};
	if ( !sirVTMeasureCookedMips( layout.szLevels[0].nWidth, layout.szLevels[0].nHeight, layout.nComponents, &expected ) ||
	     memcmp( &layout, &expected, sizeof( layout ) ) != 0 ) {
		return false;
	}
	for ( uint32_t iMip = 1; iMip < layout.nMipCount; ++iMip ) {
		const sirVirtualTextureCookMipLevel_t& source = layout.szLevels[iMip - 1U];
		const sirVirtualTextureCookMipLevel_t& destination = layout.szLevels[iMip];
		const uint8_t *pSource = pImageBytes + source.nOffset;
		uint8_t *pOutput = pImageBytes + destination.nOffset;
		for ( uint32_t y = 0; y < destination.nHeight; ++y ) {
			const uint32_t nY0 = y * 2U;
			const uint32_t nY1 = nY0 + 1U < source.nHeight ? nY0 + 1U : nY0;
			for ( uint32_t x = 0; x < destination.nWidth; ++x ) {
				const uint32_t nX0 = x * 2U;
				const uint32_t nX1 = nX0 + 1U < source.nWidth ? nX0 + 1U : nX0;
				for ( uint32_t c = 0; c < layout.nComponents; ++c ) {
					const uint64_t nI00 = ( ( uint64_t )nY0 * source.nWidth + nX0 ) * layout.nComponents + c;
					const uint64_t nI10 = ( ( uint64_t )nY0 * source.nWidth + nX1 ) * layout.nComponents + c;
					const uint64_t nI01 = ( ( uint64_t )nY1 * source.nWidth + nX0 ) * layout.nComponents + c;
					const uint64_t nI11 = ( ( uint64_t )nY1 * source.nWidth + nX1 ) * layout.nComponents + c;
					const uint32_t nSum = ( uint32_t )pSource[nI00] + pSource[nI10] + pSource[nI01] + pSource[nI11];
					pOutput[( ( uint64_t )y * destination.nWidth + x ) * layout.nComponents + c] = ( uint8_t )( ( nSum + 2U ) >> 2U );
				}
			}
		}
	}
	return true;
}

static uint32_t sirVTWrapCookedCoordinate( int64_t nCoordinate, uint32_t nExtent, sirVirtualTextureCookWrap_t eMode ) noexcept
{
	if ( eMode == SIR_VT_COOK_WRAP_CLAMP ) {
		if ( nCoordinate < 0 ) {
			return 0;
		}
		if ( nCoordinate >= ( int64_t )nExtent ) {
			return nExtent - 1U;
		}
		return ( uint32_t )nCoordinate;
	}
	int64_t nWrapped = nCoordinate % nExtent;
	if ( nWrapped < 0 ) {
		nWrapped += nExtent;
	}
	return ( uint32_t )nWrapped;
}

bool sirVTBuildCookedTile( const sirVirtualTextureCookTileRequest_t& request, uint8_t *pDestination, uint64_t nDestinationBytes ) noexcept
{
	if ( request.pMipPixels == nullptr || pDestination == nullptr || request.nMipWidth == 0 || request.nMipHeight == 0 ||
	     request.nComponents == 0 || request.nComponents > 4 || request.eWrapMode > SIR_VT_COOK_WRAP_CLAMP ) {
		return false;
	}
	const uint64_t nPixels = ( uint64_t )request.nMipWidth * request.nMipHeight;
	if ( nPixels > UINT64_MAX / request.nComponents ) {
		return false;
	}
	const uint64_t nSourceBytes = nPixels * request.nComponents;
	const uint64_t nTileBytes = ( uint64_t )SIR_VT_PHYSICAL_TILE * SIR_VT_PHYSICAL_TILE * request.nComponents;
	if ( nSourceBytes > request.nMipBytes || nTileBytes > nDestinationBytes || nSourceBytes > SIZE_MAX || nTileBytes > SIZE_MAX ) {
		return false;
	}
	const uintptr_t nSource = ( uintptr_t )request.pMipPixels;
	const uintptr_t nTarget = ( uintptr_t )pDestination;
	if ( nSourceBytes > UINTPTR_MAX - nSource || nTileBytes > UINTPTR_MAX - nTarget ||
	     ( nSource < nTarget + nTileBytes && nTarget < nSource + nSourceBytes ) ) {
		return false;
	}
	for ( uint32_t y = 0; y < SIR_VT_PHYSICAL_TILE; ++y ) {
		const int64_t nY = ( int64_t )request.nPageY * SIR_VT_PAGE_INTERIOR + y - SIR_VT_PAGE_GUTTER;
		const uint32_t nSourceY = sirVTWrapCookedCoordinate( nY, request.nMipHeight, request.eWrapMode );
		for ( uint32_t x = 0; x < SIR_VT_PHYSICAL_TILE; ++x ) {
			const int64_t nX = ( int64_t )request.nPageX * SIR_VT_PAGE_INTERIOR + x - SIR_VT_PAGE_GUTTER;
			const uint32_t nSourceX = sirVTWrapCookedCoordinate( nX, request.nMipWidth, request.eWrapMode );
			const uint8_t *pSource = request.pMipPixels + ( ( uint64_t )nSourceY * request.nMipWidth + nSourceX ) * request.nComponents;
			uint8_t *pTarget = pDestination + ( ( uint64_t )y * SIR_VT_PHYSICAL_TILE + x ) * request.nComponents;
			memcpy( pTarget, pSource, request.nComponents );
		}
	}
	return true;
}
