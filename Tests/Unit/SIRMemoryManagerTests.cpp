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

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool testMemoryBudget( void )
{
	const uint64_t nPageSize = SIRVirtualMemory::getPageSize();
	if ( nPageSize == 0 ) {
		return false;
	}

	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPageSize * 256, nPageSize * 64, nPageSize * 8, 8 };
	if ( !memory.init( config ) || !memory.isInitialized() ) {
		return false;
	}

	SIRMemorySegment *pSegment = memory.createSegment( "stream", nPageSize * 32, nPageSize * 16, SIR_MEMORY_DOMAIN_STREAMING );
	if ( pSegment == nullptr || !memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_STREAMING, nPageSize * 8 ) ) {
		return false;
	}
	if ( !pSegment->commit( 0, nPageSize * 8 ) || pSegment->commit( nPageSize * 8, nPageSize ) ) {
		return false;
	}
	if ( pSegment->getCommittedBytes() != nPageSize * 8 ) {
		return false;
	}

	const sirMemoryDomainStats_t stats = memory.getDomainStats( SIR_MEMORY_DOMAIN_STREAMING );
	if ( stats.nCommittedBytes != nPageSize * 8 || stats.nCommitLimitBytes != nPageSize * 8 ) {
		return false;
	}
	if ( pSegment->commit( 1, nPageSize ) || pSegment->decommit( nPageSize * 40, nPageSize ) ) {
		return false;
	}
	if ( !pSegment->decommit( 0, nPageSize * 8 ) || memory.getDomainStats( SIR_MEMORY_DOMAIN_STREAMING ).nCommittedBytes != 0 ) {
		return false;
	}

	if ( !memory.initTraceBuffer( 32 ) || !pSegment->commit( 0, nPageSize ) ) {
		return false;
	}
	sirMemoryTraceEvent_t szEvents[32] = {};
	const uint32_t nEvents = memory.copyTrace( szEvents, 32 );
	if ( nEvents == 0 || szEvents[nEvents - 1].nSequence == 0 ) {
		return false;
	}
	memory.shutdown();
	return !memory.isInitialized();
}

static bool testCoreDomainMetadata( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 48, nPage * 12, 8 };
	if ( !memory.init( config ) ) {
		return false;
	}

	const sirMemoryDomainStats_t core = memory.getDomainStats( SIR_MEMORY_DOMAIN_CORE );
	const sirMemoryDomainStats_t gameplay = memory.getDomainStats( SIR_MEMORY_DOMAIN_GAMEPLAY );
	if ( core.nCommittedBytes == 0 || gameplay.nCommittedBytes != 0 ) {
		return false;
	}

	SIRMemorySegment *pSegment = memory.createSegment( "physics", nPage * 8, 0, SIR_MEMORY_DOMAIN_PHYSICS );
	if ( pSegment == nullptr || !memory.initTraceBuffer( 1024 ) ) {
		return false;
	}
	if ( memory.getDomainStats( SIR_MEMORY_DOMAIN_CORE ).nCommittedBytes <= core.nCommittedBytes ) {
		return false;
	}
	if ( memory.getDomainStats( SIR_MEMORY_DOMAIN_GAMEPLAY ).nCommittedBytes != 0 ) {
		return false;
	}

	void *pPage = memory.allocateRecyclableMetadataPage( SIR_MEMORY_DOMAIN_NETWORKING );
	if ( pPage == nullptr || !memory.releaseRecyclableMetadataPage( pPage, SIR_MEMORY_DOMAIN_NETWORKING ) ) {
		return false;
	}
	if ( memory.releaseRecyclableMetadataPage( pPage, SIR_MEMORY_DOMAIN_NETWORKING ) ) {
		return false;
	}
	void *pReused = memory.allocateRecyclableMetadataPage( SIR_MEMORY_DOMAIN_STREAMING );
	if ( pReused != pPage || !memory.releaseRecyclableMetadataPage( pReused, SIR_MEMORY_DOMAIN_STREAMING ) ) {
		return false;
	}
	if ( memory.getDomainStats( SIR_MEMORY_DOMAIN_NETWORKING ).nCommittedBytes != 0 ||
	     memory.getDomainStats( SIR_MEMORY_DOMAIN_STREAMING ).nCommittedBytes != 0 ) {
		return false;
	}
	return true;
}

