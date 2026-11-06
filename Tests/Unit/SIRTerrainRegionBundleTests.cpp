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

#include <Terrain/Data/SIRTerrainRegionBundle.h>

#include <stdio.h>
#include <string.h>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #expression );                                                     \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

static uint64_t sirBundleTestHash( const uint8_t *pData, size_t nCount ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( size_t nIndex = 0; nIndex < nCount; ++nIndex ) {
		nHash = ( nHash ^ pData[nIndex] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static void sirBundleTestWrite64( uint8_t *pData, uint64_t nValue ) noexcept
{
	for ( uint32_t nIndex = 0; nIndex < 8; ++nIndex ) {
		pData[nIndex] = ( uint8_t )( nValue >> ( nIndex * 8u ) );
	}
}

static void sirBundleTestRepairHashes( std::vector<uint8_t>& bytes, uint32_t nCount ) noexcept
{
	const size_t nIndexBytes = ( size_t )nCount * SIRTerrainRegionBundle::ENTRY_BYTES;
	sirBundleTestWrite64( bytes.data() + 24, sirBundleTestHash( bytes.data() + 64, nIndexBytes ) );
	sirBundleTestWrite64( bytes.data() + 32, sirBundleTestHash( bytes.data() + 64 + nIndexBytes, bytes.size() - 64 - nIndexBytes ) );
	sirBundleTestWrite64( bytes.data() + 40, sirBundleTestHash( bytes.data(), 40 ) );
}

int main( void )
{
	const sirTerrainRegionDescriptor_t descriptors[3] = {
	    { -12, 7, 2, -100.0f, 100.0f, 4 }, { -12, 8, 3, 0.0f, 15.0f, 5 }, { 5, -2, 2, -20.0f, 20.0f, 6 } };
	const float szFirst[4] = { -100.0f, -25.0f, 25.0f, 100.0f };
	const float szSecond[9] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 15.0f };
	const float szThird[4] = { -20.0f, -10.0f, 10.0f, 20.0f };
	const sirTerrainConstHeightSpan_t heights[3] = { { szFirst, 4 }, { szSecond, 9 }, { szThird, 4 } };
	std::vector<uint8_t> encoded[3];
	sirTerrainConstByteSpan_t source[3] = {};
	for ( uint32_t nIndex = 0; nIndex < 3; ++nIndex ) {
		encoded[nIndex].resize( SIRTerrainHeightfieldCodec::getEncodedByteCount( descriptors[nIndex].nResolution ) );
		CHECK( SIRTerrainHeightfieldCodec::encode( descriptors[nIndex], heights[nIndex],
		                                           { encoded[nIndex].data(), encoded[nIndex].size() } ) == SIR_TERRAIN_CODEC_SUCCESS );
		source[nIndex] = { encoded[nIndex].data(), encoded[nIndex].size() };
	}

	size_t nBytes = 11;
	CHECK( SIRTerrainRegionBundle::measure( source, 3, nBytes ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( nBytes == 64 + 3 * 40 + encoded[0].size() + encoded[1].size() + encoded[2].size() );
	CHECK( SIRTerrainRegionBundle::measure( nullptr, 3, nBytes ) == SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT );
	CHECK( SIRTerrainRegionBundle::measure( source, 0, nBytes ) == SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT );
	CHECK( SIRTerrainRegionBundle::measure( source, 65536, nBytes ) == SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT );
	const sirTerrainConstByteSpan_t unsorted[2] = { source[2], source[0] };
	CHECK( SIRTerrainRegionBundle::measure( unsorted, 2, nBytes ) == SIR_TERRAIN_BUNDLE_DUPLICATE_OR_UNSORTED );
	const sirTerrainConstByteSpan_t repeated[2] = { source[0], source[0] };
	CHECK( SIRTerrainRegionBundle::measure( repeated, 2, nBytes ) == SIR_TERRAIN_BUNDLE_DUPLICATE_OR_UNSORTED );
	CHECK( SIRTerrainRegionBundle::measure( source, 3, nBytes ) == SIR_TERRAIN_BUNDLE_SUCCESS );

	std::vector<uint8_t> packed( nBytes + 1, 0xA5 );
	size_t nWritten = 17;
	CHECK( SIRTerrainRegionBundle::write( source, 3, { packed.data(), nBytes - 1 }, nWritten ) == SIR_TERRAIN_BUNDLE_BUFFER_TOO_SMALL );
	CHECK( nWritten == 17 && packed[0] == 0xA5 );
	CHECK( SIRTerrainRegionBundle::write( source, 3, { packed.data(), packed.size() }, nWritten ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( nWritten == nBytes && packed[nBytes] == 0xA5 );
	CHECK( packed[0] == 'S' && packed[1] == 'T' && packed[2] == 'R' && packed[3] == 'B' );
	std::vector<uint8_t> second( nBytes );
	size_t nSecond = 0;
	CHECK( SIRTerrainRegionBundle::write( source, 3, { second.data(), second.size() }, nSecond ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( nWritten == nSecond && memcmp( packed.data(), second.data(), nBytes ) == 0 );

	sirTerrainRegionBundleView_t view = {};
	CHECK( SIRTerrainRegionBundle::open( { packed.data(), nBytes }, view ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( view.nRegionCount == 3 && view.nBytes == nBytes );
	for ( uint32_t nIndex = 0; nIndex < 3; ++nIndex ) {
		sirTerrainConstByteSpan_t found = {};
		CHECK( SIRTerrainRegionBundle::find( view, descriptors[nIndex].iRegionX, descriptors[nIndex].iRegionZ, found ) ==
		       SIR_TERRAIN_BUNDLE_SUCCESS );
		CHECK( found.nCapacity == source[nIndex].nCapacity );
		CHECK( memcmp( found.pData, source[nIndex].pData, found.nCapacity ) == 0 );
		sirTerrainRegionDescriptor_t decoded = {};
		CHECK( SIRTerrainHeightfieldCodec::inspect( found, decoded ) == SIR_TERRAIN_CODEC_SUCCESS );
		CHECK( decoded.nRevision == descriptors[nIndex].nRevision );
	}
	sirTerrainConstByteSpan_t missing = { source[0].pData, 777 };
	CHECK( SIRTerrainRegionBundle::find( view, -12, 9, missing ) == SIR_TERRAIN_BUNDLE_NOT_FOUND );
	CHECK( missing.nCapacity == 777 );
	CHECK( SIRTerrainRegionBundle::find( view, 7, -2, missing ) == SIR_TERRAIN_BUNDLE_NOT_FOUND );
	CHECK( SIRTerrainRegionBundle::find( { nullptr, nBytes, 3 }, -12, 7, missing ) == SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT );
	CHECK( SIRTerrainRegionBundle::find( { packed.data(), 64, 3 }, -12, 7, missing ) == SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT );
	CHECK( SIRTerrainRegionBundle::write( source, 3, { packed.data(), nBytes }, nWritten ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	std::vector<uint8_t> aliased( 64 + 40 + source[0].nCapacity );
	memcpy( aliased.data(), source[0].pData, source[0].nCapacity );
	const sirTerrainConstByteSpan_t overlap[] = { { aliased.data(), source[0].nCapacity } };
	CHECK( SIRTerrainRegionBundle::write( overlap, 1, { aliased.data(), aliased.size() }, nWritten ) ==
	       SIR_TERRAIN_BUNDLE_OVERLAPPING_BUFFERS );
	CHECK( nWritten == nBytes && aliased[0] == 'S' );

	std::vector<uint8_t> altered = second;
	sirTerrainRegionBundleView_t sentinel = { ( const uint8_t * )0x1, 99, 77 };
	altered[0] = 'X';
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	CHECK( sentinel.nRegionCount == 77 );
	altered = second;
	altered[4] = 2;
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_UNSUPPORTED_VERSION );
	altered = second;
	altered[48] = 1;
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	altered = second;
	altered[64] ^= 1;
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_BAD_CHECKSUM );
	altered = second;
	altered[nBytes - 1] ^= 1;
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_BAD_CHECKSUM );
	altered = second;
	altered[41] ^= 1;
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_BAD_CHECKSUM );
	altered = second;
	altered[64 + 40 + 4] = 7;
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_DUPLICATE_OR_UNSORTED );
	altered = second;
	altered[64 + 16] += 1;
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	altered = second;
	altered[64 + 40] = altered[64];
	altered[64 + 40 + 4] = altered[64 + 4];
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) != SIR_TERRAIN_BUNDLE_SUCCESS );
	altered = second;
	altered[64 + 36] = 1;
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	altered = second;
	altered[64 + 8] += 1;
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	altered = second;
	altered[64 + 3 * 40 + 5] ^= 1;
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	altered = second;
	altered[64 + 32] = 7;
	sirBundleTestRepairHashes( altered, 3 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	altered = second;
	altered.resize( altered.size() - 1 );
	CHECK( SIRTerrainRegionBundle::open( { altered.data(), altered.size() }, sentinel ) == SIR_TERRAIN_BUNDLE_INVALID_FORMAT );
	CHECK( sentinel.nRegionCount == 77 );
	return 0;
}
