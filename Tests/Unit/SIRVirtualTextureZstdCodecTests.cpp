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

#include <Renderer/VirtualTexturing/SIRVirtualTextureZstdCodec.h>

#include <stdio.h>
#include <string.h>
#include <zstd.h>

#define SIR_TEST( expression )                                                                                                             \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expression );                                                         \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	uint8_t szOriginal[16384] = {};
	uint8_t szEncoded[16384] = {};
	uint8_t szDecoded[16384] = {};
	for ( uint32_t i = 0; i < sizeof( szOriginal ); ++i ) {
		szOriginal[i] = ( uint8_t )( i / 19U % 7U );
	}
	const size_t nStored = ZSTD_compress( szEncoded, sizeof( szEncoded ), szOriginal, sizeof( szOriginal ), 3 );
	SIR_TEST( !ZSTD_isError( nStored ) && nStored < sizeof( szEncoded ) );
	const sirVirtualTextureClusterCodec_t *pCodec = sirVTGetZstdClusterCodec();
	SIR_TEST( pCodec != nullptr && pCodec->pfnDecodeZstd != nullptr );
	const sirVirtualTextureCodecFrame_t good = { szEncoded, ( uint32_t )nStored, szDecoded, sizeof( szDecoded ) };
	uint32_t nWritten = UINT32_MAX;
	SIR_TEST( pCodec->pfnDecodeZstd( pCodec->pContext, good, &nWritten ) );
	SIR_TEST( nWritten == sizeof( szOriginal ) && memcmp( szDecoded, szOriginal, nWritten ) == 0 );
	SIR_TEST( !pCodec->pfnDecodeZstd( pCodec->pContext, good, nullptr ) );
	SIR_TEST( !pCodec->pfnDecodeZstd( pCodec->pContext, { nullptr, 0, szDecoded, sizeof( szDecoded ) }, &nWritten ) && nWritten == 0 );
	SIR_TEST( !pCodec->pfnDecodeZstd( pCodec->pContext, { szEncoded, ( uint32_t )nStored, nullptr, 0 }, &nWritten ) && nWritten == 0 );
	SIR_TEST( !pCodec->pfnDecodeZstd( pCodec->pContext, { szEncoded, ( uint32_t )nStored, szDecoded, 16 }, &nWritten ) && nWritten == 0 );
	SIR_TEST(
	    !pCodec->pfnDecodeZstd( pCodec->pContext, { szEncoded, ( uint32_t )nStored - 1U, szDecoded, sizeof( szDecoded ) }, &nWritten ) &&
	    nWritten == 0 );
	SIR_TEST( !pCodec->pfnDecodeZstd( pCodec->pContext, { szEncoded, ( uint32_t )nStored, szEncoded, sizeof( szEncoded ) }, &nWritten ) &&
	          nWritten == 0 );
	szEncoded[0] = 0;
	SIR_TEST( !pCodec->pfnDecodeZstd( pCodec->pContext, good, &nWritten ) && nWritten == 0 );
	return 0;
}
