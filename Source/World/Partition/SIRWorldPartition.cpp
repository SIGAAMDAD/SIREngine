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

#include <World/Partition/SIRWorldPartition.h>

#include <limits.h>
#include <new>

typedef struct sirWorldCellEntry_s
{
	sirWorldCellDescriptor_t descriptor;
	uint64_t nDeadlineTick;
	uint32_t nStamp;
	sirStreamingTarget_t eTarget;
	uint8_t nPriority;
} sirWorldCellEntry_t;

static_assert( sizeof( sirWorldCellEntry_t ) % alignof( uint64_t ) == 0, "cell ID table needs aligned storage" );

// In-place heap sort. Unlike C library qsort implementations which may
// allocate scratch storage, this cannot allocate from an unknown heap.
static void siftWorldCellIds( uint64_t *pszIds, uint32_t nStart, uint32_t nEnd ) noexcept
{
	uint32_t iRoot = nStart;
	while ( true ) {
		const uint64_t nChild = ( uint64_t )iRoot * 2U + 1U;
		if ( nChild > nEnd ) {
			return;
		}
		uint32_t iSwap = iRoot;
		const uint32_t iChild = ( uint32_t )nChild;
		if ( pszIds[iSwap] < pszIds[iChild] ) {
			iSwap = iChild;
		}
		if ( iChild < nEnd && pszIds[iSwap] < pszIds[iChild + 1U] ) {
			iSwap = iChild + 1U;
		}
		if ( iSwap == iRoot ) {
			return;
		}
		const uint64_t nSaved = pszIds[iRoot];
		pszIds[iRoot] = pszIds[iSwap];
		pszIds[iSwap] = nSaved;
		iRoot = iSwap;
	}
}

static void sortWorldCellIds( uint64_t *pszIds, uint32_t nCount ) noexcept
{
	if ( nCount < 2 ) {
		return;
	}
	for ( uint32_t iStart = nCount / 2U; iStart != 0; --iStart ) {
		siftWorldCellIds( pszIds, iStart - 1U, nCount - 1U );
	}
	for ( uint32_t iEnd = nCount - 1U; iEnd != 0; --iEnd ) {
		const uint64_t nSaved = pszIds[iEnd];
		pszIds[iEnd] = pszIds[0];
		pszIds[0] = nSaved;
		siftWorldCellIds( pszIds, 0, iEnd - 1U );
	}
}

static bool worldCellBefore( const sirWorldCellDescriptor_t &left, const sirWorldCellDescriptor_t &right ) noexcept
{
	if ( left.iGridX != right.iGridX ) {
		return left.iGridX < right.iGridX;
	}
	if ( left.iGridY != right.iGridY ) {
		return left.iGridY < right.iGridY;
	}
	return left.iGridZ < right.iGridZ;
}

static uint64_t worldCoordinateDistance( int32_t iLeft, int32_t iRight ) noexcept
{
	const int64_t nDifference = ( int64_t )iLeft - ( int64_t )iRight;
	return ( uint64_t )( nDifference < 0 ? -nDifference : nDifference );
}

static bool worldCellInSource( const sirWorldCellDescriptor_t &cell, const sirWorldStreamingSource_t &source,
                               bool bHeightStratified ) noexcept
{
	const uint64_t nX = worldCoordinateDistance( cell.iGridX, source.iGridX );
	const uint64_t nY = worldCoordinateDistance( cell.iGridY, source.iGridY );
	const uint64_t nRadius = source.nRadiusCells;
	if ( nX > nRadius || nY > nRadius ) {
		return false;
	}

	const uint64_t nRadiusSquared = nRadius * nRadius;
	const uint64_t nXSquared = nX * nX;
	if ( nY * nY > nRadiusSquared - nXSquared ) {
		return false;
	}

	if ( bHeightStratified ) {
		return worldCoordinateDistance( cell.iGridZ, source.iGridZ ) <= source.nHeightRadiusCells;
	}
	return true;
}

SIRWorldPartition::SIRWorldPartition( void ) noexcept
    : m_pCells( nullptr ), m_pszCellIds( nullptr ), m_Config{}, m_nEvaluationStamp( 0 ), m_bInitialized( false )
{
}

uint64_t SIRWorldPartition::requiredBytes( uint32_t nCellCount ) noexcept
{
	return ( uint64_t )nCellCount * ( sizeof( sirWorldCellEntry_t ) + sizeof( uint64_t ) );
}

