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

#include <Core/Memory/SIRMemoryLayoutPlanner.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void sirLayoutRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "layout coverage failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirLayoutRequire( ( condition ), __LINE__ )

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage != 0 );
	const sirMemoryConfig_t base = { nPage * 64, nPage * 32, nPage * 8, 4 };
	const sirMemoryBudgetProfile_t budget = { nPage * 8, nPage * 32 };
	const sirMemoryLayoutEntry_t entry = { nPage * 4, nPage * 2, 0, false, true };
	sirMemoryLayoutResult_t result = {};
	REQUIRE( SIRMemoryLayoutPlanner::preflight( base, budget, &entry, 1, &result ) );
	REQUIRE( result.nReservedDataBytes == nPage * 4 );
	REQUIRE( result.nSegmentCount == 1 );
	REQUIRE( result.nRemainingDataBytes == nPage * 52 );

	// A preflight never allocates and must not publish a partial result on
	// rejection.
	const sirMemoryLayoutResult_t sentinel = { 19, 23, 29, 31 };
	result = sentinel;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &entry, 1, nullptr ) );
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, nullptr, 1, &result ) );
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &entry, 5, &result ) );
	REQUIRE( result.nReservedDataBytes == sentinel.nReservedDataBytes && result.nSegmentCount == sentinel.nSegmentCount );

	sirMemoryConfig_t config = base;
	config.nMaxSegments = 0;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( config, budget, &entry, 1, &result ) );
	config = base;
	config.nReserveBytes = UINT64_MAX;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( config, budget, &entry, 1, &result ) );
	config = base;
	config.nMetadataBytes = UINT64_MAX;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( config, budget, &entry, 1, &result ) );
	config = base;
	config.nReserveBytes = config.nMetadataBytes;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( config, budget, &entry, 1, &result ) );
	config = base;
	config.nMetadataBytes = 0;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( config, budget, &entry, 1, &result ) );

	sirMemoryBudgetProfile_t badBudget = budget;
	badBudget.nInitialCommitBudgetBytes = 0;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, badBudget, &entry, 1, &result ) );
	badBudget = budget;
	badBudget.nInitialCommitBudgetBytes = badBudget.nMaximumCommitBudgetBytes + nPage;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, badBudget, &entry, 1, &result ) );
	badBudget = budget;
	badBudget.nMaximumCommitBudgetBytes = base.nReserveBytes + nPage;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, badBudget, &entry, 1, &result ) );
	badBudget = budget;
	badBudget.nInitialCommitBudgetBytes += 1;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, badBudget, &entry, 1, &result ) );
	badBudget = budget;
	badBudget.nMaximumCommitBudgetBytes += 1;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, badBudget, &entry, 1, &result ) );

	sirMemoryLayoutEntry_t badEntry = entry;
	badEntry.nReserveBytes = 0;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nReserveBytes = UINT64_MAX;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nReserveBytes = ( uint64_t )UINT32_MAX + nPage;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.bBuddyPowerOfTwo = true;
	badEntry.nReserveBytes = nPage * 3;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nCommitLimitBytes = nPage * 5;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nCommitLimitBytes = UINT64_MAX;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nAllocatorMetadataBytes = UINT64_MAX;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nAllocatorMetadataBytes = nPage * 8;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );
	badEntry = entry;
	badEntry.nReserveBytes = nPage * 60;
	badEntry.nCommitLimitBytes = 0;
	badEntry.bUse32BitOffsets = false;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, budget, &badEntry, 1, &result ) );

	badBudget = budget;
	badBudget.nInitialCommitBudgetBytes = nPage;
	badEntry = entry;
	badEntry.nAllocatorMetadataBytes = nPage * 2;
	REQUIRE( !SIRMemoryLayoutPlanner::preflight( base, badBudget, &badEntry, 1, &result ) );
	REQUIRE( result.nReservedDataBytes == sentinel.nReservedDataBytes && result.nSegmentCount == sentinel.nSegmentCount );

	// Empty and multiple-entry valid layouts preserve deterministic accounting.
	REQUIRE( SIRMemoryLayoutPlanner::preflight( base, budget, nullptr, 0, &result ) );
	REQUIRE( result.nReservedDataBytes == 0 && result.nSegmentCount == 0 );
	const sirMemoryLayoutEntry_t entries[2] = { entry, entry };
	REQUIRE( SIRMemoryLayoutPlanner::preflight( base, budget, entries, 2, &result ) );
	REQUIRE( result.nReservedDataBytes == nPage * 8 && result.nSegmentCount == 2 );
	return 0;
}
