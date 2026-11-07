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
#include <stdio.h>

#define SIR_TEST( expression )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #expression );                                                                \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	sirVirtualTextureDemandMipHistogram_t histogram = {};
	histogram.szMipWeight[2] = 100;
	histogram.szMipWeight[6] = 100;
	sirVirtualTextureDemandMipReference_t reference = { 4096, 256, 90000 };
	sirVirtualTextureDemandMipProjection_t projection = { 4096, 256, 90.0f, 16 };
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == 2 );
	projection.nTextureSize = 8192;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == 3 );
	projection.nTextureSize = 2048;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == 1 );
	projection.nTextureSize = 16;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == 0 );
	projection.nTextureSize = 4096;
	projection.nMipCount = 2;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == 1 );
	projection.nMipCount = 16;
	projection.fFovDegrees = NAN;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	projection.fFovDegrees = 1.0f;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	projection.fFovDegrees = 179.0f;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	projection.fFovDegrees = 90.0f;
	projection.nTextureSize = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	projection.nTextureSize = 4096;
	projection.nViewHeight = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	projection.nViewHeight = 256;
	projection.nMipCount = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	projection.nMipCount = 16;
	reference.nNominalTextureSize = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	reference.nNominalTextureSize = 4096;
	reference.nNominalRenderSize = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	reference.nNominalRenderSize = 256;
	reference.nFovMilliDegrees = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	reference.nFovMilliDegrees = 270000;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	reference.nFovMilliDegrees = 90000;
	histogram.szMipWeight[2] = histogram.szMipWeight[6] = 0;
	SIR_TEST( sirVTSelectDemandMip( histogram, reference, projection ) == UINT16_MAX );
	return 0;
}