sirWorldPartitionResult_t SIRWorldPartition::initialize( void *pStorage, uint64_t nStorageBytes, const sirWorldPartitionConfig_t &config,
                                                         const sirWorldCellDescriptor_t *pCells ) noexcept
{
	if ( m_bInitialized || pStorage == nullptr || pCells == nullptr || config.nCellCount == 0 ) {
		return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
	}
	if ( config.nWorldId == 0 || config.nConsumerId == 0 || config.nManifestGeneration == 0 ) {
		return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
	}

	const uint64_t nBytes = requiredBytes( config.nCellCount );
	if ( nStorageBytes < nBytes || nBytes > SIZE_MAX || ( ( uintptr_t )pStorage % alignof( sirWorldCellEntry_t ) ) != 0 ) {
		return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
	}

	// Reject overlapping source/output arrays: building the expanded entry table
	// in place would overwrite unread cooked directory descriptors.
	const uint64_t nInputBytes = ( uint64_t )config.nCellCount * sizeof( sirWorldCellDescriptor_t );
	const uintptr_t nStorageAddress = ( uintptr_t )pStorage;
	const uintptr_t nInputAddress = ( uintptr_t )pCells;
	if ( nInputBytes > UINTPTR_MAX - nInputAddress || nBytes > UINTPTR_MAX - nStorageAddress ) {
		return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
	}
	if ( nStorageAddress < nInputAddress + nInputBytes && nInputAddress < nStorageAddress + nBytes ) {
		return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
	}

	sirWorldCellEntry_t *pEntries = ( sirWorldCellEntry_t * )pStorage;
	uint64_t *pszIds = ( uint64_t * )( ( uint8_t * )pStorage + ( uint64_t )config.nCellCount * sizeof( sirWorldCellEntry_t ) );
	for ( uint32_t iCell = 0; iCell < config.nCellCount; ++iCell ) {
		const sirWorldCellDescriptor_t &descriptor = pCells[iCell];
		if ( descriptor.nCellId == 0 || ( descriptor.nFlags & ~SIR_WORLD_CELL_FLAG_ALWAYS_LOADED ) != 0 ) {
			return SIR_WORLD_PARTITION_INVALID_DIRECTORY;
		}
		if ( !config.bHeightStratified && descriptor.iGridZ != 0 ) {
			return SIR_WORLD_PARTITION_INVALID_DIRECTORY;
		}
		if ( iCell != 0 && !worldCellBefore( pCells[iCell - 1], descriptor ) ) {
			return SIR_WORLD_PARTITION_INVALID_DIRECTORY;
		}

		new ( pEntries + iCell ) sirWorldCellEntry_t{};
		pEntries[iCell].descriptor = descriptor;
		new ( pszIds + iCell ) uint64_t( descriptor.nCellId );
	}

	sortWorldCellIds( pszIds, config.nCellCount );
	for ( uint32_t iCell = 1; iCell < config.nCellCount; ++iCell ) {
		if ( pszIds[iCell] == pszIds[iCell - 1] ) {
			return SIR_WORLD_PARTITION_INVALID_DIRECTORY;
		}
	}

	m_pCells = pEntries;
	m_pszCellIds = pszIds;
	m_Config = config;
	m_nEvaluationStamp = 0;
	m_bInitialized = true;
	return SIR_WORLD_PARTITION_OK;
}

void SIRWorldPartition::shutdown( void ) noexcept
{
	m_pCells = nullptr;
	m_pszCellIds = nullptr;
	m_Config = {};
	m_nEvaluationStamp = 0;
	m_bInitialized = false;
}

bool SIRWorldPartition::hasCell( uint64_t nCellId ) const noexcept
{
	if ( !m_bInitialized || nCellId == 0 ) {
		return false;
	}

	uint32_t nFirst = 0;
	uint32_t nLast = m_Config.nCellCount;
	while ( nFirst < nLast ) {
		const uint32_t iMiddle = nFirst + ( nLast - nFirst ) / 2;
		if ( m_pszCellIds[iMiddle] < nCellId ) {
			nFirst = iMiddle + 1;
		} else {
			nLast = iMiddle;
		}
	}
	return nFirst < m_Config.nCellCount && m_pszCellIds[nFirst] == nCellId;
}

uint32_t SIRWorldPartition::getCellCount( void ) const noexcept
{
	return m_bInitialized ? m_Config.nCellCount : 0;
}

uint32_t SIRWorldPartition::lowerBoundX( int64_t iGridX ) const noexcept
{
	uint32_t nFirst = 0;
	uint32_t nLast = m_Config.nCellCount;
	while ( nFirst < nLast ) {
		const uint32_t iMiddle = nFirst + ( nLast - nFirst ) / 2;
		if ( ( int64_t )m_pCells[iMiddle].descriptor.iGridX < iGridX ) {
			nFirst = iMiddle + 1;
		} else {
			nLast = iMiddle;
		}
	}
	return nFirst;
}

void SIRWorldPartition::accumulate( uint32_t iCell, sirStreamingTarget_t eTarget, uint8_t nPriority, uint64_t nDeadlineTick ) noexcept
{
	sirWorldCellEntry_t &entry = m_pCells[iCell];
	if ( entry.nStamp != m_nEvaluationStamp ) {
		entry.nStamp = m_nEvaluationStamp;
		entry.eTarget = eTarget;
		entry.nPriority = nPriority;
		entry.nDeadlineTick = nDeadlineTick;
		return;
	}

	if ( eTarget > entry.eTarget ) {
		entry.eTarget = eTarget;
	}
	if ( nPriority > entry.nPriority ) {
		entry.nPriority = nPriority;
	}
	if ( nDeadlineTick < entry.nDeadlineTick ) {
		entry.nDeadlineTick = nDeadlineTick;
	}
}

