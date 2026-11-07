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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURESPATIALDEMAND_H__
#define __SIRENGINE_SIRVIRTUALTEXTURESPATIALDEMAND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Terrain3D's frozen T3VD stable-material (v2) spatial-demand sidecar.
// This is distinct from the Sparse V1 texture package and the V3 baked
// visibility bundle format; no texture-format compatibility is introduced.
static constexpr uint32_t SIR_VT_SPATIAL_DEMAND_MAGIC = 0x44563354U;
static constexpr uint32_t SIR_VT_SPATIAL_DEMAND_VERSION = 2U;
static constexpr uint32_t SIR_VT_SPATIAL_DEMAND_MAX_CELLS = 4096U;
static constexpr uint32_t SIR_VT_SPATIAL_DEMAND_MAX_RECORDS = 65536U;
static constexpr uint32_t SIR_VT_SPATIAL_DEMAND_MAX_PER_CELL = 64U;
static constexpr uint32_t SIR_VT_SPATIAL_DEMAND_CACHE_SLOTS = 4U;

typedef struct sirVirtualTextureSpatialDemandBaseHeader_s
{
	uint32_t nMagic;
	uint32_t nVersion;
	uint32_t nCellMeters;
	uint32_t nCellCount;
	uint32_t nRecordCount;
	uint32_t nReserved;
	uint64_t nPackageSignature;
	uint64_t nPayloadHash;
} sirVirtualTextureSpatialDemandBaseHeader_t;

typedef struct sirVirtualTextureSpatialDemandHeader_s
{
	sirVirtualTextureSpatialDemandBaseHeader_t base;
	uint64_t nAssetIdentityHash;
	int32_t nOriginCellX;
	int32_t nOriginCellZ;
	uint32_t nRecordStride;
	uint32_t nFlags;
} sirVirtualTextureSpatialDemandHeader_t;

typedef struct sirVirtualTextureSpatialDemandCell_s
{
	int32_t nX;
	int32_t nZ;
	uint32_t nFirstRecord;
	uint16_t nCount;
	uint16_t nReserved;
} sirVirtualTextureSpatialDemandCell_t;

typedef struct sirVirtualTextureSpatialDemandRecord_s
{
	uint32_t nAllocation;
	uint16_t nMip;
	uint16_t nCoverage;
	uint16_t nUvMinX;
	uint16_t nUvMinY;
	uint16_t nUvMaxX;
	uint16_t nUvMaxY;
	uint8_t nViewMask;
	uint8_t nReserved0;
	uint16_t nReserved1;
} sirVirtualTextureSpatialDemandRecord_t;

typedef struct sirVirtualTextureSpatialDemandStableRecord_s
{
	uint64_t nStableMaterialKey;
	sirVirtualTextureSpatialDemandRecord_t footprint;
	uint32_t nFlags;
} sirVirtualTextureSpatialDemandStableRecord_t;

static_assert( sizeof( sirVirtualTextureSpatialDemandBaseHeader_t ) == 40U, "Terrain3D T3VD base header ABI" );
static_assert( sizeof( sirVirtualTextureSpatialDemandHeader_t ) == 64U, "Terrain3D T3VD V2 header ABI" );
static_assert( sizeof( sirVirtualTextureSpatialDemandCell_t ) == 16U, "Terrain3D T3VD cell ABI" );
static_assert( sizeof( sirVirtualTextureSpatialDemandRecord_t ) == 20U, "Terrain3D T3VD record ABI" );
static_assert( sizeof( sirVirtualTextureSpatialDemandStableRecord_t ) == 32U, "Terrain3D T3VD stable record ABI" );

typedef struct sirVirtualTextureSpatialDemandCachedCell_s
{
	int32_t nX;
	int32_t nZ;
	uint32_t nCount;
	uint64_t nLastAccess;
	sirVirtualTextureSpatialDemandRecord_t szRecords[SIR_VT_SPATIAL_DEMAND_MAX_PER_CELL];
} sirVirtualTextureSpatialDemandCachedCell_t;

// The owner lane serializes cold binds and hot queries. Source bytes and the
// optional per-record binding array are externally owned and must outlive
// calls; all four hot cache slots are embedded and require no allocations.
class SIRVirtualTextureSpatialDemand final
{
public:
	SIRVirtualTextureSpatialDemand( void ) noexcept;
	~SIRVirtualTextureSpatialDemand( void ) noexcept = default;

	void resetCold( void ) noexcept;
	bool loadCold( const uint8_t *pBytes, uint64_t nBytes ) noexcept;
	bool isValidFor( uint64_t nPackageSignature, uint64_t nAssetIdentityHash ) const noexcept;
	bool bindStableKeysCold( const uint64_t *pKeys, const uint32_t *pRuntimeAllocations, uint32_t nKeys, uint32_t *pBoundAllocations,
	                         uint32_t nCapacity ) noexcept;
	const sirVirtualTextureSpatialDemandRecord_t *findCellHot( int32_t nX, int32_t nZ, uint32_t *pCount ) noexcept;
	bool hasCoveredCellHot( int32_t nX, int32_t nZ ) noexcept;
	uint32_t getMappedRecordCount( void ) const noexcept;
	uint32_t getCellCount( void ) const noexcept;
	uint32_t getRecordCount( void ) const noexcept;

protected:
private:
	void clearCache( void ) noexcept;
	sirVirtualTextureSpatialDemandCell_t readCell( uint32_t nIndex ) const noexcept;
	sirVirtualTextureSpatialDemandStableRecord_t readRecord( uint32_t nIndex ) const noexcept;

	const uint8_t *m_pBytes = nullptr;
	uint64_t m_nBytes = 0;
	uint32_t *m_pBoundAllocations = nullptr;
	uint32_t m_nMappedRecords = 0;
	uint64_t m_nAccessClock = 0;
	sirVirtualTextureSpatialDemandHeader_t m_Header = {};
	sirVirtualTextureSpatialDemandCachedCell_t m_szCache[SIR_VT_SPATIAL_DEMAND_CACHE_SLOTS] = {};
};

#endif // __SIRENGINE_SIRVIRTUALTEXTURESPATIALDEMAND_H__
