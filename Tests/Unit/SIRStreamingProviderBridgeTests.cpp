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

#include <Streaming/SIRStreamingProviderBridge.h>
#include <Streaming/SIRStreamingDemandReconciler.h>
#include <World/Partition/SIRWorldPartition.h>

#include <stdio.h>

typedef struct sirTestProviderState_s
{
	uint32_t nPrepare;
	uint32_t nCancel;
	uint32_t nActivate;
	uint32_t nRetire;
	uint64_t nLastUnit;
	bool bAcceptPrepare;
	bool bAcceptCancel;
	bool bAcceptActivate;
	bool bAcceptRetire;
} sirTestProviderState_t;

static bool beginPrepare( void *pContext, sirStreamingTicket_t ticket, const sirStreamingDemand_t *pDemand ) noexcept
{
	( void )ticket;
	sirTestProviderState_t *pState = ( sirTestProviderState_t * )pContext;
	++pState->nPrepare;
	pState->nLastUnit = pDemand->keyUnit.nUnitId;
	return pState->bAcceptPrepare;
}

static bool beginCancel( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	( void )ticket;
	sirTestProviderState_t *pState = ( sirTestProviderState_t * )pContext;
	++pState->nCancel;
	return pState->bAcceptCancel;
}

static bool beginActivate( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	( void )ticket;
	sirTestProviderState_t *pState = ( sirTestProviderState_t * )pContext;
	++pState->nActivate;
	return pState->bAcceptActivate;
}

static bool beginRetire( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	( void )ticket;
	sirTestProviderState_t *pState = ( sirTestProviderState_t * )pContext;
	++pState->nRetire;
	return pState->bAcceptRetire;
}

static sirStreamingDemand_t makeDemand( uint64_t nUnitId, sirStreamingTarget_t eTarget ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit.nContainerId = 7;
	demand.keyUnit.nUnitId = nUnitId;
	demand.keyUnit.nManifestGeneration = 1;
	demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	demand.nConsumerId = 33;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = 1;
	demand.eTarget = eTarget;
	return demand;
}

static sirStreamingProviderCallbacks_t makeCallbacks( sirTestProviderState_t& state ) noexcept
{
	sirStreamingProviderCallbacks_t callbacks = {};
	callbacks.pContext = &state;
	callbacks.pBeginPrepare = beginPrepare;
	callbacks.pCancelPrepare = beginCancel;
	callbacks.pBeginActivate = beginActivate;
	callbacks.pBeginRetire = beginRetire;
	return callbacks;
}

static bool pump( SIRStreamingProviderBridge& bridge, uint32_t nSlots = 1 ) noexcept
{
	sirStreamingBridgeStats_t stats = {};
	return bridge.pump( nSlots, &stats ) == SIR_STREAMING_OK;
}

