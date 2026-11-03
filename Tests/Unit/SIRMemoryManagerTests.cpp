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
	uint8_t *pAddress = (uint8_t *)pSegment->getBase();
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
		memory.createSegment( "overflow", nPage * 4, UINT64_MAX ) != nullptr ||
		memory.allocateMetadata( UINT64_MAX, 8 ) != nullptr ||
		memory.allocateMetadata( 8, 3 ) != nullptr ) {
		return false;
	}
	const char *szLongName = "SIR_SEGMENT_NAME_LONGER_THAN_THE_32_BYTE_STORAGE_ARRAY";
	SIRMemorySegment *pSegment = memory.createSegment( szLongName, nPage * 4 );
	return pSegment != nullptr && strlen( pSegment->getName() ) == 31 &&
		strncmp( pSegment->getName(), szLongName, 31 ) == 0;
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

int main( void )
{
	if ( !testMemoryBudget() || !testCoreDomainMetadata() ||
		!testBitmapAndRecommit() || !testOversizedRequests() || !testFailedInitializationRecovery() ) {
		fputs( "SIR memory owner test failed\n", stderr );
		return 1;
	}
	return 0;
}
