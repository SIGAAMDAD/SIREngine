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

#include <Resources/SIRResourceCatalog.h>

#include <string.h>

sirResourceCatalogResult_t SIRResourceCatalog::initialize( sirResourceEntry_t *pEntries, uint32_t nEntryCapacity,
                                                           sirResourceDependency_t *pDependencies, uint32_t nDependencyCapacity,
                                                           uint32_t *pScratch, uint8_t *pSeen ) noexcept
{
	if ( m_pEntries != nullptr || pEntries == nullptr || pDependencies == nullptr || pScratch == nullptr || pSeen == nullptr ||
	     nEntryCapacity == 0 || nDependencyCapacity == 0 ) {
		return SIR_RESOURCE_CATALOG_INVALID_ARGUMENT;
	}
	if ( nEntryCapacity > UINT32_MAX / sizeof( *pEntries ) || nDependencyCapacity > UINT32_MAX / sizeof( *pDependencies ) ) {
		return SIR_RESOURCE_CATALOG_INVALID_ARGUMENT;
	}
	m_pEntries = pEntries;
	m_pDependencies = pDependencies;
	m_pScratch = pScratch;
	m_pSeen = pSeen;
	m_nEntryCapacity = nEntryCapacity;
	m_nDependencyCapacity = nDependencyCapacity;
	m_nEntryCount = 0;
	m_nDependencyCount = 0;
	m_bSealed = false;
	return SIR_RESOURCE_CATALOG_OK;
}

void SIRResourceCatalog::shutdown( void ) noexcept
{
	m_pEntries = nullptr;
	m_pDependencies = nullptr;
	m_pScratch = nullptr;
	m_pSeen = nullptr;
	m_nEntryCapacity = 0;
	m_nDependencyCapacity = 0;
	m_nEntryCount = 0;
	m_nDependencyCount = 0;
	m_bSealed = false;
}

uint32_t SIRResourceCatalog::findIndex( uint64_t nId ) const noexcept
{
	for ( uint32_t iEntry = 0; iEntry < m_nEntryCount; ++iEntry ) {
		if ( m_pEntries[iEntry].nId == nId ) {
			return iEntry;
		}
	}
	return UINT32_MAX;
}

sirResourceCatalogResult_t SIRResourceCatalog::addResource( uint64_t nId, uint32_t nTypeId, uint64_t nPathAtom,
                                                            sirResourceSource_t eSource ) noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_RESOURCE_CATALOG_UNINITIALIZED;
	}
	if ( m_bSealed ) {
		return SIR_RESOURCE_CATALOG_SEALED;
	}
	if ( nId == 0 || nTypeId == 0 || ( eSource != SIR_RESOURCE_SOURCE_EXTERNAL && eSource != SIR_RESOURCE_SOURCE_EMBEDDED ) ||
	     ( eSource == SIR_RESOURCE_SOURCE_EXTERNAL && nPathAtom == 0 ) ) {
		return SIR_RESOURCE_CATALOG_INVALID_ARGUMENT;
	}
	if ( findIndex( nId ) != UINT32_MAX ) {
		return SIR_RESOURCE_CATALOG_DUPLICATE;
	}
	if ( m_nEntryCount == m_nEntryCapacity ) {
		return SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY;
	}
	m_pEntries[m_nEntryCount++] = { nId, nPathAtom, nTypeId, 1, eSource, {} };
	return SIR_RESOURCE_CATALOG_OK;
}

bool SIRResourceCatalog::canReach( uint32_t iStart, uint32_t iTarget ) noexcept
{
	memset( m_pSeen, 0, m_nEntryCount );
	uint32_t nTop = 0;
	m_pSeen[iStart] = 1;
	m_pScratch[nTop++] = iStart;
	while ( nTop > 0 ) {
		const uint32_t iNode = m_pScratch[--nTop];
		if ( iNode == iTarget ) {
			return true;
		}
		for ( uint32_t iDependency = 0; iDependency < m_nDependencyCount; ++iDependency ) {
			const sirResourceDependency_t &edge = m_pDependencies[iDependency];
			if ( edge.nOwnerId != m_pEntries[iNode].nId ) {
				continue;
			}
			const uint32_t iNext = findIndex( edge.nTargetId );
			if ( iNext != UINT32_MAX && m_pSeen[iNext] == 0 ) {
				m_pSeen[iNext] = 1;
				m_pScratch[nTop++] = iNext;
			}
		}
	}
	return false;
}

sirResourceCatalogResult_t SIRResourceCatalog::addDependency( uint64_t nOwnerId, uint64_t nTargetId ) noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_RESOURCE_CATALOG_UNINITIALIZED;
	}
	if ( m_bSealed ) {
		return SIR_RESOURCE_CATALOG_SEALED;
	}
	const uint32_t iOwner = findIndex( nOwnerId );
	const uint32_t iTarget = findIndex( nTargetId );
	if ( nOwnerId == 0 || nTargetId == 0 || iOwner == UINT32_MAX || iTarget == UINT32_MAX ) {
		return SIR_RESOURCE_CATALOG_NOT_FOUND;
	}
	if ( iOwner == iTarget ) {
		return SIR_RESOURCE_CATALOG_CYCLIC;
	}
	for ( uint32_t iDependency = 0; iDependency < m_nDependencyCount; ++iDependency ) {
		const sirResourceDependency_t &edge = m_pDependencies[iDependency];
		if ( edge.nOwnerId == nOwnerId && edge.nTargetId == nTargetId ) {
			return SIR_RESOURCE_CATALOG_DUPLICATE;
		}
	}
	if ( m_nDependencyCount == m_nDependencyCapacity ) {
		return SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY;
	}
	if ( canReach( iTarget, iOwner ) ) {
		return SIR_RESOURCE_CATALOG_CYCLIC;
	}
	m_pDependencies[m_nDependencyCount++] = { nOwnerId, nTargetId };
	return SIR_RESOURCE_CATALOG_OK;
}

