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

#include <limits.h>
#include <string.h>

static uint64_t sirVTDemandSpatialHash( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint64_t i = 0; i < nBytes; ++i ) {
		nHash = ( nHash ^ pBytes[i] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

SIRVirtualTextureSpatialDemand::SIRVirtualTextureSpatialDemand( void ) noexcept
{
	resetCold();
}

void SIRVirtualTextureSpatialDemand::clearCache( void ) noexcept
{
	for ( uint32_t i = 0; i < SIR_VT_SPATIAL_DEMAND_CACHE_SLOTS; ++i ) {
		m_szCache[i].nCount = 0;
		m_szCache[i].nLastAccess = 0;
	}
	m_nAccessClock = 0;
}

void SIRVirtualTextureSpatialDemand::resetCold( void ) noexcept
{
	m_pBytes = nullptr;
	m_nBytes = 0;
	m_pBoundAllocations = nullptr;
	m_nMappedRecords = 0;
	m_Header = {};
	clearCache();
}

sirVirtualTextureSpatialDemandCell_t SIRVirtualTextureSpatialDemand::readCell( uint32_t nIndex ) const noexcept
{
	sirVirtualTextureSpatialDemandCell_t cell = {};
	memcpy( &cell, m_pBytes + sizeof( m_Header ) + ( uint64_t )nIndex * sizeof( cell ), sizeof( cell ) );
	return cell;
}

sirVirtualTextureSpatialDemandStableRecord_t SIRVirtualTextureSpatialDemand::readRecord( uint32_t nIndex ) const noexcept
{
	sirVirtualTextureSpatialDemandStableRecord_t record = {};
	const uint64_t nRecordOffset =
	    sizeof( m_Header ) + ( uint64_t )m_Header.base.nCellCount * sizeof( sirVirtualTextureSpatialDemandCell_t );
	memcpy( &record, m_pBytes + nRecordOffset + ( uint64_t )nIndex * sizeof( record ), sizeof( record ) );
	return record;
}

bool SIRVirtualTextureSpatialDemand::loadCold( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	resetCold();
	if ( pBytes == nullptr || nBytes < sizeof( m_Header ) || nBytes > 4U * 1024U * 1024U ) {
		return false;
	}
	sirVirtualTextureSpatialDemandHeader_t header = {};
	memcpy( &header, pBytes, sizeof( header ) );
	if ( header.base.nMagic != SIR_VT_SPATIAL_DEMAND_MAGIC || header.base.nVersion != SIR_VT_SPATIAL_DEMAND_VERSION ||
	     header.base.nCellMeters != 128 || header.base.nReserved != 0 || header.base.nPackageSignature == 0 ||
	     header.base.nCellCount > SIR_VT_SPATIAL_DEMAND_MAX_CELLS || header.base.nRecordCount > SIR_VT_SPATIAL_DEMAND_MAX_RECORDS ||
	     header.nAssetIdentityHash == 0 || header.nRecordStride != sizeof( sirVirtualTextureSpatialDemandStableRecord_t ) ||
	     header.nFlags != 0 ) {
		return false;
	}
	const uint64_t nCellsBytes = ( uint64_t )header.base.nCellCount * sizeof( sirVirtualTextureSpatialDemandCell_t );
	const uint64_t nRecordsBytes = ( uint64_t )header.base.nRecordCount * sizeof( sirVirtualTextureSpatialDemandStableRecord_t );
	if ( nBytes != sizeof( header ) + nCellsBytes + nRecordsBytes ||
	     sirVTDemandSpatialHash( pBytes + sizeof( header ), nCellsBytes + nRecordsBytes ) != header.base.nPayloadHash ) {
		return false;
	}
	int32_t nLastX = INT32_MIN;
	int32_t nLastZ = INT32_MIN;
	uint32_t nExpected = 0;
	for ( uint32_t i = 0; i < header.base.nCellCount; ++i ) {
		sirVirtualTextureSpatialDemandCell_t cell = {};
		memcpy( &cell, pBytes + sizeof( header ) + ( uint64_t )i * sizeof( cell ), sizeof( cell ) );
		if ( cell.nReserved != 0 || cell.nCount == 0 || cell.nCount > SIR_VT_SPATIAL_DEMAND_MAX_PER_CELL ||
		     cell.nFirstRecord != nExpected || cell.nCount > header.base.nRecordCount - nExpected ||
		     ( i > 0 && ( cell.nX < nLastX || ( cell.nX == nLastX && cell.nZ <= nLastZ ) ) ) ) {
			return false;
		}
		nExpected += cell.nCount;
		nLastX = cell.nX;
		nLastZ = cell.nZ;
	}
	if ( nExpected != header.base.nRecordCount ) {
		return false;
	}
	const uint64_t nRecordOffset = sizeof( header ) + nCellsBytes;
	for ( uint32_t i = 0; i < header.base.nRecordCount; ++i ) {
		sirVirtualTextureSpatialDemandStableRecord_t record = {};
		memcpy( &record, pBytes + nRecordOffset + ( uint64_t )i * sizeof( record ), sizeof( record ) );
		if ( record.nStableMaterialKey == 0 || ( record.nFlags & ~1U ) != 0 || record.footprint.nReserved0 != 0 ||
		     record.footprint.nReserved1 != 0 || record.footprint.nCoverage == 0 || record.footprint.nViewMask == 0 ||
		     record.footprint.nUvMinX > record.footprint.nUvMaxX || record.footprint.nUvMinY > record.footprint.nUvMaxY ) {
			return false;
		}
	}
	m_Header = header;
	m_pBytes = pBytes;
	m_nBytes = nBytes;
	return true;
}

bool SIRVirtualTextureSpatialDemand::isValidFor( uint64_t nPackageSignature, uint64_t nAssetIdentityHash ) const noexcept
{
	return m_pBytes != nullptr && nPackageSignature != 0 && m_Header.base.nPackageSignature == nPackageSignature &&
	       nAssetIdentityHash != 0 && m_Header.nAssetIdentityHash == nAssetIdentityHash;
}

bool SIRVirtualTextureSpatialDemand::bindStableKeysCold( const uint64_t *pKeys, const uint32_t *pRuntimeAllocations, uint32_t nKeys,
                                                         uint32_t *pBoundAllocations, uint32_t nCapacity ) noexcept
{
	m_pBoundAllocations = nullptr;
	m_nMappedRecords = 0;
	clearCache();
	if ( m_pBytes == nullptr || pBoundAllocations == nullptr || nCapacity < m_Header.base.nRecordCount ||
	     ( nKeys != 0 && ( pKeys == nullptr || pRuntimeAllocations == nullptr ) ) ) {
		return false;
	}
	for ( uint32_t i = 1; i < nKeys; ++i ) {
		if ( pKeys[i] <= pKeys[i - 1] ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < m_Header.base.nRecordCount; ++i ) {
		pBoundAllocations[i] = UINT32_MAX;
		const sirVirtualTextureSpatialDemandStableRecord_t record = readRecord( i );
		uint32_t nLow = 0;
		uint32_t nHigh = nKeys;
		while ( nLow < nHigh ) {
			const uint32_t nMid = nLow + ( nHigh - nLow ) / 2U;
			if ( pKeys[nMid] < record.nStableMaterialKey ) {
				nLow = nMid + 1;
			} else {
				nHigh = nMid;
			}
		}
		if ( nLow < nKeys && pKeys[nLow] == record.nStableMaterialKey && pRuntimeAllocations[nLow] != UINT32_MAX ) {
			pBoundAllocations[i] = pRuntimeAllocations[nLow];
			++m_nMappedRecords;
		}
	}
	m_pBoundAllocations = pBoundAllocations;
	return m_nMappedRecords != 0;
}

const sirVirtualTextureSpatialDemandRecord_t *SIRVirtualTextureSpatialDemand::findCellHot( int32_t nX, int32_t nZ,
                                                                                           uint32_t *pCount ) noexcept
{
	if ( pCount == nullptr ) {
		return nullptr;
	}
	*pCount = 0;
	if ( m_pBytes == nullptr ) {
		return nullptr;
	}
	for ( uint32_t i = 0; i < SIR_VT_SPATIAL_DEMAND_CACHE_SLOTS; ++i ) {
		sirVirtualTextureSpatialDemandCachedCell_t& slot = m_szCache[i];
		if ( slot.nCount != 0 && slot.nX == nX && slot.nZ == nZ ) {
			slot.nLastAccess = ++m_nAccessClock;
			*pCount = slot.nCount;
			return slot.szRecords;
		}
	}
	uint32_t nLow = 0;
	uint32_t nHigh = m_Header.base.nCellCount;
	while ( nLow < nHigh ) {
		const uint32_t nMid = nLow + ( nHigh - nLow ) / 2U;
		const sirVirtualTextureSpatialDemandCell_t cell = readCell( nMid );
		if ( cell.nX < nX || ( cell.nX == nX && cell.nZ < nZ ) ) {
			nLow = nMid + 1;
		} else {
			nHigh = nMid;
		}
	}
	if ( nLow >= m_Header.base.nCellCount ) {
		return nullptr;
	}
	const sirVirtualTextureSpatialDemandCell_t cell = readCell( nLow );
	if ( cell.nX != nX || cell.nZ != nZ ) {
		return nullptr;
	}
	uint32_t nVictim = 0;
	for ( uint32_t i = 1; i < SIR_VT_SPATIAL_DEMAND_CACHE_SLOTS; ++i ) {
		if ( m_szCache[i].nLastAccess < m_szCache[nVictim].nLastAccess ) {
			nVictim = i;
		}
	}
	sirVirtualTextureSpatialDemandCachedCell_t& slot = m_szCache[nVictim];
	for ( uint32_t i = 0; i < cell.nCount; ++i ) {
		const sirVirtualTextureSpatialDemandStableRecord_t record = readRecord( cell.nFirstRecord + i );
		slot.szRecords[i] = record.footprint;
		slot.szRecords[i].nReserved1 = ( uint16_t )record.nFlags;
		slot.szRecords[i].nAllocation = m_pBoundAllocations == nullptr ? UINT32_MAX : m_pBoundAllocations[cell.nFirstRecord + i];
	}
	slot.nX = nX;
	slot.nZ = nZ;
	slot.nCount = cell.nCount;
	slot.nLastAccess = ++m_nAccessClock;
	*pCount = slot.nCount;
	return slot.szRecords;
}

bool SIRVirtualTextureSpatialDemand::hasCoveredCellHot( int32_t nX, int32_t nZ ) noexcept
{
	if ( m_nMappedRecords == 0 ) {
		return false;
	}
	uint32_t nCount = 0;
	const sirVirtualTextureSpatialDemandRecord_t *pRecords = findCellHot( nX, nZ, &nCount );
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( pRecords[i].nAllocation != UINT32_MAX ) {
			return true;
		}
	}
	return false;
}

uint32_t SIRVirtualTextureSpatialDemand::getMappedRecordCount( void ) const noexcept
{
	return m_nMappedRecords;
}

uint32_t SIRVirtualTextureSpatialDemand::getCellCount( void ) const noexcept
{
	return m_Header.base.nCellCount;
}

uint32_t SIRVirtualTextureSpatialDemand::getRecordCount( void ) const noexcept
{
	return m_Header.base.nRecordCount;
}
