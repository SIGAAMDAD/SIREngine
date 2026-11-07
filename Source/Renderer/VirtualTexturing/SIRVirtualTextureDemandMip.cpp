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

#include <Renderer/VirtualTexturing/SIRVirtualTextureDemandMip.h>

#include <math.h>

// Native projection of terrain_3d_vt_spatial_demand.cpp's mip percentile.
// The nominal base mip is a histogram metadata field, not an added offset.
uint16_t sirVTSelectDemandMip( const sirVirtualTextureDemandMipHistogram_t& histogram,
                               const sirVirtualTextureDemandMipReference_t& reference,
                               const sirVirtualTextureDemandMipProjection_t& projection ) noexcept
{
	if ( projection.nTextureSize == 0 || projection.nViewHeight == 0 || projection.nMipCount == 0 || !isfinite( projection.fFovDegrees ) ||
	     projection.fFovDegrees <= 1.0f || projection.fFovDegrees >= 179.0f || reference.nNominalTextureSize == 0 ||
	     reference.nNominalRenderSize == 0 || reference.nFovMilliDegrees == 0 ) {
		return UINT16_MAX;
	}
	uint32_t nTotal = 0;
	for ( uint32_t i = 0; i < 16; ++i ) {
		nTotal += histogram.szMipWeight[i];
	}
	if ( nTotal == 0 ) {
		return UINT16_MAX;
	}
	const uint32_t nThreshold = ( nTotal + 9U ) / 10U;
	uint32_t nSeen = 0;
	uint32_t nMip = 15;
	for ( uint32_t i = 0; i < 16; ++i ) {
		nSeen += histogram.szMipWeight[i];
		if ( nSeen >= nThreshold ) {
			nMip = i;
			break;
		}
	}
	const double fReferenceTan = tan( ( double )reference.nFovMilliDegrees * 3.141592653589793 / 360000.0 );
	const double fRuntimeTan = tan( ( double )projection.fFovDegrees * 3.141592653589793 / 360.0 );
	const double fRatio = ( ( double )projection.nTextureSize / reference.nNominalTextureSize ) *
	                      ( ( double )reference.nNominalRenderSize / projection.nViewHeight ) * ( fRuntimeTan / fReferenceTan );
	if ( !( fRatio > 0.0 ) || !isfinite( fRatio ) ) {
		return UINT16_MAX;
	}
	const double fAdjusted = ( double )nMip + log2( fRatio );
	const double fClamped = fAdjusted < 0.0                         ? 0.0
	                        : fAdjusted > projection.nMipCount - 1U ? ( double )( projection.nMipCount - 1U )
	                                                                : fAdjusted;
	return ( uint16_t )floor( fClamped + 0.5 );
}
