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

#include <Terrain/Data/SIRTerrainHeightfieldView.h>

#include <math.h>
#include <stdio.h>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Heightfield view assertion failed: %s at line %d\n", #expression, __LINE__ );                                \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	sirTerrainRegionDescriptor_t descriptor = { -2, -3, 2, 0.0f, 6.0f, 4 };
	float szHeights[4] = { 0.0f, 2.0f, 4.0f, 6.0f };
	std::vector<uint8_t> encoded( SIRTerrainHeightfieldCodec::getEncodedByteCount( 2 ) );
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szHeights, 4 }, { encoded.data(), encoded.size() } ) ==
	       SIR_TERRAIN_CODEC_SUCCESS );
	sirTerrainR16View_t view = {};
	CHECK( SIRTerrainHeightfieldView::open( { encoded.data(), encoded.size() }, view, 4 ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( view.descriptor.iRegionX == -2 && view.descriptor.iRegionZ == -3 );
	CHECK( view.nSampleBytes == 8 );
	CHECK( view.pSamples == encoded.data() + SIRTerrainHeightfieldCodec::HEADER_BYTES );
	float fValue = -99.0f;
	CHECK( SIRTerrainHeightfieldView::read( view, 0, 0, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS && fValue == 0.0f );
	CHECK( SIRTerrainHeightfieldView::read( view, 1, 0, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS && fValue == 2.0f );
	CHECK( SIRTerrainHeightfieldView::read( view, 0, 1, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS && fValue == 4.0f );
	CHECK( SIRTerrainHeightfieldView::read( view, 1, 1, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS && fValue == 6.0f );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( view, 0.5, 0.5, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS );
	CHECK( fabsf( fValue - 3.0f ) < 0.0001f );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( view, 0.25, 0.75, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS );
	CHECK( fabsf( fValue - 3.5f ) < 0.0001f );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( view, 1.0, 1.0, fValue ) == SIR_TERRAIN_SAMPLE_SUCCESS && fValue == 6.0f );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( view, -0.01, 0.0, fValue ) == SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( view, 0.0, 1.01, fValue ) == SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( view, NAN, 0.0, fValue ) == SIR_TERRAIN_SAMPLE_INVALID_ARGUMENT );
	CHECK( SIRTerrainHeightfieldView::read( view, 2, 0, fValue ) == SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS );
	CHECK( SIRTerrainHeightfieldView::read( view, 0, 2, fValue ) == SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS );
	CHECK( fValue == 6.0f );
	CHECK( SIRTerrainHeightfieldView::open( { encoded.data(), encoded.size() }, view, 5 ) == SIR_TERRAIN_CODEC_STALE_REVISION );
	CHECK( view.descriptor.nRevision == 4 );
	encoded[67] ^= 1;
	CHECK( SIRTerrainHeightfieldView::open( { encoded.data(), encoded.size() }, view ) == SIR_TERRAIN_CODEC_BAD_CHECKSUM );
	encoded[67] ^= 1;
	sirTerrainR16View_t invalid = view;
	invalid.nSampleBytes = 2;
	CHECK( SIRTerrainHeightfieldView::read( invalid, 0, 0, fValue ) == SIR_TERRAIN_SAMPLE_INVALID_VIEW );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( invalid, 0.0, 0.0, fValue ) == SIR_TERRAIN_SAMPLE_INVALID_VIEW );
	invalid.pSamples = nullptr;
	CHECK( SIRTerrainHeightfieldView::read( invalid, 0, 0, fValue ) == SIR_TERRAIN_SAMPLE_INVALID_VIEW );
	CHECK( SIRTerrainHeightfieldView::sampleBilinear( invalid, 0.0, 0.0, fValue ) == SIR_TERRAIN_SAMPLE_INVALID_VIEW );
	return 0;
}