static bool testBitmapAndRecommit( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 512, nPage * 240, nPage * 16, 8 };
	if ( !memory.init( config ) ) {
		return false;
	}
	SIRMemorySegment *pSegment = memory.createSegment( "test", nPage * 192, 0, SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT );
	if ( pSegment == nullptr || !pSegment->commit( nPage * 60, nPage * 80 ) ) {
		return false;
	}
	if ( !pSegment->commit( nPage * 70, nPage * 20 ) || pSegment->getCommittedBytes() != nPage * 80 ) {
		return false;
	}
	if ( !pSegment->decommit( nPage * 63, nPage * 65 ) || pSegment->getCommittedBytes() != nPage * 15 ) {
		return false;
	}
	if ( !pSegment->decommit( nPage * 63, nPage * 65 ) || pSegment->getCommittedBytes() != nPage * 15 ) {
		return false;
	}
	uint8_t *pAddress = ( uint8_t * )pSegment->getBase();
	pAddress[nPage * 62] = 0xA5;
	if ( !pSegment->decommit( nPage * 60, nPage * 3 ) || !pSegment->commit( nPage * 62, nPage ) ) {
		return false;
	}
	if ( pAddress[nPage * 62] != 0 || pSegment->getCommittedBytes() != nPage * 13 ) {
		return false;
	}
	if ( !pSegment->decommit( nPage * 128, nPage * 12 ) || !pSegment->decommit( nPage * 62, nPage ) ) {
		return false;
	}
	if ( pSegment->getCommittedBytes() != 0 || memory.getDomainStats( SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT ).nCommittedBytes != 0 ) {
		return false;
	}
	return true;
}

static bool testOversizedRequests( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 8, 8 };
	if ( !memory.init( config ) ) {
		return false;
	}
	if ( memory.createSegment( "overflow", UINT64_MAX ) != nullptr ||
	     memory.createSegment( "overflow", nPage * 4, UINT64_MAX ) != nullptr || memory.allocateMetadata( UINT64_MAX, 8 ) != nullptr ||
	     memory.allocateMetadata( 8, 3 ) != nullptr ) {
		return false;
	}
	const char *szLongName = "SIR_SEGMENT_NAME_LONGER_THAN_THE_32_BYTE_STORAGE_ARRAY";
	SIRMemorySegment *pSegment = memory.createSegment( szLongName, nPage * 4 );
	return pSegment != nullptr && strlen( pSegment->getName() ) == 31 && strncmp( pSegment->getName(), szLongName, 31 ) == 0;
}

static bool testFailedInitializationRecovery( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t invalid = { nPage * 4096, nPage, nPage * 2048, 30 };
	if ( memory.init( invalid ) || memory.isInitialized() || memory.getPageSize() != 0 ) {
		return false;
	}
	const sirMemoryConfig_t valid = { nPage * 128, nPage * 16, nPage * 8, 4 };
	if ( !memory.init( valid ) || !memory.isInitialized() ) {
		return false;
	}
	memory.shutdown();
	return memory.getPageSize() == 0 && !memory.isInitialized();
}

static bool testBackingTelemetry( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 8, 4 };
	if ( !memory.init( config ) ) {
		return false;
	}
	SIRMemorySegment *pSegment = memory.createSegment( "telemetry", nPage * 4, nPage, SIR_MEMORY_DOMAIN_STREAMING );
	if ( pSegment == nullptr ) {
		return false;
	}
	const sirPageBackingStats_t original = memory.getPageBackingStats();
	if ( !pSegment->commit( 0, nPage ) || !pSegment->commit( 0, nPage ) || pSegment->commit( nPage, nPage ) ||
	     !pSegment->decommit( 0, nPage ) || !pSegment->decommit( 0, nPage ) ) {
		return false;
	}
	const sirPageBackingStats_t stats = memory.getPageBackingStats();
	if ( stats.nNoopCommitRequests != original.nNoopCommitRequests + 1 ||
	     stats.nNoopDecommitRequests != original.nNoopDecommitRequests + 1 || stats.nNoopCommitBytes != original.nNoopCommitBytes + nPage ||
	     stats.nNoopDecommitBytes != original.nNoopDecommitBytes + nPage ||
	     stats.nSuccessfulCommitCalls != original.nSuccessfulCommitCalls + 1 ||
	     stats.nSuccessfulDecommitCalls != original.nSuccessfulDecommitCalls + 1 ||
	     stats.nCommitBudgetDenials != original.nCommitBudgetDenials + 1 ||
	     stats.nManagedCommittedBytes != original.nManagedCommittedBytes ) {
		return false;
	}
	memory.shutdown();
	const sirPageBackingStats_t cleared = memory.getPageBackingStats();
	return cleared.nManagedCommittedBytes == 0 && cleared.nNoopCommitRequests == 0 && cleared.nSuccessfulCommitCalls == 0;
}

