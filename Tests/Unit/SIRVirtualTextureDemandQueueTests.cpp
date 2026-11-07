/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureDemandQueue.h>

#include <stdio.h>

#define SIR_TEST( cond )                                                                                                                   \
	do {                                                                                                                                   \
		if ( !( cond ) ) {                                                                                                                 \
			fprintf( stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond );                                                               \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	SIRVirtualTextureDemandQueue queue;
	sirVirtualTextureDemandTicket_t ticket = {};
	SIR_TEST( !queue.beginLoad( nullptr ) );
	SIR_TEST( !queue.beginLoad( &ticket ) );
	SIR_TEST( !queue.requestBundle( UINT32_MAX, 1 ) );
	SIR_TEST( queue.getStats().nRejected == 1 );

	// Donor highest-priority-first, oldest-on-ties ordering.
	SIR_TEST( queue.requestBundle( 11, 20 ) );
	SIR_TEST( queue.requestBundle( 12, 40 ) );
	SIR_TEST( queue.requestBundle( 13, 40 ) );
	SIR_TEST( queue.requestBundle( 14, 10 ) );
	SIR_TEST( queue.getStats().nQueued == 4 );
	SIR_TEST( !queue.requestBundle( 15, 255 ) ); // Pinned queued jobs.
	SIR_TEST( queue.getStats().nRejected == 2 );
	SIR_TEST( queue.requestBundle( 11, 80 ) ); // Dedup + priority promotion.
	SIR_TEST( queue.getStats().nCoalesced == 1 );
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 11 );
	const sirVirtualTextureDemandTicket_t first = ticket;
	SIR_TEST( !queue.resetCold() ); // Caller still owns the in-flight payload.
	SIR_TEST( !queue.finishLoad( { first.nSlot, first.nBundle, first.nEpoch + 1U, first.nSerial }, true ) );
	SIR_TEST( !queue.finishLoad( { first.nSlot, first.nBundle, first.nEpoch, first.nSerial + 1U }, true ) );
	SIR_TEST( !queue.requestBundle( 16, 90 ) ); // LOADING and QUEUED never recycled.
	SIR_TEST( queue.finishLoad( ticket, true ) && queue.isReady( 11 ) );
	SIR_TEST( !queue.finishLoad( ticket, true ) ); // Exactly-once completion.
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 12 );
	SIR_TEST( queue.finishLoad( ticket, false ) );
	SIR_TEST( !queue.isReady( 12 ) );
	SIR_TEST( queue.requestBundle( 17, 30 ) ); // FAILED slot recycled.
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 13 );
	SIR_TEST( queue.finishLoad( ticket, true ) );
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 17 );
	SIR_TEST( queue.finishLoad( ticket, true ) );
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 14 );
	SIR_TEST( queue.finishLoad( ticket, true ) );
	SIR_TEST( queue.getStats().nReady == 4 && queue.getStats().nQueued == 0 && queue.getStats().nLoading == 0 );

	// All READY: evict donor LRU (14), never the currently favored most-recent slot.
	SIR_TEST( queue.requestBundle( 13, 1 ) );
	SIR_TEST( queue.requestBundle( 18, 5 ) );
	SIR_TEST( !queue.isReady( 14 ) && queue.isReady( 13 ) && queue.getStats().nEvictions == 1 );
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 18 );
	SIR_TEST( queue.finishLoad( ticket, true ) );
	SIR_TEST( queue.requestBundle( 19, 3 ) );
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 19 );
	SIR_TEST( queue.finishLoad( ticket, true ) );

	const uint64_t nPreviousEpoch = queue.getEpoch();
	SIR_TEST( queue.resetCold() && queue.getEpoch() != nPreviousEpoch );
	SIR_TEST( queue.getStats().nReady == 0 && queue.getStats().nCoalesced == 0 );
	SIR_TEST( !queue.finishLoad( first, true ) ); // Stale epoch cannot publish.
	SIR_TEST( !queue.isReady( 18 ) );
	SIR_TEST( queue.requestBundle( 11, 1 ) );
	SIR_TEST( queue.beginLoad( &ticket ) && ticket.nBundle == 11 );
	SIR_TEST( ticket.nSerial != first.nSerial && ticket.nEpoch != first.nEpoch );
	SIR_TEST( !queue.finishLoad( { 5, ticket.nBundle, ticket.nEpoch, ticket.nSerial }, true ) );
	SIR_TEST( queue.finishLoad( ticket, true ) );
	SIR_TEST( queue.resetCold() );
	return 0;
}
