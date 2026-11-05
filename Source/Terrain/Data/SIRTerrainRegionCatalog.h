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

#ifndef __SIRENGINE_SIRTERRAINREGIONCATALOG_H__
#define __SIRENGINE_SIRTERRAINREGIONCATALOG_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Terrain/Data/SIRTerrainHeightfieldCodec.h>

typedef struct sirTerrainRegionKey_s
{
	int32_t iRegionX;
	int32_t iRegionZ;
} sirTerrainRegionKey_t;

typedef struct sirTerrainRegionHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirTerrainRegionHandle_t;

typedef struct sirTerrainRegionCatalogEntry_s
{
	sirTerrainRegionDescriptor_t region;
	uint32_t nGeneration;
	uint8_t nState;
} sirTerrainRegionCatalogEntry_t;

typedef enum sirTerrainCatalogResult_e : uint8_t
{
	SIR_TERRAIN_CATALOG_SUCCESS = 0,
	SIR_TERRAIN_CATALOG_INVALID_ARGUMENT,
	SIR_TERRAIN_CATALOG_NOT_INITIALIZED,
	SIR_TERRAIN_CATALOG_FULL,
	SIR_TERRAIN_CATALOG_NOT_FOUND,
	SIR_TERRAIN_CATALOG_STALE_REVISION,
	SIR_TERRAIN_CATALOG_REVISION_CONFLICT,
	SIR_TERRAIN_CATALOG_STALE_HANDLE
} sirTerrainCatalogResult_t;

// The caller owns the entire entry array and its lifetime. One writer at a time.
// Readers receive metadata copies; physical region pages remain GRDG-owned.
class SIRTerrainRegionCatalog
{
public:
	SIRTerrainRegionCatalog( void ) noexcept;

	uint32_t getCount( void ) const noexcept;
	uint32_t getCapacity( void ) const noexcept;

	bool initialize( sirTerrainRegionCatalogEntry_t *pEntries, uint32_t nCapacity ) noexcept;
	void reset( void ) noexcept;
	sirTerrainCatalogResult_t upsert( const sirTerrainRegionDescriptor_t& descriptor, sirTerrainRegionHandle_t& handle ) noexcept;
	sirTerrainCatalogResult_t find( sirTerrainRegionKey_t key, sirTerrainRegionDescriptor_t& descriptor,
	                                sirTerrainRegionHandle_t& handle ) const noexcept;
	sirTerrainCatalogResult_t find( sirTerrainRegionHandle_t handle, sirTerrainRegionDescriptor_t& descriptor ) const noexcept;
	sirTerrainCatalogResult_t erase( sirTerrainRegionHandle_t handle ) noexcept;

protected:
private:
	static uint32_t hashKey( sirTerrainRegionKey_t key ) noexcept;
	static uint32_t nextGeneration( uint32_t nGeneration ) noexcept;

	sirTerrainRegionCatalogEntry_t *m_pEntries;
	uint32_t m_nCapacity;
	uint32_t m_nCount;
};

#endif // __SIRENGINE_SIRTERRAINREGIONCATALOG_H__