static bool testRetainedPageBacking( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 8, 4 };
	if ( !memory.init( config ) ) {
		return false;
	}
	SIRMemorySegment *pSegment = memory.createSegment( "retained", nPage * 6, nPage * 2, SIR_MEMORY_DOMAIN_STREAMING );
	if ( pSegment == nullptr || !memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_STREAMING, nPage * 3 ) ||
	     !pSegment->commit( 0, nPage * 2 ) ) {
		return false;
	}
	uint8_t *pBytes = ( uint8_t * )pSegment->getBase();
	pBytes[0] = 0xA5;
	pBytes[nPage] = 0x5A;
	if ( pSegment->retireIdleRange( 0, nPage, 7, 11 ) || !pSegment->bindRetirementOwner( 7, 11 ) ||
	     pSegment->bindRetirementOwner( 7, 12 ) || pSegment->retireIdleRange( 0, nPage, 7, 12 ) ||
	     !pSegment->retireIdleRange( 0, nPage, 7, 11 ) || pSegment->retireIdleRange( 0, nPage, 7, 11 ) ) {
		return false;
	}
	const sirPageBackingStats_t before = memory.getPageBackingStats();
	if ( before.nPendingDecommitBytes != nPage || pSegment->commit( nPage * 2, nPage ) || pBytes[0] != 0xA5 ) {
		return false;
	}
	if ( pSegment->cancelRetiredRange( 0, nPage, 7, 12 ) || !pSegment->commit( 0, nPage ) || pBytes[0] != 0xA5 ||
	     memory.getPageBackingStats().nPendingDecommitBytes != 0 ) {
		return false;
	}
	if ( !pSegment->retireIdleRange( 0, nPage * 2, 7, 11 ) ) {
		return false;
	}
	const sirPageDrainReport_t drained = memory.drainPendingDecommits( nPage );
	if ( drained.nDecommittedBytes != nPage || drained.nRemainingPendingBytes != nPage || drained.nCompletedRuns != 1 ||
	     drained.nFailedRuns != 0 || pSegment->getCommittedBytes() != nPage ) {
		return false;
	}
	if ( !pSegment->cancelRetiredRange( nPage, nPage, 7, 11 ) || memory.getPageBackingStats().nPendingDecommitBytes != 0 ||
	     pBytes[nPage] != 0x5A ) {
		return false;
	}
	if ( !pSegment->commit( 0, nPage ) || pBytes[0] != 0 || memory.getPageBackingStats().nRetainedReuseBytes < nPage * 2 ) {
		return false;
	}
	if ( !pSegment->decommit( 0, nPage * 2 ) || memory.getPageBackingStats().nPendingDecommitBytes != 0 ) {
		return false;
	}
	return true;
}

