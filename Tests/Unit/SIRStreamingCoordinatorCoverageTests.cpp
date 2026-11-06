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

#include <Streaming/SIRStreamingCoordinator.h>
#include <stdio.h>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Coverage probe failed at line %d: %s\n", __LINE__, #expression );                                            \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static sirStreamingDemand_t makeDemand( uint64_t nUnit, uint64_t nConsumer = 1 ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit.nContainerId = 1;
	demand.keyUnit.nUnitId = nUnit;
	demand.keyUnit.nManifestGeneration = 1;
	demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	demand.nConsumerId = nConsumer;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = 1;
	demand.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	return demand;
}

static int runLifecycleCoverage( void ) noexcept
{
	SIRStreamingCoordinator co;
	alignas( 64 ) uint8_t mem[8192] = {};
	sirStreamingTicket_t a = {}, b = {};
	sirStreamingSnapshot_t s = {};
	auto changed = makeDemand( 1 );
	CHECK( co.submit( makeDemand( 1 ), &a ) == SIR_STREAMING_NOT_READY );
	CHECK( co.querySlot( 0, &a, &s ) == SIR_STREAMING_NOT_READY );
	CHECK( co.initialize( nullptr, sizeof( mem ), 1 ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.initialize( mem, 0, 2 ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.initialize( mem, sizeof( mem ), 2 ) == SIR_STREAMING_OK );
	CHECK( co.initialize( mem, sizeof( mem ), 2 ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.submit( makeDemand( 1 ), &a ) == SIR_STREAMING_OK );
	CHECK( co.submit( makeDemand( 1 ), &b ) == SIR_STREAMING_DUPLICATE );
	CHECK( co.submit( makeDemand( 2 ), &b ) == SIR_STREAMING_OK );
	CHECK( co.submit( makeDemand( 3 ), &b ) == SIR_STREAMING_OUT_OF_CAPACITY );
	CHECK( co.query( a, &s ) == SIR_STREAMING_OK );
	CHECK( co.querySlot( 0, &a, &s ) == SIR_STREAMING_OK );
	CHECK( co.querySlot( 2, &b, &s ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.query( a, nullptr ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.updateDemand( a, makeDemand( 1 ) ) == SIR_STREAMING_OK );
	changed.nRequestEpoch = 0;
	CHECK( co.updateDemand( a, changed ) == SIR_STREAMING_INVALID_ARGUMENT );
	changed = makeDemand( 1 );
	changed.nRequestEpoch = 2;
	CHECK( co.updateDemand( a, changed ) == SIR_STREAMING_OK );
	CHECK( co.advance( a, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_OK );
	CHECK( co.advance( a, SIR_STREAMING_STAGE_PREPARED ) == SIR_STREAMING_OK );
	CHECK( co.advance( a, SIR_STREAMING_STAGE_ACTIVE ) == SIR_STREAMING_OK );
	CHECK( co.getStats().nActive == 1 );
	changed.eTarget = SIR_STREAMING_TARGET_PREPARED;
	CHECK( co.updateDemand( a, changed ) == SIR_STREAMING_BUSY );
	CHECK( co.cancel( a ) == SIR_STREAMING_OK );
	CHECK( co.updateDemand( a, changed ) == SIR_STREAMING_BUSY );
	CHECK( co.advance( a, SIR_STREAMING_STAGE_RETIRING ) == SIR_STREAMING_OK );
	CHECK( co.advance( a, SIR_STREAMING_STAGE_EMPTY ) == SIR_STREAMING_OK );
	CHECK( co.cancel( b ) == SIR_STREAMING_OK );
	CHECK( co.shutdown() == SIR_STREAMING_OK );
	CHECK( co.submit( makeDemand( 1 ), &a ) == SIR_STREAMING_NOT_READY );
	CHECK( co.shutdown() == SIR_STREAMING_NOT_READY );
	return 0;
}

static int runNegativeCoverage( void ) noexcept
{
	alignas( 64 ) uint8_t storage[4096] = {};
	SIRStreamingCoordinator co;
	sirStreamingTicket_t ticket = {}, otherTicket = {};
	sirStreamingSnapshot_t snap = {};
	auto invalid = makeDemand( 1 );
	auto otherGen = makeDemand( 1 );
	auto different = makeDemand( 1 );
	sirStreamingTicket_t stale = { 0, 0 };
	CHECK( co.getStats().nCapacity == 0 );
	CHECK( co.query( { 0, 1 }, &snap ) == SIR_STREAMING_STALE_TICKET );
	CHECK( co.querySlot( 0, &ticket, &snap ) == SIR_STREAMING_NOT_READY );
	CHECK( co.initialize( storage, sizeof( storage ), 0 ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.initialize( storage + 1, sizeof( storage ) - 1, 1 ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.initialize( storage, sizeof( storage ), 4 ) == SIR_STREAMING_OK );
	CHECK( co.querySlot( 0, &ticket, &snap ) == SIR_STREAMING_NOT_READY );
	CHECK( co.querySlot( 0, nullptr, &snap ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.querySlot( 0, &ticket, nullptr ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.querySlot( 4, &ticket, &snap ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.submit( makeDemand( 1 ), nullptr ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.submit( makeDemand( 0 ), &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	invalid.keyUnit.nManifestGeneration = 0;
	CHECK( co.submit( invalid, &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	invalid = makeDemand( 1 );
	invalid.keyUnit.eKind = SIR_STREAMING_UNIT_COUNT;
	CHECK( co.submit( invalid, &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	invalid = makeDemand( 1 );
	invalid.nConsumerId = 0;
	CHECK( co.submit( invalid, &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	invalid = makeDemand( 1 );
	invalid.nConsumerGeneration = 0;
	CHECK( co.submit( invalid, &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	invalid = makeDemand( 1 );
	invalid.nRequestEpoch = 0;
	CHECK( co.submit( invalid, &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	invalid = makeDemand( 1 );
	invalid.eTarget = ( sirStreamingTarget_t )99;
	CHECK( co.submit( invalid, &ticket ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.submit( makeDemand( 1 ), &ticket ) == SIR_STREAMING_OK );
	otherGen.nConsumerGeneration = 2;
	CHECK( co.submit( otherGen, &otherTicket ) == SIR_STREAMING_OK );
	CHECK( co.cancel( otherTicket ) == SIR_STREAMING_OK );
	CHECK( co.query( stale, &snap ) == SIR_STREAMING_STALE_TICKET );
	stale = { 100, 1 };
	CHECK( co.cancel( stale ) == SIR_STREAMING_STALE_TICKET );
	CHECK( co.advance( stale, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_STALE_TICKET );
	CHECK( co.updateDemand( stale, makeDemand( 1 ) ) == SIR_STREAMING_STALE_TICKET );
	different.nConsumerId = 2;
	CHECK( co.updateDemand( ticket, different ) == SIR_STREAMING_INVALID_ARGUMENT );
	different = makeDemand( 1 );
	different.nConsumerGeneration = 2;
	CHECK( co.updateDemand( ticket, different ) == SIR_STREAMING_INVALID_ARGUMENT );
	different = makeDemand( 3 );
	CHECK( co.updateDemand( ticket, different ) == SIR_STREAMING_INVALID_ARGUMENT );
	different = makeDemand( 1 );
	different.nRequestEpoch = 2;
	CHECK( co.updateDemand( ticket, different ) == SIR_STREAMING_OK );
	different.nRequestEpoch = 1;
	CHECK( co.updateDemand( ticket, different ) == SIR_STREAMING_INVALID_ARGUMENT );
	CHECK( co.getStats().nQueued == 1 );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_RETIRING ) == SIR_STREAMING_INVALID_TRANSITION );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_OK );
	CHECK( co.getStats().nPreparing == 1 );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_FAILED ) == SIR_STREAMING_OK );
	CHECK( co.getStats().nFailed == 1 );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_RETIRING ) == SIR_STREAMING_OK );
	CHECK( co.getStats().nRetiring == 1 );
	CHECK( co.updateDemand( ticket, makeDemand( 1 ) ) == SIR_STREAMING_INVALID_ARGUMENT );
	different.nRequestEpoch = 2;
	CHECK( co.updateDemand( ticket, different ) == SIR_STREAMING_BUSY );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_EMPTY ) == SIR_STREAMING_OK );
	CHECK( co.query( ticket, &snap ) == SIR_STREAMING_STALE_TICKET );
	CHECK( co.querySlot( 0, &stale, &snap ) == SIR_STREAMING_NOT_READY );
	CHECK( co.submit( makeDemand( 2 ), &ticket ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_PREPARED ) == SIR_STREAMING_OK );
	CHECK( co.getStats().nPrepared == 1 );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_FAILED ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_RETIRING ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_EMPTY ) == SIR_STREAMING_OK );
	CHECK( co.submit( makeDemand( 3 ), &ticket ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_PREPARING ) == SIR_STREAMING_OK );
	CHECK( co.cancel( ticket ) == SIR_STREAMING_OK );
	CHECK( co.querySlot( ticket.nSlot, &stale, &snap ) == SIR_STREAMING_OK );
	CHECK( snap.bCancellationRequested );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_PREPARED ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_ACTIVE ) == SIR_STREAMING_INVALID_TRANSITION );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_RETIRING ) == SIR_STREAMING_OK );
	CHECK( co.advance( ticket, SIR_STREAMING_STAGE_EMPTY ) == SIR_STREAMING_OK );
	CHECK( co.shutdown() == SIR_STREAMING_OK );
	CHECK( co.getStats().nCapacity == 0 );
	CHECK( co.query( { 0, 1 }, &snap ) == SIR_STREAMING_STALE_TICKET );
	CHECK( co.querySlot( 0, &stale, &snap ) == SIR_STREAMING_NOT_READY );
	return 0;
}

int main( void )
{
	if ( runLifecycleCoverage() != 0 ) {
		return 1;
	}
	return runNegativeCoverage();
}
