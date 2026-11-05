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

#include <Streaming/SIRStreamingDemandReconciler.h>

#include <stdio.h>

static sirStreamingDemand_t makeAdmissionDemand( uint64_t nUnitId, uint64_t nConsumerId, uint32_t nEpoch, uint8_t nPriority ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit.nContainerId = 12;
	demand.keyUnit.nUnitId = nUnitId;
	demand.keyUnit.nManifestGeneration = 1;
	demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	demand.nConsumerId = nConsumerId;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = nEpoch;
	demand.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	demand.nPriority = nPriority;
	return demand;
}

static bool testHighPriorityAdmittedBeforeEarlierLowPriority( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t foreign = {};
	if ( coordinator.submit( makeAdmissionDemand( 900, 70, 1, 5 ), &foreign ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t szDemands[2] = { makeAdmissionDemand( 10, 90, 1, 1 ), makeAdmissionDemand( 20, 90, 1, 220 ) };
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( szDemands, 2, 1, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nDeferred != 1 ||
	     stats.nTracked != 1 ) {
		return false;
	}
	sirStreamingTicket_t high = {};
	if ( reconciler.findTicket( szDemands[1].keyUnit, &high ) != SIR_STREAMING_OK ||
	     reconciler.findTicket( szDemands[0].keyUnit, &high ) != SIR_STREAMING_NOT_READY ) {
		return false;
	}

	if ( coordinator.cancel( foreign ) != SIR_STREAMING_OK ) {
		return false;
	}
	szDemands[0].nRequestEpoch = 2;
	szDemands[1].nRequestEpoch = 2;
	if ( reconciler.reconcile( szDemands, 2, 2, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nUpdated != 1 ||
	     stats.nDeferred != 0 || stats.nTracked != 2 ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 3, &stats ) != SIR_STREAMING_OK || stats.nCancelled != 2 || stats.nTracked != 0 ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testEqualPriorityPreservesProducerOrder( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t foreign = {};
	if ( coordinator.submit( makeAdmissionDemand( 900, 70, 1, 5 ), &foreign ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingDemand_t szDemands[2] = { makeAdmissionDemand( 40, 90, 1, 20 ), makeAdmissionDemand( 41, 90, 1, 20 ) };
	sirStreamingReconcileStats_t stats = {};
	sirStreamingTicket_t first = {};
	if ( reconciler.reconcile( szDemands, 2, 1, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nDeferred != 1 ||
	     reconciler.findTicket( szDemands[0].keyUnit, &first ) != SIR_STREAMING_OK ||
	     reconciler.findTicket( szDemands[1].keyUnit, &first ) != SIR_STREAMING_NOT_READY ) {
		return false;
	}
	if ( coordinator.cancel( foreign ) != SIR_STREAMING_OK || reconciler.reconcile( nullptr, 0, 2, &stats ) != SIR_STREAMING_OK ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testPinnedCapacityDefersBeforeRetry( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t foreignA = {};
	sirStreamingTicket_t foreignB = {};
	if ( coordinator.submit( makeAdmissionDemand( 901, 70, 1, 5 ), &foreignA ) != SIR_STREAMING_OK ||
	     coordinator.submit( makeAdmissionDemand( 902, 70, 1, 5 ), &foreignB ) != SIR_STREAMING_OK ||
	     coordinator.advance( foreignA, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ||
	     coordinator.cancel( foreignA ) != SIR_STREAMING_OK || coordinator.cancel( foreignB ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingDemand_t szDemands[2] = { makeAdmissionDemand( 50, 90, 1, 1 ), makeAdmissionDemand( 51, 90, 1, 200 ) };
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( szDemands, 2, 1, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nDeferred != 1 ) {
		return false;
	}
	sirStreamingTicket_t high = {};
	if ( reconciler.findTicket( szDemands[1].keyUnit, &high ) != SIR_STREAMING_OK ||
	     reconciler.findTicket( szDemands[0].keyUnit, &high ) != SIR_STREAMING_NOT_READY ) {
		return false;
	}
	if ( coordinator.advance( foreignA, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ||
	     coordinator.advance( foreignA, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( foreignA, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}
	szDemands[0].nRequestEpoch = 2;
	szDemands[1].nRequestEpoch = 2;
	if ( reconciler.reconcile( szDemands, 2, 2, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nDeferred != 0 ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 3, &stats ) != SIR_STREAMING_OK || stats.nCancelled != 2 ||
	     reconciler.shutdown() != SIR_STREAMING_OK || coordinator.shutdown() != SIR_STREAMING_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !testHighPriorityAdmittedBeforeEarlierLowPriority() ) {
		fprintf( stderr, "testHighPriorityAdmittedBeforeEarlierLowPriority failed\n" );
		return 1;
	}
	if ( !testEqualPriorityPreservesProducerOrder() ) {
		fprintf( stderr, "testEqualPriorityPreservesProducerOrder failed\n" );
		return 2;
	}
	if ( !testPinnedCapacityDefersBeforeRetry() ) {
		fprintf( stderr, "testPinnedCapacityDefersBeforeRetry failed\n" );
		return 3;
	}
	return 0;
}
