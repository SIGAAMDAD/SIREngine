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
#include <Streaming/SIRStreamingProviderBridge.h>

#include <stdio.h>

static sirStreamingDemand_t makeValidationDemand( uint64_t nUnitId, uint32_t nEpoch ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit.nContainerId = 1;
	demand.keyUnit.nUnitId = nUnitId;
	demand.keyUnit.nManifestGeneration = 1;
	demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	demand.nConsumerId = 12;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = nEpoch;
	demand.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	return demand;
}

static bool providerPrepare( void *pContext, sirStreamingTicket_t ticket, const sirStreamingDemand_t *pDemand ) noexcept
{
	( void )pContext;
	( void )ticket;
	( void )pDemand;
	return true;
}

static bool providerOperation( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	( void )pContext;
	( void )ticket;
	return true;
}

static sirStreamingProviderCallbacks_t validCallbacks( void ) noexcept
{
	sirStreamingProviderCallbacks_t callbacks = {};
	callbacks.pBeginPrepare = providerPrepare;
	callbacks.pCancelPrepare = providerOperation;
	callbacks.pBeginActivate = providerOperation;
	callbacks.pBeginRetire = providerOperation;
	return callbacks;
}

static bool testReconcilerValidation( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[4096] = {};
	alignas( 64 ) uint8_t szLedger[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	sirStreamingReconcileStats_t stats = {};
	sirStreamingTicket_t ticket = {};
	const sirStreamingUnitKey_t missing = makeValidationDemand( 900, 1 ).keyUnit;

	if ( reconciler.shutdown() != SIR_STREAMING_NOT_READY || reconciler.reconcile( nullptr, 0, 1, &stats ) != SIR_STREAMING_NOT_READY ||
	     reconciler.findTicket( missing, &ticket ) != SIR_STREAMING_NOT_READY || reconciler.getTrackedCount() != 0 ||
	     coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ) {
		return false;
	}

	if ( reconciler.initialize( nullptr, szLedger, sizeof( szLedger ), 2, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, nullptr, sizeof( szLedger ), 2, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 0, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 0, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 12, 0 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 3, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger, 1, 2, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger + 1, sizeof( szLedger ) - 1, 2, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 12, 1 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 12, 1 ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}

	sirStreamingDemand_t demand = makeValidationDemand( 100, 1 );
	if ( reconciler.reconcile( &demand, 1, 1, nullptr ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.reconcile( &demand, 1, 0, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.reconcile( &demand, 3, 1, &stats ) != SIR_STREAMING_OUT_OF_CAPACITY ||
	     reconciler.reconcile( nullptr, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.findTicket( missing, nullptr ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.findTicket( missing, &ticket ) != SIR_STREAMING_NOT_READY ) {
		return false;
	}

	sirStreamingDemand_t bad = demand;
	bad.nConsumerId = 0;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.nConsumerGeneration = 2;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.nRequestEpoch = 2;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.keyUnit.nUnitId = 0;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.keyUnit.nManifestGeneration = 0;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.keyUnit.eKind = SIR_STREAMING_UNIT_COUNT;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.eTarget = ( sirStreamingTarget_t )0;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	bad = demand;
	bad.eTarget = ( sirStreamingTarget_t )3;
	if ( reconciler.reconcile( &bad, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	sirStreamingDemand_t repeated[2] = { demand, demand };
	if ( reconciler.reconcile( repeated, 2, 1, &stats ) != SIR_STREAMING_DUPLICATE ||
	     reconciler.reconcile( &demand, 1, 1, &stats ) != SIR_STREAMING_OK ||
	     reconciler.reconcile( &demand, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     reconciler.findTicket( demand.keyUnit, &ticket ) != SIR_STREAMING_OK || reconciler.shutdown() != SIR_STREAMING_BUSY ) {
		return false;
	}

	if ( coordinator.cancel( ticket ) != SIR_STREAMING_OK ||
	     reconciler.findTicket( demand.keyUnit, &ticket ) != SIR_STREAMING_STALE_TICKET ||
	     reconciler.reconcile( nullptr, 0, 2, &stats ) != SIR_STREAMING_OK || reconciler.shutdown() != SIR_STREAMING_OK ||
	     coordinator.shutdown() != SIR_STREAMING_OK ) {
		return false;
	}
	return true;
}

static bool testBridgeValidation( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingProviderBridge bridge;
	sirStreamingBridgeStats_t stats = {};
	sirStreamingTicket_t ticket = {};
	sirStreamingProviderCallbacks_t callbacks = validCallbacks();

	if ( bridge.shutdown() != SIR_STREAMING_NOT_READY || bridge.pump( 1, &stats ) != SIR_STREAMING_NOT_READY ||
	     bridge.acknowledgePrepared( ticket, true ) != SIR_STREAMING_STALE_TICKET ||
	     bridge.acknowledgeActivated( ticket, true ) != SIR_STREAMING_STALE_TICKET ||
	     bridge.acknowledgeRetired( ticket ) != SIR_STREAMING_STALE_TICKET ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ) {
		return false;
	}

	if ( bridge.initialize( nullptr, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     bridge.initialize( &coordinator, nullptr, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	callbacks.pBeginPrepare = nullptr;
	if ( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	callbacks = validCallbacks();
	callbacks.pCancelPrepare = nullptr;
	if ( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	callbacks = validCallbacks();
	callbacks.pBeginActivate = nullptr;
	if ( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	callbacks = validCallbacks();
	callbacks.pBeginRetire = nullptr;
	if ( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	callbacks = validCallbacks();

	if ( bridge.initialize( &coordinator, szBridge, 1, callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     bridge.initialize( &coordinator, szBridge + 1, sizeof( szBridge ) - 1, callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}

	sirStreamingDemand_t demand = makeValidationDemand( 200, 1 );
	if ( coordinator.submit( demand, &ticket ) != SIR_STREAMING_OK ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     coordinator.advance( ticket, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_OK ||
	     bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), callbacks ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     bridge.pump( 0, &stats ) != SIR_STREAMING_INVALID_ARGUMENT || bridge.pump( 1, nullptr ) != SIR_STREAMING_INVALID_ARGUMENT ||
	     bridge.pump( 2, &stats ) != SIR_STREAMING_OK || bridge.shutdown() != SIR_STREAMING_OK ||
	     coordinator.shutdown() != SIR_STREAMING_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !testReconcilerValidation() ) {
		fprintf( stderr, "testReconcilerValidation failed\n" );
		return 1;
	}
	if ( !testBridgeValidation() ) {
		fprintf( stderr, "testBridgeValidation failed\n" );
		return 2;
	}
	return 0;
}