static bool testValidationAndBoundaryInputs( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	if ( nPage == 0 ) {
		return false;
	}
	const sirMemoryConfig_t valid = { nPage * 128, nPage * 24, nPage * 8, 2 };
	const sirMemoryBudgetProfile_t budget = { nPage * 16, nPage * 24 };
	{
		SIRMemoryManager cold;
		SIRMemorySegment unattached;
		if ( cold.setCommitBudgetBytes( nPage ) || cold.createSegment( "no", nPage ) != nullptr ||
		     cold.allocateMetadata( nPage, 8 ) != nullptr || cold.allocateRecyclableMetadataPage() != nullptr ||
		     cold.releaseRecyclableMetadataPage( nullptr ) || cold.setDomainCommitLimit( SIR_MEMORY_DOMAIN_CORE, nPage ) ||
		     cold.getDomainStats( SIR_MEMORY_DOMAIN_CORE ).nCommittedBytes != 0 || unattached.getPageSize() != 0 ||
		     unattached.getCommittedBytes() != 0 || unattached.getManager() != nullptr || unattached.commit( 0, nPage ) ||
		     unattached.decommit( 0, nPage ) || unattached.bindRetirementOwner( 1, 1 ) || unattached.retireIdleRange( 0, nPage, 1, 1 ) ||
		     unattached.cancelRetiredRange( 0, nPage, 1, 1 ) || cold.copyTrace( nullptr, 1 ) != 0 || cold.initTraceBuffer( 0 ) ||
		     cold.initTraceBuffer( 1 ) ) {
			return false;
		}
		sirMemoryConfig_t invalid = valid;
		invalid.nMaxSegments = 0;
		if ( cold.init( invalid ) ) {
			return false;
		}
		invalid = valid;
		invalid.nMetadataBytes = 0;
		if ( cold.init( invalid ) ) {
			return false;
		}
		invalid = valid;
		invalid.nReserveBytes = invalid.nMetadataBytes;
		if ( cold.init( invalid ) ) {
			return false;
		}
		invalid = valid;
		invalid.nReserveBytes = UINT64_MAX;
		if ( cold.init( invalid ) ) {
			return false;
		}
		invalid = valid;
		invalid.nMetadataBytes = UINT64_MAX;
		if ( cold.init( invalid ) ) {
			return false;
		}
		if ( cold.initBudgeted( valid, { 0, nPage * 24 } ) || cold.initBudgeted( valid, { nPage * 25, nPage * 24 } ) ||
		     cold.initBudgeted( valid, { nPage * 16, nPage * 256 } ) || cold.initBudgeted( valid, { nPage * 16 + 1, nPage * 24 } ) ||
		     cold.initBudgeted( valid, { nPage * 16, nPage * 24 + 1 } ) ) {
			return false;
		}
	}
	{
		SIRMemoryManager memory( valid, budget );
		if ( !memory.isInitialized() || memory.init( valid ) || memory.initBudgeted( valid, budget ) ||
		     memory.setCommitBudgetBytes( nPage * 12 ) || memory.setCommitBudgetBytes( nPage * 24 + 1 ) ||
		     memory.setCommitBudgetBytes( nPage * 25 ) || !memory.setCommitBudgetBytes( nPage * 24 ) ||
		     memory.getStats().nCommitBudgetBytes != nPage * 24 ) {
			return false;
		}
		const sirMemoryDomain_t invalidDomain = ( sirMemoryDomain_t )SIR_MEMORY_DOMAIN_COUNT;
		if ( memory.getDomainStats( invalidDomain ).nCommittedBytes != 0 || memory.setDomainCommitLimit( invalidDomain, nPage ) ||
		     memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_NETWORKING, 0 ) ||
		     memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_NETWORKING, nPage + 1 ) || memory.allocateMetadata( 0, 8 ) != nullptr ||
		     memory.allocateMetadata( 8, 3 ) != nullptr || memory.allocateMetadata( 8, 8, invalidDomain ) != nullptr ||
		     memory.allocateRecyclableMetadataPage( invalidDomain ) != nullptr ||
		     memory.releaseRecyclableMetadataPage( nullptr, SIR_MEMORY_DOMAIN_NETWORKING ) ||
		     memory.releaseRecyclableMetadataPage( ( void * )( uintptr_t )1, SIR_MEMORY_DOMAIN_NETWORKING ) ||
		     memory.createSegment( nullptr, nPage ) != nullptr || memory.createSegment( "invalid", nPage, 0, invalidDomain ) != nullptr ||
		     memory.createSegment( "zero", 0 ) != nullptr || memory.createSegment( "large", UINT64_MAX ) != nullptr ||
		     memory.createSegment( "limit", nPage, nPage * 2 ) != nullptr ) {
			return false;
		}
		SIRMemorySegment *pFirst = memory.createSegment( "first", nPage * 2, nPage, SIR_MEMORY_DOMAIN_STREAMING );
		SIRMemorySegment *pSecond = memory.createSegment( "second", nPage * 2, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		if ( pFirst == nullptr || pSecond == nullptr || memory.createSegment( "third", nPage ) != nullptr ||
		     pFirst->getReservedBytes() != nPage * 2 || pFirst->getCommitLimitBytes() != nPage ||
		     pFirst->getDomain() != SIR_MEMORY_DOMAIN_STREAMING || pSecond->getCommitLimitBytes() != nPage * 2 || pFirst->commit( 0, 0 ) ||
		     pFirst->decommit( 0, 0 ) || pFirst->commit( 1, nPage ) || pFirst->decommit( 1, nPage ) || pFirst->commit( 0, nPage * 3 ) ||
		     pFirst->decommit( 0, nPage * 3 ) || pFirst->commit( nPage * 3, nPage ) || pFirst->decommit( nPage * 3, nPage ) ) {
			return false;
		}
		if ( !memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_STREAMING, nPage ) || !pFirst->commit( 0, nPage ) ||
		     memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_STREAMING, nPage / 2 ) || !pFirst->decommit( 0, nPage ) ) {
			return false;
		}
		if ( !memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_NETWORKING, nPage ) ) {
			return false;
		}
		void *pPage = memory.allocateRecyclableMetadataPage( SIR_MEMORY_DOMAIN_NETWORKING );
		if ( pPage == nullptr || memory.allocateRecyclableMetadataPage( SIR_MEMORY_DOMAIN_NETWORKING ) != nullptr ||
		     memory.releaseRecyclableMetadataPage( pPage, SIR_MEMORY_DOMAIN_GAMEPLAY ) ||
		     memory.releaseRecyclableMetadataPage( ( uint8_t * )pPage + 1, SIR_MEMORY_DOMAIN_NETWORKING ) ||
		     !memory.releaseRecyclableMetadataPage( pPage, SIR_MEMORY_DOMAIN_NETWORKING ) ||
		     memory.releaseRecyclableMetadataPage( pPage, SIR_MEMORY_DOMAIN_NETWORKING ) ) {
			return false;
		}
		if ( memory.initTraceBuffer( 0 ) || !memory.initTraceBuffer( 4 ) || memory.initTraceBuffer( 4 ) ||
		     memory.copyTrace( nullptr, 1 ) != 0 || memory.copyTrace( ( sirMemoryTraceEvent_t * )pPage, 0 ) != 0 ) {
			return false;
		}
		memory.shutdown();
		if ( memory.isInitialized() || memory.getPageSize() != 0 ) {
			return false;
		}
	}
	{
		SIRMemoryManager memory( valid );
		if ( !memory.isInitialized() ) {
			return false;
		}
	}
	return true;
}

