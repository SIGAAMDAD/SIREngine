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

#include <Renderer/VirtualTexturing/SIRVirtualTextureSpatialDemand.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static constexpr uint32_t SIR_TEST_CELL_COUNT = 5U;
static constexpr uint32_t SIR_TEST_RECORD_COUNT = 6U;
static constexpr uint32_t SIR_TEST_PACK_BYTES = sizeof( sirVirtualTextureSpatialDemandHeader_t ) +
                                                SIR_TEST_CELL_COUNT * sizeof( sirVirtualTextureSpatialDemandCell_t ) +
                                                SIR_TEST_RECORD_COUNT * sizeof( sirVirtualTextureSpatialDemandStableRecord_t );

static uint64_t sirTestHash( const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint32_t i = 0; i < nBytes; ++i ) {
		nHash = ( nHash ^ pBytes[i] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static void sirSignFixture( uint8_t *pBytes ) noexcept
{
	sirVirtualTextureSpatialDemandHeader_t header = {};
	memcpy( &header, pBytes, sizeof( header ) );
	header.base.nPayloadHash = sirTestHash( pBytes + sizeof( header ), SIR_TEST_PACK_BYTES - sizeof( header ) );
	memcpy( pBytes, &header, sizeof( header ) );
}

static void sirMakeFixture( uint8_t *pBytes ) noexcept
{
	memset( pBytes, 0, SIR_TEST_PACK_BYTES );
	sirVirtualTextureSpatialDemandHeader_t header = {};
	header.base.nMagic = SIR_VT_SPATIAL_DEMAND_MAGIC;
	header.base.nVersion = SIR_VT_SPATIAL_DEMAND_VERSION;
	header.base.nCellMeters = 128;
	header.base.nCellCount = SIR_TEST_CELL_COUNT;
	header.base.nRecordCount = SIR_TEST_RECORD_COUNT;
	header.base.nPackageSignature = 0x123456789ULL;
	header.nAssetIdentityHash = 0x87654321ULL;
	header.nRecordStride = sizeof( sirVirtualTextureSpatialDemandStableRecord_t );
	memcpy( pBytes, &header, sizeof( header ) );
	const int32_t szX[SIR_TEST_CELL_COUNT] = { -2, 3, 3, 4, 5 };
	const int32_t szZ[SIR_TEST_CELL_COUNT] = { 4, 0, 1, -1, 5 };
	for ( uint32_t i = 0; i < SIR_TEST_CELL_COUNT; ++i ) {
		sirVirtualTextureSpatialDemandCell_t cell = {};
		cell.nX = szX[i];
		cell.nZ = szZ[i];
		cell.nFirstRecord = i == 0 ? 0 : i + 1;
		cell.nCount = i == 0 ? 2 : 1;
		if ( i > 0 ) {
			cell.nFirstRecord = i + 1;
		}
		memcpy( pBytes + sizeof( header ) + i * sizeof( cell ), &cell, sizeof( cell ) );
	}
	const uint32_t nRecordOffset = sizeof( header ) + SIR_TEST_CELL_COUNT * sizeof( sirVirtualTextureSpatialDemandCell_t );
	for ( uint32_t i = 0; i < SIR_TEST_RECORD_COUNT; ++i ) {
		sirVirtualTextureSpatialDemandStableRecord_t record = {};
		record.nStableMaterialKey = 100ULL + i;
		record.footprint.nCoverage = ( uint16_t )( 100U + i );
		record.footprint.nMip = 2;
		record.footprint.nUvMaxX = 65535;
		record.footprint.nUvMaxY = 65535;
		record.footprint.nViewMask = 255;
		record.nFlags = i == 1 ? 1 : 0;
		memcpy( pBytes + nRecordOffset + i * sizeof( record ), &record, sizeof( record ) );
	}
	sirSignFixture( pBytes );
}

static bool testValidFixture( void ) noexcept
{
	uint8_t szPack[SIR_TEST_PACK_BYTES] = {};
	sirMakeFixture( szPack );
	SIRVirtualTextureSpatialDemand index;
	SIR_TEST( index.loadCold( szPack, sizeof( szPack ) ) );
	SIR_TEST( index.isValidFor( 0x123456789ULL, 0x87654321ULL ) );
	SIR_TEST( !index.isValidFor( 0x123456788ULL, 0x87654321ULL ) );
	SIR_TEST( !index.isValidFor( 0x123456789ULL, 0x87654320ULL ) );
	SIR_TEST( index.getCellCount() == SIR_TEST_CELL_COUNT );
	SIR_TEST( index.getRecordCount() == SIR_TEST_RECORD_COUNT );
	SIR_TEST( index.getMappedRecordCount() == 0 );
	SIR_TEST( !index.hasCoveredCellHot( -2, 4 ) );
	uint32_t nCount = 0;
	const sirVirtualTextureSpatialDemandRecord_t *pRecords = index.findCellHot( -2, 4, &nCount );
	SIR_TEST( pRecords != nullptr && nCount == 2 && pRecords[0].nAllocation == UINT32_MAX );
	SIR_TEST( pRecords[1].nReserved1 == 1 );
	SIR_TEST( index.findCellHot( -2, 4, nullptr ) == nullptr );
	SIR_TEST( index.findCellHot( 55, 88, &nCount ) == nullptr && nCount == 0 );
	const uint64_t szKeys[3] = { 100, 102, 105 };
	const uint32_t szAllocations[3] = { 9, 11, 16 };
	uint32_t szBound[SIR_TEST_RECORD_COUNT] = {};
	SIR_TEST( !index.bindStableKeysCold( szKeys, szAllocations, 3, szBound, 5 ) );
	SIR_TEST( index.bindStableKeysCold( szKeys, szAllocations, 3, szBound, SIR_TEST_RECORD_COUNT ) );
	SIR_TEST( index.getMappedRecordCount() == 3 );
	pRecords = index.findCellHot( -2, 4, &nCount );
	SIR_TEST( pRecords != nullptr && nCount == 2 && pRecords[0].nAllocation == 9 && pRecords[1].nAllocation == UINT32_MAX );
	SIR_TEST( index.hasCoveredCellHot( -2, 4 ) );
	SIR_TEST( !index.hasCoveredCellHot( 3, 1 ) );
	SIR_TEST( index.hasCoveredCellHot( 3, 0 ) );
	SIR_TEST( index.hasCoveredCellHot( 5, 5 ) );
	for ( uint32_t i = 0; i < SIR_TEST_CELL_COUNT; ++i ) {
		const int32_t szX[SIR_TEST_CELL_COUNT] = { -2, 3, 3, 4, 5 };
		const int32_t szZ[SIR_TEST_CELL_COUNT] = { 4, 0, 1, -1, 5 };
		pRecords = index.findCellHot( szX[i], szZ[i], &nCount );
		SIR_TEST( pRecords != nullptr && nCount >= 1 );
	}
	SIR_TEST( !index.hasCoveredCellHot( 88, 99 ) );
	const uint64_t szInvalidKeys[2] = { 102, 102 };
	SIR_TEST( !index.bindStableKeysCold( szInvalidKeys, szAllocations, 2, szBound, SIR_TEST_RECORD_COUNT ) );
	SIR_TEST( index.getMappedRecordCount() == 0 );
	index.resetCold();
	SIR_TEST( !index.isValidFor( 0x123456789ULL, 0x87654321ULL ) );
	SIR_TEST( index.findCellHot( -2, 4, &nCount ) == nullptr && nCount == 0 );
	return true;
}

static bool testMalformedFixture( void ) noexcept
{
	uint8_t szPack[SIR_TEST_PACK_BYTES] = {};
	SIRVirtualTextureSpatialDemand index;
	sirMakeFixture( szPack );
	SIR_TEST( !index.loadCold( nullptr, sizeof( szPack ) ) );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) - 1 ) );
	SIR_TEST( !index.loadCold( szPack, UINT64_MAX ) );
	SIR_TEST( index.loadCold( szPack, sizeof( szPack ) ) );
	szPack[sizeof( szPack ) - 1] ^= 1;
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	SIR_TEST( index.getCellCount() == 0 );
	sirMakeFixture( szPack );
	sirVirtualTextureSpatialDemandHeader_t header = {};
	memcpy( &header, szPack, sizeof( header ) );
	header.base.nPackageSignature = 0;
	memcpy( szPack, &header, sizeof( header ) );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	sirMakeFixture( szPack );
	memcpy( &header, szPack, sizeof( header ) );
	header.base.nVersion = 1;
	memcpy( szPack, &header, sizeof( header ) );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	sirMakeFixture( szPack );
	sirVirtualTextureSpatialDemandCell_t cell = {};
	memcpy( &cell, szPack + sizeof( header ) + sizeof( cell ), sizeof( cell ) );
	cell.nX = -2;
	cell.nZ = 4;
	memcpy( szPack + sizeof( header ) + sizeof( cell ), &cell, sizeof( cell ) );
	sirSignFixture( szPack );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	sirMakeFixture( szPack );
	memcpy( &cell, szPack + sizeof( header ), sizeof( cell ) );
	cell.nCount = 65;
	memcpy( szPack + sizeof( header ), &cell, sizeof( cell ) );
	sirSignFixture( szPack );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	const uint32_t nRecordOffset = sizeof( header ) + SIR_TEST_CELL_COUNT * sizeof( sirVirtualTextureSpatialDemandCell_t );
	sirMakeFixture( szPack );
	sirVirtualTextureSpatialDemandStableRecord_t record = {};
	memcpy( &record, szPack + nRecordOffset, sizeof( record ) );
	record.nStableMaterialKey = 0;
	memcpy( szPack + nRecordOffset, &record, sizeof( record ) );
	sirSignFixture( szPack );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	sirMakeFixture( szPack );
	memcpy( &record, szPack + nRecordOffset, sizeof( record ) );
	record.footprint.nUvMinY = 2;
	record.footprint.nUvMaxY = 1;
	memcpy( szPack + nRecordOffset, &record, sizeof( record ) );
	sirSignFixture( szPack );
	SIR_TEST( !index.loadCold( szPack, sizeof( szPack ) ) );
	return true;
}

int main( void )
{
	if ( !testValidFixture() || !testMalformedFixture() ) {
		return 1;
	}
	printf( "Terrain3D T3VD v2 spatial demand index PASS\n" );
	return 0;
}
