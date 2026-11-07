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

#include <Renderer/VirtualTexturing/SIRVirtualTextureReferenceEncoder.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST( expression )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expression );                                                         \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

// Compiled from the unmodified Terrain3D reference encoder's block kernels.
// This independent oracle is linked by the local donor-parity harness only;
// production CTest independently validates outputs and negative paths.
#ifdef SIR_VT_DONOR_ORACLE
extern "C" bool donorReferenceEncode( uint32_t nFamily, uint32_t nComponents, const uint8_t *pSource, uint8_t *pTarget,
                                      uint8_t nThreshold );
#endif

int main( void )
{
	static constexpr uint32_t N_TILE = SIR_VT_PHYSICAL_TILE;
	static constexpr uint64_t s_szExpectedHashes[SIR_VT_PHYSICAL_COUNT] = { UINT64_C( 0x4feea8fea927d121 ), UINT64_C( 0x4307aefb7aa5b04e ),
	                                                                        UINT64_C( 0xbb9bda454c58c7fd ), UINT64_C( 0x859144d89fad1d98 ),
	                                                                        UINT64_C( 0x45e4a07338d88372 ) };
	uint8_t szPixels[N_TILE * N_TILE * 4U] = {};
	uint8_t szEncoded[N_TILE * N_TILE * 4U] = {};
	uint8_t szExpected[N_TILE * N_TILE * 4U] = {};
	for ( uint32_t iFamily = 0; iFamily < SIR_VT_PHYSICAL_COUNT; ++iFamily ) {
		const sirVirtualTexturePhysicalFamily_t eFamily = ( sirVirtualTexturePhysicalFamily_t )iFamily;
		const uint32_t nComponents = iFamily == SIR_VT_PHYSICAL_R8                                      ? 1U
		                             : iFamily == SIR_VT_PHYSICAL_R16 || iFamily == SIR_VT_PHYSICAL_BC5 ? 2U
		                                                                                                : 4U;
		const uint32_t nDecodedBytes = N_TILE * N_TILE * nComponents;
		const uint32_t nPhysicalBytes = sirVTPackPhysicalTileBytes( eFamily );
		for ( uint32_t i = 0; i < nDecodedBytes; ++i ) {
			szPixels[i] = ( uint8_t )( ( i * 47U + i / 13U * 17U + iFamily * 19U ) & 255U );
		}
		memset( szEncoded, 0xCC, sizeof( szEncoded ) );
		memset( szExpected, 0xDD, sizeof( szExpected ) );
		sirVirtualTextureReferenceEncodeRequest_t request = { szPixels, nDecodedBytes, szEncoded, sizeof( szEncoded ),
		                                                      eFamily,  nComponents,   128U };
		uint32_t nWritten = UINT32_MAX;
		SIR_TEST( sirVTReferenceEncodeTile( request, &nWritten ) && nWritten == nPhysicalBytes );
		uint64_t nHash = UINT64_C( 1469598103934665603 );
		for ( uint32_t i = 0; i < nPhysicalBytes; ++i ) {
			nHash = ( nHash ^ szEncoded[i] ) * UINT64_C( 1099511628211 );
		}
		SIR_TEST( nHash == s_szExpectedHashes[iFamily] );
#ifdef SIR_VT_DONOR_ORACLE
		SIR_TEST( donorReferenceEncode( iFamily, nComponents, szPixels, szExpected, 128U ) );
		SIR_TEST( memcmp( szEncoded, szExpected, nPhysicalBytes ) == 0 );
#endif
		for ( uint32_t i = nPhysicalBytes; i < sizeof( szEncoded ); ++i ) {
			SIR_TEST( szEncoded[i] == 0xCC );
		}
		request.nSourceBytes = nDecodedBytes - 1U;
		nWritten = UINT32_MAX;
		SIR_TEST( !sirVTReferenceEncodeTile( request, &nWritten ) && nWritten == 0 );
		request.nSourceBytes = nDecodedBytes;
		request.nDestinationCapacity = nPhysicalBytes - 1U;
		SIR_TEST( !sirVTReferenceEncodeTile( request, &nWritten ) && nWritten == 0 );
		request.nDestinationCapacity = nPhysicalBytes;
		request.pDestination = szPixels;
		SIR_TEST( !sirVTReferenceEncodeTile( request, &nWritten ) && nWritten == 0 );
		request.pDestination = szEncoded;
		request.nComponents = 5U;
		SIR_TEST( !sirVTReferenceEncodeTile( request, &nWritten ) );
	}
	sirVirtualTextureReferenceEncodeRequest_t invalid = {};
	uint32_t nWritten = 17;
	SIR_TEST( !sirVTReferenceEncodeTile( invalid, &nWritten ) && nWritten == 0 );
	SIR_TEST( !sirVTReferenceEncodeTile( invalid, nullptr ) );
	return 0;
}