static bool testTraceWrapAndSparseRetire( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	if ( nPage == 0 ) {
		return false;
	}
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 12, 4 };
	if ( !memory.init( config ) || memory.initTraceBuffer( UINT32_MAX ) || !memory.initTraceBuffer( 2 ) ) {
		return false;
	}
	sirMemoryTraceEvent_t events[2] = {};
	if ( memory.copyTrace( events, 2 ) != 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( memory.allocateMetadata( 8, 8 ) == nullptr ) {
			return false;
		}
	}
	if ( memory.copyTrace( events, 2 ) != 2 || events[1].nSequence <= events[0].nSequence || memory.getStats().nTraceDroppedCount < 6 ) {
		return false;
	}
	if ( memory.copyTrace( events, 1 ) != 1 ) {
		return false;
	}
	SIRMemorySegment *pSegment = memory.createSegment( "trace-sparse", nPage * 4 );
	if ( pSegment == nullptr || !pSegment->commit( 0, nPage * 4 ) || !pSegment->bindRetirementOwner( 5, 7 ) ||
	     !pSegment->retireIdleRange( 0, nPage, 5, 7 ) || pSegment->cancelRetiredRange( nPage, nPage, 5, 7 ) ||
	     !pSegment->retireIdleRange( nPage * 2, nPage, 5, 7 ) ) {
		return false;
	}
	const sirPageDrainReport_t waiting = memory.drainPendingDecommits( nPage - 1 );
	if ( waiting.nRemainingPendingBytes != nPage * 2 || waiting.nCompletedRuns != 0 ) {
		return false;
	}
	const sirPageDrainReport_t drained = memory.drainPendingDecommits( nPage * 4 );
	if ( drained.nCompletedRuns != 2 || drained.nFailedRuns != 0 || drained.nDecommittedBytes != nPage * 2 ||
	     drained.nRemainingPendingBytes != 0 ) {
		return false;
	}
	if ( !pSegment->decommit( nPage, nPage ) || !pSegment->decommit( nPage * 3, nPage ) ) {
		return false;
	}
	memory.shutdown();
	return true;
}

int main( void )
{
	if ( !testMemoryBudget() || !testCoreDomainMetadata() || !testBitmapAndRecommit() || !testOversizedRequests() ||
	     !testFailedInitializationRecovery() || !testBackingTelemetry() || !testRetainedPageBacking() ||
	     !testValidationAndBoundaryInputs() || !testTraceWrapAndSparseRetire() ) {
		fputs( "SIR memory owner test failed\n", stderr );
		return 1;
	}
	return 0;
}
