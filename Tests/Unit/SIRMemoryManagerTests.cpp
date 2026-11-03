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

int main( void )
{
	if ( !testMemoryBudget() ) {
		fputs( "SIR memory owner test failed\n", stderr );
		return 1;
	}
	return 0;
}
