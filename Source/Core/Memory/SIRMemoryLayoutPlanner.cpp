/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Memory/SIRMemoryLayoutPlanner.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Platform/SIRVirtualMemory.h>
#include <limits>

static bool sirAddLayoutBytes( uint64_t *pTotal, uint64_t nBytes )
{
	if ( nBytes > ( std::numeric_limits<uint64_t>::max )() - *pTotal ) {
		return false;
	}
	*pTotal += nBytes;
	return true;
}

bool SIRMemoryLayoutPlanner::preflight( const sirMemoryConfig_t &config, const sirMemoryBudgetProfile_t &budget,
                                        const sirMemoryLayoutEntry_t *pEntries, uint32_t nEntryCount, sirMemoryLayoutResult_t *pResult )
{
	if ( pResult == nullptr || ( pEntries == nullptr && nEntryCount != 0 ) || nEntryCount > config.nMaxSegments ||
	     config.nMaxSegments == 0 ) {
		return false;
	}
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	uint64_t nReserve = 0;
	uint64_t nMetadata = 0;
	if ( !SIR_IS_POWER_OF_TWO( nPage ) ) {
		return false;
	}
	if ( !SIR_IS_POWER_OF_TWO( nPage ) || ( uint64_t )( nPage ) > UINTPTR_MAX ||
	     ( uint64_t )( config.nReserveBytes ) > UINTPTR_MAX - ( ( uint64_t )( nPage )-1 ) ) {
		return false;
	}
	nReserve = SIR_ALIGN_UP( config.nReserveBytes, nPage );
	if ( !SIR_IS_POWER_OF_TWO( nPage ) || ( uint64_t )( nPage ) > UINTPTR_MAX ||
	     ( uint64_t )( config.nMetadataBytes ) > UINTPTR_MAX - ( ( uint64_t )( nPage )-1 ) ) {
		return false;
	}
	nMetadata = SIR_ALIGN_UP( config.nMetadataBytes, nPage );
	if ( nMetadata == 0 || nReserve <= nMetadata || budget.nInitialCommitBudgetBytes == 0 ||
	     budget.nInitialCommitBudgetBytes > budget.nMaximumCommitBudgetBytes || budget.nMaximumCommitBudgetBytes > nReserve ||
	     ( budget.nInitialCommitBudgetBytes % nPage ) != 0 || ( budget.nMaximumCommitBudgetBytes % nPage ) != 0 ) {
		return false;
	}
	uint64_t nRequiredMetadata = ( ( uint64_t )( config.nMaxSegments ) ) * sizeof( SIRMemorySegment );
	uint64_t nTotalData = 0;
	for ( uint32_t i = 0; i < nEntryCount; ++i ) {
		const sirMemoryLayoutEntry_t &entry = pEntries[i];
		uint64_t nBytes = 0;
		if ( entry.nReserveBytes == 0 ) {
			return false;
		}
		if ( !SIR_IS_POWER_OF_TWO( nPage ) || ( uint64_t )( nPage ) > UINTPTR_MAX ||
		     ( uint64_t )( entry.nReserveBytes ) > UINTPTR_MAX - ( ( uint64_t )( nPage )-1 ) ) {
			return false;
		}
		nBytes = SIR_ALIGN_UP( entry.nReserveBytes, nPage );
		if ( ( entry.bUse32BitOffsets && nBytes > UINT32_MAX ) || ( entry.bBuddyPowerOfTwo && !SIR_IS_POWER_OF_TWO( nBytes ) ) ) {
			return false;
		}
		uint64_t nLimit = nBytes;
		if ( entry.nCommitLimitBytes != 0 ) {
			if ( !SIR_IS_POWER_OF_TWO( nPage ) || ( uint64_t )( nPage ) > UINTPTR_MAX ||
			     ( uint64_t )( entry.nCommitLimitBytes ) > UINTPTR_MAX - ( ( uint64_t )( nPage )-1 ) ) {
				return false;
			}
			nLimit = SIR_ALIGN_UP( entry.nCommitLimitBytes, nPage );
			if ( nLimit > nBytes ) {
				return false;
			}
		}
		const uint64_t nPageCount = nBytes / nPage;
		const uint64_t nWords = nPageCount / 64 + ( nPageCount % 64 != 0 );
		if ( !sirAddLayoutBytes( &nTotalData, nBytes ) || !sirAddLayoutBytes( &nRequiredMetadata, nWords * 8 + 8 ) ||
		     !sirAddLayoutBytes( &nRequiredMetadata, entry.nAllocatorMetadataBytes ) || !sirAddLayoutBytes( &nRequiredMetadata, 64 ) ) {
			return false;
		}
	}
	if ( nRequiredMetadata > nMetadata || nTotalData > nReserve - nMetadata || nRequiredMetadata > budget.nInitialCommitBudgetBytes ) {
		return false;
	}
	*pResult = { nTotalData, nRequiredMetadata, nReserve - nMetadata - nTotalData, nEntryCount };
	return true;
}

uint64_t SIRMemoryLayoutPlanner::estimateCompactBucketMetadata( uint64_t nSegmentBytes, uint32_t nSlabBytes, uint32_t nWorkerSeats,
                                                                uint32_t nAlignment )
{
	const sirCompactBucketConfig_t config = { 2048, nAlignment, nSlabBytes, nWorkerSeats, 0 };
	return SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, config );
}

uint64_t SIRMemoryLayoutPlanner::estimateCompactBucketColdMetadata( uint64_t nSegmentBytes, uint32_t nSlabBytes, uint32_t nWorkerSeats,
                                                                    uint32_t nAlignment )
{
	const sirCompactBucketConfig_t config = { 2048, nAlignment, nSlabBytes, nWorkerSeats, 0 };
	if ( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, config ) == 0 ) {
		return 0;
	}
	// The state-block pool is empty after init; its high-water bound is
	// included in the conservative estimateCompactBucketMetadata result.
	const uint64_t nSlabs = nSegmentBytes / nSlabBytes;
	const uint64_t nSlotsPerSlab = nSlabBytes / nAlignment;
	const uint64_t nStateUpper = nSlabs * ( ( ( ( ( nSlotsPerSlab + 31 ) / 32 ) * 8 + 63 ) & ~63ull ) + 64 );
	return estimateCompactBucketMetadata( nSegmentBytes, nSlabBytes, nWorkerSeats, nAlignment ) - nStateUpper;
}
