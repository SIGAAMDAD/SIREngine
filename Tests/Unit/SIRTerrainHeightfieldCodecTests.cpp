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

#include <Terrain/Data/SIRTerrainHeightfieldCodec.h>

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Terrain codec assertion failed: %s at line %d\n", #expression, __LINE__ );                                   \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	sirTerrainRegionDescriptor_t descriptor = { -8, 13, 2, -100.0f, 900.0f, 42 };
	float szHeights[4] = { -100.0f, 0.0f, 900.0f, 1200.0f };
	const size_t nBytes = SIRTerrainHeightfieldCodec::getEncodedByteCount( 2 );
	CHECK( nBytes == 72 );
	CHECK( SIRTerrainHeightfieldCodec::getEncodedByteCount( 0 ) == 0 );
	CHECK( SIRTerrainHeightfieldCodec::getEncodedByteCount( 4097 ) == 0 );
	std::vector<uint8_t> output( nBytes, 0 );
	sirTerrainMutableByteSpan_t writable = { output.data(), output.size() };
	sirTerrainConstHeightSpan_t heights = { szHeights, 4 };
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, heights, writable ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( output[0] == 'S' && output[1] == 'T' && output[2] == 'H' && output[3] == 'F' );
	CHECK( output[12] == 248 && output[13] == 255 );
	CHECK( output[64] == 0 && output[65] == 0 && output[68] == 255 && output[69] == 255 );
	std::vector<uint8_t> identical( nBytes, 0 );
	sirTerrainMutableByteSpan_t secondOutput = { identical.data(), identical.size() };
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, heights, secondOutput ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( output == identical );

	sirTerrainRegionDescriptor_t readDescriptor = {};
	sirTerrainConstByteSpan_t encoded = { output.data(), output.size() };
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor, 42 ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( readDescriptor.iRegionX == -8 && readDescriptor.iRegionZ == 13 );
	CHECK( readDescriptor.nRevision == 42 && readDescriptor.nResolution == 2 );
	float szDecoded[4] = {};
	sirTerrainMutableHeightSpan_t decoded = { szDecoded, 4 };
	CHECK( SIRTerrainHeightfieldCodec::decode( encoded, readDescriptor, decoded, 42 ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( szDecoded[0] == -100.0f && szDecoded[2] == 900.0f && szDecoded[3] == 900.0f );
	CHECK( fabsf( szDecoded[1] ) < 0.01f );

	readDescriptor.iRegionX = 1234;
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor, 43 ) == SIR_TERRAIN_CODEC_STALE_REVISION );
	CHECK( readDescriptor.iRegionX == 1234 );
	sirTerrainMutableHeightSpan_t shortHeights = { szDecoded, 3 };
	CHECK( SIRTerrainHeightfieldCodec::decode( encoded, readDescriptor, shortHeights ) == SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL );
	CHECK( readDescriptor.iRegionX == 1234 );
	sirTerrainMutableByteSpan_t shortOutput = { output.data(), nBytes - 1 };
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, heights, shortOutput ) == SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL );
	sirTerrainConstByteSpan_t truncated = { output.data(), nBytes - 1 };
	CHECK( SIRTerrainHeightfieldCodec::inspect( truncated, readDescriptor ) == SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL );
	sirTerrainConstHeightSpan_t shortSource = { szHeights, 3 };
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, shortSource, writable ) == SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL );
	float fNaN = NAN;
	szHeights[1] = fNaN;
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, heights, writable ) == SIR_TERRAIN_CODEC_NONFINITE_HEIGHT );
	szHeights[1] = 0.0f;
	descriptor.fMinHeight = NAN;
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, heights, writable ) == SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	descriptor.fMinHeight = -100.0f;

	// Reject buffer aliases before mutating the encoded source, decoded output or descriptor.
	alignas( sirTerrainRegionDescriptor_t ) uint8_t szAliased[128] = {};
	memcpy( szAliased, szHeights, sizeof( szHeights ) );
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { ( const float * )szAliased, 4 }, { szAliased, nBytes } ) ==
	       SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( memcmp( szAliased, szHeights, sizeof( szHeights ) ) == 0 );
	memcpy( szAliased, output.data(), nBytes );
	CHECK( SIRTerrainHeightfieldCodec::decode( { szAliased, nBytes }, readDescriptor, { ( float * )( szAliased + 64 ), 4 } ) ==
	       SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( memcmp( szAliased, output.data(), nBytes ) == 0 );
	CHECK( SIRTerrainHeightfieldCodec::inspect( { szAliased, nBytes }, *( sirTerrainRegionDescriptor_t * )szAliased ) ==
	       SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( memcmp( szAliased, output.data(), nBytes ) == 0 );
	CHECK( SIRTerrainHeightfieldCodec::decode( { szAliased, nBytes }, *( sirTerrainRegionDescriptor_t * )szAliased, { szDecoded, 4 } ) ==
	       SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( memcmp( szAliased, output.data(), nBytes ) == 0 );
	memcpy( szAliased, &descriptor, sizeof( descriptor ) );
	CHECK( SIRTerrainHeightfieldCodec::encode( *( const sirTerrainRegionDescriptor_t * )szAliased, heights, { szAliased, nBytes } ) ==
	       SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( memcmp( szAliased, &descriptor, sizeof( descriptor ) ) == 0 );
	CHECK( SIRTerrainHeightfieldCodec::decode( encoded, *( sirTerrainRegionDescriptor_t * )szAliased, { ( float * )szAliased, 4 } ) ==
	       SIR_TERRAIN_CODEC_INVALID_ARGUMENT );

	// Exercise every short-circuit validation and non-mutating codec failure path.
	sirTerrainRegionDescriptor_t invalidDescriptor = descriptor;
	invalidDescriptor.nRevision = 0;
	CHECK( !SIRTerrainHeightfieldCodec::isValidDescriptor( invalidDescriptor ) );
	invalidDescriptor = descriptor;
	invalidDescriptor.nResolution = 0;
	CHECK( !SIRTerrainHeightfieldCodec::isValidDescriptor( invalidDescriptor ) );
	invalidDescriptor = descriptor;
	invalidDescriptor.fMaxHeight = INFINITY;
	CHECK( !SIRTerrainHeightfieldCodec::isValidDescriptor( invalidDescriptor ) );
	invalidDescriptor = descriptor;
	invalidDescriptor.fMaxHeight = descriptor.fMinHeight;
	CHECK( !SIRTerrainHeightfieldCodec::isValidDescriptor( invalidDescriptor ) );
	invalidDescriptor = descriptor;
	invalidDescriptor.fMinHeight = -FLT_MAX;
	invalidDescriptor.fMaxHeight = FLT_MAX;
	CHECK( !SIRTerrainHeightfieldCodec::isValidDescriptor( invalidDescriptor ) );
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { nullptr, 4 }, writable ) == SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, heights, { nullptr, nBytes } ) == SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( SIRTerrainHeightfieldCodec::inspect( { nullptr, nBytes }, readDescriptor ) == SIR_TERRAIN_CODEC_INVALID_ARGUMENT );
	CHECK( SIRTerrainHeightfieldCodec::inspect( { output.data(), 63 }, readDescriptor ) == SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL );
	CHECK( SIRTerrainHeightfieldCodec::decode( encoded, readDescriptor, { nullptr, 4 } ) == SIR_TERRAIN_CODEC_INVALID_ARGUMENT );

	std::vector<uint8_t> malformed = output;
	malformed[6] = 0;
	CHECK( SIRTerrainHeightfieldCodec::inspect( { malformed.data(), malformed.size() }, readDescriptor ) ==
	       SIR_TERRAIN_CODEC_INVALID_FORMAT );
	malformed = output;
	malformed[8] = 0;
	CHECK( SIRTerrainHeightfieldCodec::inspect( { malformed.data(), malformed.size() }, readDescriptor ) ==
	       SIR_TERRAIN_CODEC_INVALID_FORMAT );
	malformed = output;
	malformed[28] = 0;
	CHECK( SIRTerrainHeightfieldCodec::inspect( { malformed.data(), malformed.size() }, readDescriptor ) ==
	       SIR_TERRAIN_CODEC_INVALID_FORMAT );
	malformed = output;
	malformed[0] = '!';
	CHECK( SIRTerrainHeightfieldCodec::decode( { malformed.data(), malformed.size() }, readDescriptor, decoded ) ==
	       SIR_TERRAIN_CODEC_INVALID_FORMAT );
	malformed = output;
	malformed.push_back( 0 );
	CHECK( SIRTerrainHeightfieldCodec::inspect( { malformed.data(), malformed.size() }, readDescriptor ) ==
	       SIR_TERRAIN_CODEC_INVALID_FORMAT );
	float szLowHeight[4] = { -200.0f, 0.0f, 900.0f, 1200.0f };
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szLowHeight, 4 }, secondOutput ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( identical[64] == 0 && identical[65] == 0 );

	output[67] ^= 1;
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor ) == SIR_TERRAIN_CODEC_BAD_CHECKSUM );
	output[67] ^= 1;
	output[48] = 1;
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor ) == SIR_TERRAIN_CODEC_INVALID_FORMAT );
	output[48] = 0;
	output[4] = 2;
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor ) == SIR_TERRAIN_CODEC_UNSUPPORTED_VERSION );
	output[4] = 1;
	output[36] = 7;
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor ) == SIR_TERRAIN_CODEC_INVALID_FORMAT );
	output[36] = 8;
	output[12] ^= 1;
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor ) == SIR_TERRAIN_CODEC_BAD_CHECKSUM );
	output[12] ^= 1;
	output[0] = '!';
	CHECK( SIRTerrainHeightfieldCodec::inspect( encoded, readDescriptor ) == SIR_TERRAIN_CODEC_INVALID_FORMAT );
	return 0;
}
