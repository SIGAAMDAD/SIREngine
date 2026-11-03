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

#include <Core/Platform/SIRVirtualMemory.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

extern "C" int __wrap_madvise( void *pAddress, size_t nBytes, int nAdvice ) noexcept
{
	( void )pAddress;
	( void )nBytes;
	( void )nAdvice;
	errno = EIO;
	return -1;
}

int main( void )
{
	const size_t nPage = SIRVirtualMemory::getPageSize();
	void *pAddress = SIRVirtualMemory::reserve( nPage );
	if ( pAddress == nullptr || !SIRVirtualMemory::commit( pAddress, nPage ) ) {
		return 1;
	}

	volatile uint8_t *pBytes = ( volatile uint8_t * )pAddress;
	pBytes[0] = 0x5A;
	if ( SIRVirtualMemory::decommit( pAddress, nPage ) || pBytes[0] != 0x5A ) {
		fputs( "failed decommit damaged committed page\n", stderr );
		return 1;
	}
	const bool bReleased = SIRVirtualMemory::release( pAddress, nPage );
	if ( !bReleased ) {
		return 1;
	}
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 8, 4 };
	if ( !memory.init( config ) ) {
		return 1;
	}
	SIRMemorySegment *pSegment = memory.createSegment( "rollback", nPage * 2 );
	if ( pSegment == nullptr || !pSegment->commit( 0, nPage ) || !pSegment->bindRetirementOwner( 7, 13 ) ) {
		return 1;
	}
	volatile uint8_t *pManaged = ( volatile uint8_t * )pSegment->getBase();
	pManaged[0] = 0xA7;
	if ( !pSegment->retireIdleRange( 0, nPage, 7, 13 ) ) {
		return 1;
	}
	const sirPageBackingStats_t before = memory.getPageBackingStats();
	const sirPageDrainReport_t report = memory.drainPendingDecommits( nPage );
	const sirPageBackingStats_t after = memory.getPageBackingStats();
	if ( report.nFailedRuns != 1 || report.nCompletedRuns != 0 || report.nDecommittedBytes != 0 ||
	     report.nRemainingPendingBytes != nPage || after.nManagedCommittedBytes != before.nManagedCommittedBytes ||
	     after.nFailedDecommitCalls != before.nFailedDecommitCalls + 1 || pManaged[0] != 0xA7 ) {
		fputs( "failed pending drain lost committed memory or corrupted payload\n", stderr );
		return 1;
	}
	if ( !pSegment->cancelRetiredRange( 0, nPage, 7, 13 ) ||
	     memory.getPageBackingStats().nPendingDecommitBytes != 0 ) {
		return 1;
	}
	return 0;
}