static bool testPrepareActivateRetire( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, true, true, true, true };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 1 ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), makeCallbacks( provider ) ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t ticket = {};
	const sirStreamingDemand_t demand = makeDemand( 501, SIR_STREAMING_TARGET_ACTIVE );
	if ( coordinator.submit( demand, &ticket ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	if ( provider.nPrepare != 1 || provider.nLastUnit != 501 ) {
		return false;
	}
	if ( !pump( bridge ) || provider.nPrepare != 1 ) {
		return false;
	}
	if ( bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_INVALID_TRANSITION ||
	     bridge.acknowledgeActivated( ticket, true ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( ticket, true ) != SIR_STREAMING_OK || !pump( bridge ) || provider.nActivate != 1 ) {
		return false;
	}
	if ( !pump( bridge ) || provider.nActivate != 1 ) {
		return false;
	}
	if ( bridge.acknowledgeActivated( ticket, true ) != SIR_STREAMING_OK ||
	     bridge.acknowledgeActivated( ticket, true ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}

	sirStreamingSnapshot_t snapshot = {};
	if ( coordinator.query( ticket, &snapshot ) != SIR_STREAMING_OK || snapshot.eStage != SIR_STREAMING_STAGE_ACTIVE ) {
		return false;
	}
	if ( coordinator.cancel( ticket ) != SIR_STREAMING_OK || !pump( bridge ) || provider.nRetire != 1 ) {
		return false;
	}
	if ( bridge.shutdown() != SIR_STREAMING_BUSY || coordinator.query( ticket, &snapshot ) != SIR_STREAMING_OK ||
	     snapshot.eStage != SIR_STREAMING_STAGE_RETIRING ) {
		return false;
	}
	if ( bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_OK || bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_STALE_TICKET ) {
		return false;
	}
	if ( bridge.shutdown() != SIR_STREAMING_OK || coordinator.shutdown() != SIR_STREAMING_OK ) {
		return false;
	}
	return true;
}

static bool testCancellationDuringPrepare( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, true, false, true, false };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 1 ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), makeCallbacks( provider ) ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t ticket = {};
	if ( coordinator.submit( makeDemand( 1, SIR_STREAMING_TARGET_PREPARED ), &ticket ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	if ( coordinator.cancel( ticket ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	if ( provider.nCancel != 1 || bridge.shutdown() != SIR_STREAMING_BUSY ) {
		return false;
	}
	provider.bAcceptCancel = true;
	if ( !pump( bridge ) || provider.nCancel != 2 ) {
		return false;
	}
	if ( !pump( bridge ) || provider.nCancel != 2 ) {
		return false;
	}
	// The worker completes despite cancellation. The bridge must retire this
	// exact prepared revision without issuing activation.
	if ( bridge.acknowledgePrepared( ticket, true ) != SIR_STREAMING_OK || !pump( bridge ) || provider.nRetire != 1 ) {
		return false;
	}
	if ( coordinator.getStats().nRetiring != 0 ) {
		return false;
	}
	provider.bAcceptRetire = true;
	if ( !pump( bridge ) || provider.nRetire != 2 || bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_OK ) {
		return false;
	}
	return bridge.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testActivationCancelledAndFailure( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, true, true, true, true };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 1 ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), makeCallbacks( provider ) ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t ticket = {};
	if ( coordinator.submit( makeDemand( 8, SIR_STREAMING_TARGET_ACTIVE ), &ticket ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( ticket, true ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	if ( coordinator.cancel( ticket ) != SIR_STREAMING_OK || !pump( bridge ) || provider.nRetire != 0 ) {
		return false;
	}
	if ( bridge.acknowledgeActivated( ticket, true ) != SIR_STREAMING_OK || !pump( bridge ) || provider.nRetire != 1 ) {
		return false;
	}
	if ( coordinator.getStats().nActive != 0 || bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t next = {};
	if ( coordinator.submit( makeDemand( 9, SIR_STREAMING_TARGET_ACTIVE ), &next ) != SIR_STREAMING_OK || next.nSlot != ticket.nSlot ||
	     next.nGeneration == ticket.nGeneration ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( ticket, true ) != SIR_STREAMING_STALE_TICKET || !pump( bridge ) ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( next, true ) != SIR_STREAMING_OK || !pump( bridge ) ||
	     bridge.acknowledgeActivated( next, false ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.getStats().nFailed != 1 || !pump( bridge ) || bridge.acknowledgeRetired( next ) != SIR_STREAMING_OK ) {
		return false;
	}
	return bridge.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testBusyAndSlotEnumeration( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, false, true, true, true };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 2 ) != SIR_STREAMING_OK ) {
		return false;
	}
	const sirStreamingProviderCallbacks_t callbacks = makeCallbacks( provider );
	if ( bridge.initialize( &coordinator, szBridge + 1, sizeof( szBridge ) - 1, callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t ticketA = {};
	sirStreamingTicket_t ticketB = {};
	if ( coordinator.submit( makeDemand( 10, SIR_STREAMING_TARGET_PREPARED ), &ticketA ) != SIR_STREAMING_OK ||
	     coordinator.submit( makeDemand( 11, SIR_STREAMING_TARGET_PREPARED ), &ticketB ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( bridge.pump( 0, nullptr ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	sirStreamingTicket_t enumerated = {};
	sirStreamingSnapshot_t snapshot = {};
	if ( coordinator.querySlot( 2, &enumerated, &snapshot ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     coordinator.querySlot( 0, &enumerated, &snapshot ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( !pump( bridge ) || !pump( bridge ) || provider.nPrepare != 2 ) {
		return false;
	}
	provider.bAcceptPrepare = true;
	if ( !pump( bridge ) || !pump( bridge ) || provider.nPrepare != 4 ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( ticketA, false ) != SIR_STREAMING_OK ||
	     bridge.acknowledgePrepared( ticketB, true ) != SIR_STREAMING_OK || !pump( bridge, 2 ) ) {
		return false;
	}
	if ( provider.nRetire != 1 || coordinator.getStats().nPrepared != 1 ) {
		return false;
	}
	if ( coordinator.cancel( ticketB ) != SIR_STREAMING_OK || !pump( bridge, 2 ) || provider.nRetire != 2 ) {
		return false;
	}
	if ( bridge.acknowledgeRetired( ticketA ) != SIR_STREAMING_OK || bridge.acknowledgeRetired( ticketB ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.querySlot( 0, &enumerated, &snapshot ) != SIR_STREAMING_NOT_READY ) {
		return false;
	}
	return bridge.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testDowngradeDuringActivation( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, true, true, true, true };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 1 ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), makeCallbacks( provider ) ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t ticket = {};
	sirStreamingDemand_t demand = makeDemand( 90, SIR_STREAMING_TARGET_ACTIVE );
	if ( coordinator.submit( demand, &ticket ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( ticket, true ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}

	demand.nRequestEpoch = 2;
	demand.eTarget = SIR_STREAMING_TARGET_PREPARED;
	if ( coordinator.updateDemand( ticket, demand ) != SIR_STREAMING_OK ||
	     bridge.acknowledgeActivated( ticket, true ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingSnapshot_t snapshot = {};
	if ( coordinator.query( ticket, &snapshot ) != SIR_STREAMING_OK || !snapshot.bCancellationRequested ||
	     snapshot.eStage != SIR_STREAMING_STAGE_PREPARED ) {
		return false;
	}
	if ( !pump( bridge ) || provider.nRetire != 1 || bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_OK ) {
		return false;
	}
	return bridge.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool reconcileWorldSource( SIRWorldPartition& partition, SIRStreamingDemandReconciler& reconciler,
                                  const sirWorldStreamingSource_t& source, uint32_t nEpoch, uint32_t nExpected, uint32_t nSubmitted,
                                  uint32_t nCancelled, sirStreamingDemand_t *pDemands ) noexcept
{
	uint32_t nRequired = 0;
	sirStreamingReconcileStats_t stats = {};
	if ( partition.gather( &source, 1, 0, nEpoch, pDemands, 2, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != nExpected ) {
		return false;
	}
	if ( reconciler.reconcile( pDemands, nRequired, nEpoch, &stats ) != SIR_STREAMING_OK ) {
		return false;
	}
	return stats.nSubmitted == nSubmitted && stats.nCancelled == nCancelled;
}

static bool testPartitionReconcileBridge( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	alignas( 64 ) uint8_t szReconcile[4096] = {};
	alignas( 64 ) uint8_t szWorld[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	SIRStreamingDemandReconciler reconciler;
	SIRWorldPartition partition;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, true, true, true, true };
	const sirWorldPartitionConfig_t config = { 7, 33, 1, 2, false };
	const sirWorldCellDescriptor_t szCells[2] = { { 11, 0, 0, 0, 0, 0 }, { 12, 0, 1, 0, 0, 0 } };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szReconcile, sizeof( szReconcile ), 2, 33, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), makeCallbacks( provider ) ) != SIR_STREAMING_OK ||
	     partition.initialize( szWorld, sizeof( szWorld ), config, szCells ) != SIR_WORLD_PARTITION_OK ) {
		return false;
	}

	sirWorldStreamingSource_t source = {};
	source.nSourceId = 1;
	source.nGeneration = 1;
	source.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	source.nPriority = 100;
	sirStreamingDemand_t szDemands[2] = {};
	if ( !reconcileWorldSource( partition, reconciler, source, 1, 1, 1, 0, szDemands ) ) {
		return false;
	}

	sirStreamingTicket_t first = {};
	if ( reconciler.findTicket( szDemands[0].keyUnit, &first ) != SIR_STREAMING_OK || !pump( bridge, 2 ) ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( first, true ) != SIR_STREAMING_OK || !pump( bridge, 2 ) ||
	     bridge.acknowledgeActivated( first, true ) != SIR_STREAMING_OK ) {
		return false;
	}

	source.iGridX = 1;
	if ( !reconcileWorldSource( partition, reconciler, source, 2, 1, 1, 1, szDemands ) ) {
		return false;
	}
	sirStreamingTicket_t second = {};
	if ( reconciler.findTicket( szDemands[0].keyUnit, &second ) != SIR_STREAMING_OK || !pump( bridge, 2 ) ) {
		return false;
	}
	if ( provider.nRetire != 1 || provider.nPrepare != 2 || bridge.acknowledgeRetired( first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( bridge.acknowledgePrepared( second, true ) != SIR_STREAMING_OK || !pump( bridge, 2 ) ||
	     bridge.acknowledgeActivated( second, true ) != SIR_STREAMING_OK ) {
		return false;
	}

	source.iGridX = 100;
	if ( !reconcileWorldSource( partition, reconciler, source, 3, 0, 0, 1, szDemands ) || !pump( bridge, 2 ) ||
	     bridge.acknowledgeRetired( second ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( reconciler.shutdown() != SIR_STREAMING_OK || bridge.shutdown() != SIR_STREAMING_OK ||
	     coordinator.shutdown() != SIR_STREAMING_OK ) {
		return false;
	}
	partition.shutdown();
	return true;
}

static bool testOutOfBandReleaseFailsClosed( void ) noexcept
{
	alignas( 64 ) uint8_t szTickets[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirTestProviderState_t provider = { 0, 0, 0, 0, 0, true, true, true, true };
	if ( coordinator.initialize( szTickets, sizeof( szTickets ), 1 ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), makeCallbacks( provider ) ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingTicket_t ticket = {};
	if ( coordinator.submit( makeDemand( 50, SIR_STREAMING_TARGET_ACTIVE ), &ticket ) != SIR_STREAMING_OK || !pump( bridge ) ) {
		return false;
	}
	// Deliberate invalid external mutation: a provider still holds its job.
	// Shutdown must not free the bridge storage after this bypass.
	if ( coordinator.advance( ticket, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingTicket_t reused = {};
	if ( coordinator.submit( makeDemand( 51, SIR_STREAMING_TARGET_ACTIVE ), &reused ) != SIR_STREAMING_OK || !pump( bridge ) ||
	     provider.nPrepare != 1 ) {
		return false;
	}
	if ( coordinator.cancel( reused ) != SIR_STREAMING_OK ) {
		return false;
	}
	return bridge.shutdown() == SIR_STREAMING_BUSY;
}

int main( void )
{
	if ( !testPrepareActivateRetire() ) {
		fprintf( stderr, "testPrepareActivateRetire failed\n" );
		return 1;
	}
	if ( !testCancellationDuringPrepare() ) {
		fprintf( stderr, "testCancellationDuringPrepare failed\n" );
		return 2;
	}
	if ( !testActivationCancelledAndFailure() ) {
		fprintf( stderr, "testActivationCancelledAndFailure failed\n" );
		return 3;
	}
	if ( !testBusyAndSlotEnumeration() ) {
		fprintf( stderr, "testBusyAndSlotEnumeration failed\n" );
		return 4;
	}
	if ( !testDowngradeDuringActivation() ) {
		fprintf( stderr, "testDowngradeDuringActivation failed\n" );
		return 5;
	}
	if ( !testPartitionReconcileBridge() ) {
		fprintf( stderr, "testPartitionReconcileBridge failed\n" );
		return 6;
	}
	if ( !testOutOfBandReleaseFailsClosed() ) {
		fprintf( stderr, "testOutOfBandReleaseFailsClosed failed\n" );
		return 7;
	}
	return 0;
}
