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

#include <Renderer/VirtualTexturing/SIRVirtualTextureDemandBundleCodec.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST( expression )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #expression );                                                                \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

static void sirVTWriteU32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		pBytes[i] = ( uint8_t )( nValue >> ( 8U * i ) );
	}
}

int main( void )
{
	uint8_t szWire[64] = {};
	uint8_t szScratch[64] = {};
	sirVirtualTextureDemandVisibilityRecord_t output = {};
	szWire[0] = 42;
	szWire[8] = 1;
	szWire[10] = 255;
	szWire[11] = 255;
	sirVTWriteU32( szWire + 12, 400 );
	szWire[20] = 100;
	szWire[28] = 100;
	szWire[48] = 2;
	sirVTWriteU32( szWire + 52, 1 );
	sirVTWriteU32( szWire + 56, 2 );
	sirVTWriteU32( szWire + 60, 25 );
	sirVirtualTextureDemandBundle_t bundle = {};
	bundle.nCount = 1;
	bundle.nEncodedBytes = 64;
	bundle.nDecodedBytes = 64;
	bundle.nHash = sirVTDemandBundleHash( szWire, 64 );
	sirVirtualTextureDemandBundleBuffers_t buffers = { szWire, 64, szScratch, 64, &output, 1 };
	SIR_TEST( sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	SIR_TEST( output.nStableMaterialKey == 42 && output.szMipWeight[2] == 100 && output.nCoverageSampleCount == 25 );
	SIR_TEST( sirVTDemandBundleHash( nullptr, 0 ) == UINT64_C( 14695981039346656037 ) );
	SIR_TEST( sirVTDemandBundleHash( nullptr, 1 ) == 0 );

	uint8_t szPack[65] = { 63 };
	memcpy( szPack + 1, szWire, 64 );
	bundle.nCodec = 1;
	bundle.nEncodedBytes = 65;
	bundle.nHash = sirVTDemandBundleHash( szPack, 65 );
	buffers.pEncoded = szPack;
	buffers.nEncodedBytes = 65;
	SIR_TEST( sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	bundle.nHash++;
	SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	bundle.nHash--;
	szPack[0] = 128;
	bundle.nHash = sirVTDemandBundleHash( szPack, 65 );
	SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	szPack[0] = 126;
	bundle.nHash = sirVTDemandBundleHash( szPack, 65 );
	SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	szPack[0] = 63;
	bundle.nHash = sirVTDemandBundleHash( szPack, 65 );
	buffers.pScratch = szPack + 1;
	SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	buffers.pScratch = szScratch;
	buffers.nRecordCapacity = 0;
	SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	buffers.nRecordCapacity = 1;
	for ( uint32_t i = 0; i < 8; ++i ) {
		const uint32_t szOffsets[8] = { 0, 8, 9, 48, 50, 52, 56, 61 };
		const uint8_t szMutations[8] = { 0, 9, 128, 16, 1, 0, 0, 2 };
		const uint8_t nOriginal = szPack[1 + szOffsets[i]];
		szPack[1 + szOffsets[i]] = szMutations[i];
		bundle.nHash = sirVTDemandBundleHash( szPack, 65 );
		SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
		szPack[1 + szOffsets[i]] = nOriginal;
	}
	bundle.nHash = sirVTDemandBundleHash( szPack, 65 );
	SIR_TEST( sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	buffers.pEncoded = nullptr;
	SIR_TEST( !sirVTDemandBundleDecodeBundle( bundle, buffers ) );
	return 0;
}