sirWorldPartitionResult_t SIRWorldPartition::gather( const sirWorldStreamingSource_t *pSources, uint32_t nSourceCount,
                                                     uint64_t nEnabledLayers, uint32_t nRequestEpoch, sirStreamingDemand_t *pDemands,
                                                     uint32_t nDemandCapacity, uint32_t *pRequiredCount ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_WORLD_PARTITION_NOT_READY;
	}
	if ( pRequiredCount == nullptr || nRequestEpoch == 0 || ( nSourceCount != 0 && pSources == nullptr ) ) {
		return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
	}
	for ( uint32_t iSource = 0; iSource < nSourceCount; ++iSource ) {
		const sirWorldStreamingSource_t &source = pSources[iSource];
		if ( source.nSourceId == 0 || source.nGeneration == 0 ) {
			return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
		}
		if ( source.eTarget != SIR_STREAMING_TARGET_ACTIVE && source.eTarget != SIR_STREAMING_TARGET_PREPARED ) {
			return SIR_WORLD_PARTITION_INVALID_ARGUMENT;
		}
	}

	if ( m_nEvaluationStamp == UINT32_MAX ) {
		for ( uint32_t iCell = 0; iCell < m_Config.nCellCount; ++iCell ) {
			m_pCells[iCell].nStamp = 0;
		}
		m_nEvaluationStamp = 1;
	} else {
		++m_nEvaluationStamp;
	}

	for ( uint32_t iCell = 0; iCell < m_Config.nCellCount; ++iCell ) {
		const sirWorldCellDescriptor_t &cell = m_pCells[iCell].descriptor;
		if ( ( cell.nFlags & SIR_WORLD_CELL_FLAG_ALWAYS_LOADED ) == 0 ) {
			continue;
		}
		if ( ( cell.nRequiredLayers & nEnabledLayers ) != cell.nRequiredLayers ) {
			continue;
		}
		accumulate( iCell, SIR_STREAMING_TARGET_ACTIVE, UINT8_MAX, 0 );
	}

	for ( uint32_t iSource = 0; iSource < nSourceCount; ++iSource ) {
		const sirWorldStreamingSource_t &source = pSources[iSource];
		const int64_t iMinX = ( int64_t )source.iGridX - ( int64_t )source.nRadiusCells;
		const int64_t iMaxX = ( int64_t )source.iGridX + ( int64_t )source.nRadiusCells;
		for ( uint32_t iCell = lowerBoundX( iMinX ); iCell < m_Config.nCellCount; ++iCell ) {
			const sirWorldCellDescriptor_t &cell = m_pCells[iCell].descriptor;
			if ( ( int64_t )cell.iGridX > iMaxX ) {
				break;
			}
			if ( ( cell.nRequiredLayers & nEnabledLayers ) != cell.nRequiredLayers ) {
				continue;
			}
			if ( !worldCellInSource( cell, source, m_Config.bHeightStratified ) ) {
				continue;
			}
			accumulate( iCell, source.eTarget, source.nPriority, source.nDeadlineTick );
		}
	}

	uint32_t nSelected = 0;
	for ( uint32_t iCell = 0; iCell < m_Config.nCellCount; ++iCell ) {
		if ( m_pCells[iCell].nStamp == m_nEvaluationStamp ) {
			++nSelected;
		}
	}
	*pRequiredCount = nSelected;
	if ( nSelected > nDemandCapacity || ( nSelected != 0 && pDemands == nullptr ) ) {
		return SIR_WORLD_PARTITION_OUT_OF_CAPACITY;
	}

	uint32_t iDemand = 0;
	for ( uint32_t iCell = 0; iCell < m_Config.nCellCount; ++iCell ) {
		const sirWorldCellEntry_t &entry = m_pCells[iCell];
		if ( entry.nStamp != m_nEvaluationStamp ) {
			continue;
		}
		sirStreamingDemand_t &demand = pDemands[iDemand++];
		demand = {};
		demand.keyUnit.nContainerId = m_Config.nWorldId;
		demand.keyUnit.nUnitId = entry.descriptor.nCellId;
		demand.keyUnit.nManifestGeneration = m_Config.nManifestGeneration;
		demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
		demand.nConsumerId = m_Config.nConsumerId;
		demand.nConsumerGeneration = m_Config.nManifestGeneration;
		demand.nRequestEpoch = nRequestEpoch;
		demand.nDeadlineTick = entry.nDeadlineTick;
		demand.eTarget = entry.eTarget;
		demand.nPriority = entry.nPriority;
	}
	return SIR_WORLD_PARTITION_OK;
}