// Compute a deterministic dependency-first resource order. All temporary
// counts use the catalog's existing editor-domain scratch; no loader bytes
// or additional catalog authority is allocated.
sirResourceCatalogResult_t SIRResourceCatalog::buildLoadOrder( uint64_t *pOutIds, uint32_t nCapacity,
                                                               uint32_t *pOutCount ) noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_RESOURCE_CATALOG_UNINITIALIZED;
	}
	if ( pOutCount == nullptr || ( pOutIds == nullptr && nCapacity != 0 ) ) {
		return SIR_RESOURCE_CATALOG_INVALID_ARGUMENT;
	}
	*pOutCount = m_nEntryCount;
	if ( pOutIds == nullptr && nCapacity == 0 ) {
		return SIR_RESOURCE_CATALOG_OK;
	}
	if ( nCapacity < m_nEntryCount ) {
		return SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY;
	}
	// Validate catalog edges before touching user output.
	for ( uint32_t iEdge = 0; iEdge < m_nDependencyCount; ++iEdge ) {
		const sirResourceDependency_t& edge = m_pDependencies[iEdge];
		if ( findIndex( edge.nOwnerId ) == UINT32_MAX || findIndex( edge.nTargetId ) == UINT32_MAX ) {
			return SIR_RESOURCE_CATALOG_NOT_FOUND;
		}
	}
	// First validate a complete topological traversal. Only the second
	// pass writes caller output, preserving it if the catalog was corrupted.
	for ( uint32_t iPass = 0; iPass < 2; ++iPass ) {
		memset( m_pSeen, 0, m_nEntryCount );
		memset( m_pScratch, 0, ( size_t )m_nEntryCount * sizeof( uint32_t ) );
		for ( uint32_t iEdge = 0; iEdge < m_nDependencyCount; ++iEdge ) {
			const uint32_t iOwner = findIndex( m_pDependencies[iEdge].nOwnerId );
			if ( m_pScratch[iOwner] == UINT32_MAX ) {
				return SIR_RESOURCE_CATALOG_OUT_OF_CAPACITY;
			}
			++m_pScratch[iOwner];
		}
		for ( uint32_t iOutput = 0; iOutput < m_nEntryCount; ++iOutput ) {
			uint32_t iNext = UINT32_MAX;
			for ( uint32_t iResource = 0; iResource < m_nEntryCount; ++iResource ) {
				if ( m_pSeen[iResource] == 0 && m_pScratch[iResource] == 0 &&
				     ( iNext == UINT32_MAX || m_pEntries[iResource].nId < m_pEntries[iNext].nId ) ) {
					iNext = iResource;
				}
			}
			if ( iNext == UINT32_MAX ) {
				return SIR_RESOURCE_CATALOG_CYCLIC;
			}
			const uint64_t nCompletedId = m_pEntries[iNext].nId;
			m_pSeen[iNext] = 1;
			if ( iPass == 1 ) {
				pOutIds[iOutput] = nCompletedId;
			}
			for ( uint32_t iEdge = 0; iEdge < m_nDependencyCount; ++iEdge ) {
				const sirResourceDependency_t& edge = m_pDependencies[iEdge];
				if ( edge.nTargetId == nCompletedId ) {
					const uint32_t iOwner = findIndex( edge.nOwnerId );
					if ( m_pScratch[iOwner] == 0 ) {
						return SIR_RESOURCE_CATALOG_CYCLIC;
					}
					--m_pScratch[iOwner];
				}
			}
		}
	}
	return SIR_RESOURCE_CATALOG_OK;
}

sirResourceCatalogResult_t SIRResourceCatalog::findResource( uint64_t nId, sirResourceEntry_t *pOutEntry ) const noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_RESOURCE_CATALOG_UNINITIALIZED;
	}
	if ( pOutEntry == nullptr || nId == 0 ) {
		return SIR_RESOURCE_CATALOG_INVALID_ARGUMENT;
	}
	const uint32_t iEntry = findIndex( nId );
	if ( iEntry == UINT32_MAX ) {
		return SIR_RESOURCE_CATALOG_NOT_FOUND;
	}
	*pOutEntry = m_pEntries[iEntry];
	return SIR_RESOURCE_CATALOG_OK;
}

sirResourceCatalogResult_t SIRResourceCatalog::dependencyAt( uint32_t iDependency, sirResourceDependency_t *pOutDependency ) const noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_RESOURCE_CATALOG_UNINITIALIZED;
	}
	if ( pOutDependency == nullptr || iDependency >= m_nDependencyCount ) {
		return SIR_RESOURCE_CATALOG_INVALID_ARGUMENT;
	}
	*pOutDependency = m_pDependencies[iDependency];
	return SIR_RESOURCE_CATALOG_OK;
}

sirResourceCatalogResult_t SIRResourceCatalog::seal( void ) noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_RESOURCE_CATALOG_UNINITIALIZED;
	}
	m_bSealed = true;
	return SIR_RESOURCE_CATALOG_OK;
}

uint32_t SIRResourceCatalog::getResourceCount( void ) const noexcept
{
	return m_nEntryCount;
}

uint32_t SIRResourceCatalog::getDependencyCount( void ) const noexcept
{
	return m_nDependencyCount;
}
