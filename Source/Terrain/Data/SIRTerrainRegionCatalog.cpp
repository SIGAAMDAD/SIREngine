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

#include <Terrain/Data/SIRTerrainRegionCatalog.h>

static constexpr uint8_t SIR_TERRAIN_SLOT_EMPTY = 0;
static constexpr uint8_t SIR_TERRAIN_SLOT_OCCUPIED = 1;
static constexpr uint8_t SIR_TERRAIN_SLOT_TOMBSTONE = 2;

SIRTerrainRegionCatalog::SIRTerrainRegionCatalog( void ) noexcept : m_pEntries( nullptr ), m_nCapacity( 0 ), m_nCount( 0 )
{
}

uint32_t SIRTerrainRegionCatalog::getCount( void ) const noexcept
{
	return m_nCount;
}

uint32_t SIRTerrainRegionCatalog::getCapacity( void ) const noexcept
{
	return m_nCapacity;
}

uint32_t SIRTerrainRegionCatalog::hashKey( sirTerrainRegionKey_t key ) noexcept
{
	uint32_t nX = ( uint32_t )key.iRegionX * 0x9E3779B1u;
	uint32_t nZ = ( uint32_t )key.iRegionZ * 0x85EBCA77u;
	uint32_t nHash = nX ^ ( ( nZ << 16u ) | ( nZ >> 16u ) );
	nHash ^= nHash >> 16u;
	nHash *= 0x7FEB352Du;
	nHash ^= nHash >> 15u;
	return nHash;
}

uint32_t SIRTerrainRegionCatalog::nextGeneration( uint32_t nGeneration ) noexcept
{
	++nGeneration;
	return nGeneration ? nGeneration : 1;
}

bool SIRTerrainRegionCatalog::initialize( sirTerrainRegionCatalogEntry_t *pEntries, uint32_t nCapacity ) noexcept
{
	if ( m_pEntries || !pEntries || nCapacity < 2 || ( nCapacity & ( nCapacity - 1 ) ) != 0 ) {
		return false;
	}
	m_pEntries = pEntries;
	m_nCapacity = nCapacity;
	m_nCount = 0;
	for ( uint32_t nSlot = 0; nSlot < m_nCapacity; ++nSlot ) {
		m_pEntries[nSlot].region = {};
		m_pEntries[nSlot].nState = SIR_TERRAIN_SLOT_EMPTY;
		m_pEntries[nSlot].nGeneration = 1;
	}
	return true;
}

void SIRTerrainRegionCatalog::reset( void ) noexcept
{
	if ( !m_pEntries ) {
		return;
	}
	for ( uint32_t nSlot = 0; nSlot < m_nCapacity; ++nSlot ) {
		sirTerrainRegionCatalogEntry_t& entry = m_pEntries[nSlot];
		entry.region = {};
		entry.nState = SIR_TERRAIN_SLOT_EMPTY;
		entry.nGeneration = nextGeneration( entry.nGeneration );
	}
	m_nCount = 0;
}

sirTerrainCatalogResult_t SIRTerrainRegionCatalog::upsert( const sirTerrainRegionDescriptor_t& descriptor,
                                                           sirTerrainRegionHandle_t& handle ) noexcept
{
	if ( !m_pEntries ) {
		return SIR_TERRAIN_CATALOG_NOT_INITIALIZED;
	}
	if ( !SIRTerrainHeightfieldCodec::isValidDescriptor( descriptor ) ) {
		return SIR_TERRAIN_CATALOG_INVALID_ARGUMENT;
	}
	const sirTerrainRegionKey_t key = { descriptor.iRegionX, descriptor.iRegionZ };
	const uint32_t nMask = m_nCapacity - 1;
	uint32_t nSlot = hashKey( key ) & nMask;
	uint32_t nReusable = m_nCapacity;
	for ( uint32_t nProbe = 0; nProbe < m_nCapacity; ++nProbe ) {
		sirTerrainRegionCatalogEntry_t& entry = m_pEntries[nSlot];
		if ( entry.nState == SIR_TERRAIN_SLOT_EMPTY ) {
			if ( nReusable != m_nCapacity ) {
				nSlot = nReusable;
			}
			break;
		}
		if ( entry.nState == SIR_TERRAIN_SLOT_TOMBSTONE ) {
			if ( nReusable == m_nCapacity ) {
				nReusable = nSlot;
			}
		} else if ( entry.region.iRegionX == descriptor.iRegionX && entry.region.iRegionZ == descriptor.iRegionZ ) {
			if ( descriptor.nRevision < entry.region.nRevision ) {
				return SIR_TERRAIN_CATALOG_STALE_REVISION;
			}
			if ( descriptor.nRevision == entry.region.nRevision ) {
				if ( descriptor.nResolution != entry.region.nResolution || descriptor.fMinHeight != entry.region.fMinHeight ||
				     descriptor.fMaxHeight != entry.region.fMaxHeight ) {
					return SIR_TERRAIN_CATALOG_REVISION_CONFLICT;
				}
			} else {
				entry.nGeneration = nextGeneration( entry.nGeneration );
				entry.region = descriptor;
			}
			handle = { nSlot, entry.nGeneration };
			return SIR_TERRAIN_CATALOG_SUCCESS;
		}
		nSlot = ( nSlot + 1 ) & nMask;
	}
	if ( m_nCount == m_nCapacity ) {
		return SIR_TERRAIN_CATALOG_FULL;
	}
	if ( nReusable != m_nCapacity && m_pEntries[nSlot].nState != SIR_TERRAIN_SLOT_EMPTY ) {
		nSlot = nReusable;
	}
	sirTerrainRegionCatalogEntry_t& entry = m_pEntries[nSlot];
	entry.region = descriptor;
	entry.nState = SIR_TERRAIN_SLOT_OCCUPIED;
	++m_nCount;
	handle = { nSlot, entry.nGeneration };
	return SIR_TERRAIN_CATALOG_SUCCESS;
}

