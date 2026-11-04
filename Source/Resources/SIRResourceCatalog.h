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

#ifndef __SIRENGINE_SIRRESOURCECATALOG_H__
#define __SIRENGINE_SIRRESOURCECATALOG_H__

#include <SIRPCH.hpp>

typedef enum sirResourceCatalogResult_e : uint8_t
{
	SIR_RESOURCE_CATALOG_OK = 0,
	SIR_RESOURCE_CATALOG_INVALID_ARGUMENT = 1,
	SIR_RESOURCE_CATALOG_UNINITIALIZED = 2,
	SIR_RESOURCE_CATALOG_DUPLICATE = 3,
	SIR_RESOURCE_CATALOG_NOT_FOUND = 4,
	SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY = 5,
	SIR_RESOURCE_CATALOG_CYCLIC = 6,
	SIR_RESOURCE_CATALOG_SEALED = 7
} sirResourceCatalogResult_t;

typedef enum sirResourceSource_e : uint8_t
{
	SIR_RESOURCE_SOURCE_EXTERNAL = 1,
	SIR_RESOURCE_SOURCE_EMBEDDED = 2
} sirResourceSource_t;

typedef struct sirResourceEntry_s
{
	uint64_t nId;
	uint64_t nPathAtom;
	uint32_t nTypeId;
	uint32_t nGeneration;
	sirResourceSource_t eSource;
	uint8_t szReserved[7];
} sirResourceEntry_t;

typedef struct sirResourceDependency_s
{
	uint64_t nOwnerId;
	uint64_t nTargetId;
} sirResourceDependency_t;

// Resource metadata/catalog ownership; actual bytes remain owned by the
// engine resource loader and eventually GRDG, not this authoring index.
// All arrays are supplied by a SIREngine memory domain; no heap fallback.
class SIRResourceCatalog
{
private:
	sirResourceEntry_t *m_pEntries = nullptr;
	sirResourceDependency_t *m_pDependencies = nullptr;
	uint32_t *m_pScratch = nullptr;
	uint8_t *m_pSeen = nullptr;
	uint32_t m_nEntryCapacity = 0;
	uint32_t m_nDependencyCapacity = 0;
	uint32_t m_nEntryCount = 0;
	uint32_t m_nDependencyCount = 0;
	bool m_bSealed = false;

	uint32_t findIndex( uint64_t nId ) const noexcept;
	bool canReach( uint32_t iStart, uint32_t iTarget ) noexcept;

public:
	sirResourceCatalogResult_t initialize( sirResourceEntry_t *pEntries, uint32_t nEntryCapacity, sirResourceDependency_t *pDependencies,
	                                       uint32_t nDependencyCapacity, uint32_t *pScratch, uint8_t *pSeen ) noexcept;
	void shutdown( void ) noexcept;
	sirResourceCatalogResult_t addResource( uint64_t nId, uint32_t nTypeId, uint64_t nPathAtom, sirResourceSource_t eSource ) noexcept;
	sirResourceCatalogResult_t addDependency( uint64_t nOwnerId, uint64_t nTargetId ) noexcept;
	sirResourceCatalogResult_t seal( void ) noexcept;
	sirResourceCatalogResult_t buildLoadOrder( uint64_t *pOutIds, uint32_t nCapacity, uint32_t *pOutCount ) noexcept;
	sirResourceCatalogResult_t findResource( uint64_t nId, sirResourceEntry_t *pOutEntry ) const noexcept;
	sirResourceCatalogResult_t dependencyAt( uint32_t iDependency, sirResourceDependency_t *pOutDependency ) const noexcept;
	uint32_t getResourceCount( void ) const noexcept;
	uint32_t getDependencyCount( void ) const noexcept;
};

#endif // __SIRENGINE_SIRRESOURCECATALOG_H__