sirTerrainCatalogResult_t SIRTerrainRegionCatalog::find( sirTerrainRegionKey_t key, sirTerrainRegionDescriptor_t& descriptor,
                                                         sirTerrainRegionHandle_t& handle ) const noexcept
{
	if ( !m_pEntries ) {
		return SIR_TERRAIN_CATALOG_NOT_INITIALIZED;
	}
	const uint32_t nMask = m_nCapacity - 1;
	uint32_t nSlot = hashKey( key ) & nMask;
	for ( uint32_t nProbe = 0; nProbe < m_nCapacity; ++nProbe ) {
		const sirTerrainRegionCatalogEntry_t& entry = m_pEntries[nSlot];
		if ( entry.nState == SIR_TERRAIN_SLOT_EMPTY ) {
			break;
		}
		if ( entry.nState == SIR_TERRAIN_SLOT_OCCUPIED && entry.region.iRegionX == key.iRegionX && entry.region.iRegionZ == key.iRegionZ ) {
			descriptor = entry.region;
			handle = { nSlot, entry.nGeneration };
			return SIR_TERRAIN_CATALOG_SUCCESS;
		}
		nSlot = ( nSlot + 1 ) & nMask;
	}
	return SIR_TERRAIN_CATALOG_NOT_FOUND;
}

sirTerrainCatalogResult_t SIRTerrainRegionCatalog::find( sirTerrainRegionHandle_t handle,
                                                         sirTerrainRegionDescriptor_t& descriptor ) const noexcept
{
	if ( !m_pEntries ) {
		return SIR_TERRAIN_CATALOG_NOT_INITIALIZED;
	}
	if ( handle.nSlot >= m_nCapacity || !handle.nGeneration ) {
		return SIR_TERRAIN_CATALOG_STALE_HANDLE;
	}
	const sirTerrainRegionCatalogEntry_t& entry = m_pEntries[handle.nSlot];
	if ( entry.nState != SIR_TERRAIN_SLOT_OCCUPIED || entry.nGeneration != handle.nGeneration ) {
		return SIR_TERRAIN_CATALOG_STALE_HANDLE;
	}
	descriptor = entry.region;
	return SIR_TERRAIN_CATALOG_SUCCESS;
}

sirTerrainCatalogResult_t SIRTerrainRegionCatalog::erase( sirTerrainRegionHandle_t handle ) noexcept
{
	if ( !m_pEntries ) {
		return SIR_TERRAIN_CATALOG_NOT_INITIALIZED;
	}
	if ( handle.nSlot >= m_nCapacity || !handle.nGeneration ) {
		return SIR_TERRAIN_CATALOG_STALE_HANDLE;
	}
	sirTerrainRegionCatalogEntry_t& entry = m_pEntries[handle.nSlot];
	if ( entry.nState != SIR_TERRAIN_SLOT_OCCUPIED || entry.nGeneration != handle.nGeneration ) {
		return SIR_TERRAIN_CATALOG_STALE_HANDLE;
	}
	entry.region = {};
	entry.nState = SIR_TERRAIN_SLOT_TOMBSTONE;
	entry.nGeneration = nextGeneration( entry.nGeneration );
	--m_nCount;
	return SIR_TERRAIN_CATALOG_SUCCESS;
}
