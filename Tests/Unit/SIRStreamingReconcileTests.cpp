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
#include <World/Partition/SIRWorldPartition.h>

static sirStreamingDemand_t makeReconcileDemand( uint64_t nUnitId, uint32_t nEpoch, sirStreamingTarget_t eTarget ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit.nContainerId = 12;
	demand.keyUnit.nUnitId = nUnitId;
	demand.keyUnit.nManifestGeneration = 1;
	demand.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	demand.nConsumerId = 90;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = nEpoch;
	demand.nDeadlineTick = 100;
	demand.nPriority = 3;
	demand.eTarget = eTarget;
	return demand;
}

static bool testStableTicketAndQueuedReconciliation( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 3 ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 3, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demands[2] = { makeReconcileDemand( 2, 1, SIR_STREAMING_TARGET_ACTIVE ),
	                                    makeReconcileDemand( 3, 1, SIR_STREAMING_TARGET_ACTIVE ) };
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( demands, 2, 1, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 2 || stats.nTracked != 2 ) {
		return false;
	}
	sirStreamingTicket_t a = {};
	if ( reconciler.findTicket( demands[0].keyUnit, &a ) != SIR_STREAMING_OK ) {
		return false;
	}

	demands[0].nRequestEpoch = 2;
	demands[0].nPriority = 70;
	demands[0].nDeadlineTick = 6;
	if ( reconciler.reconcile( demands, 1, 2, &stats ) != SIR_STREAMING_OK || stats.nUpdated != 1 || stats.nCancelled != 1 ||
	     stats.nTracked != 1 ) {
		return false;
	}
	sirStreamingTicket_t preserved = {};
	sirStreamingSnapshot_t snapshot = {};
	if ( reconciler.findTicket( demands[0].keyUnit, &preserved ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( a.nSlot != preserved.nSlot || a.nGeneration != preserved.nGeneration ) {
		return false;
	}
	if ( coordinator.query( a, &snapshot ) != SIR_STREAMING_OK || snapshot.demand.nPriority != 70 || snapshot.demand.nDeadlineTick != 6 ||
	     snapshot.demand.nRequestEpoch != 2 ) {
		return false;
	}

	demands[0].nRequestEpoch = 3;
	demands[1].nRequestEpoch = 3;
	if ( reconciler.reconcile( demands, 2, 3, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nTracked != 2 ) {
		return false;
	}
	if ( reconciler.reconcile( demands, 2, 3, &stats ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 4, &stats ) != SIR_STREAMING_OK || stats.nTracked != 0 || stats.nCancelled != 2 ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testInFlightCancelReentry( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demand = makeReconcileDemand( 1, 1, SIR_STREAMING_TARGET_ACTIVE );
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( &demand, 1, 1, &stats ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingTicket_t first = {};
	if ( reconciler.findTicket( demand.keyUnit, &first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 2, &stats ) != SIR_STREAMING_OK || stats.nRetirePending != 1 ) {
		return false;
	}
	if ( reconciler.shutdown() != SIR_STREAMING_BUSY ) {
		return false;
	}

	demand.nRequestEpoch = 3;
	if ( reconciler.reconcile( &demand, 1, 3, &stats ) != SIR_STREAMING_OK || stats.nDeferred != 1 || stats.nSubmitted != 0 ) {
		return false;
	}
	sirStreamingSnapshot_t snapshot = {};
	if ( coordinator.query( first, &snapshot ) != SIR_STREAMING_OK || !snapshot.bCancellationRequested ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_INVALID_TRANSITION ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}

	demand.nRequestEpoch = 4;
	if ( reconciler.reconcile( &demand, 1, 4, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nDeferred != 0 ) {
		return false;
	}
	sirStreamingTicket_t second = {};
	if ( reconciler.findTicket( demand.keyUnit, &second ) != SIR_STREAMING_OK || first.nSlot != second.nSlot ||
	     first.nGeneration == second.nGeneration ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 5, &stats ) != SIR_STREAMING_OK || stats.nTracked != 0 ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testDemotionAndPromotion( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demand = makeReconcileDemand( 77, 1, SIR_STREAMING_TARGET_PREPARED );
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( &demand, 1, 1, &stats ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingTicket_t first = {};
	if ( reconciler.findTicket( demand.keyUnit, &first ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ) {
		return false;
	}

	demand.nRequestEpoch = 2;
	demand.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	if ( reconciler.reconcile( &demand, 1, 2, &stats ) != SIR_STREAMING_OK || stats.nUpdated != 1 ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_ACTIVE ) != SIR_STREAMING_OK ) {
		return false;
	}

	demand.nRequestEpoch = 3;
	demand.eTarget = SIR_STREAMING_TARGET_PREPARED;
	if ( reconciler.reconcile( &demand, 1, 3, &stats ) != SIR_STREAMING_OK || stats.nCancelled != 1 || stats.nDeferred != 1 ||
	     stats.nRetirePending != 1 ) {
		return false;
	}
	if ( coordinator.advance( first, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( first, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}
	demand.nRequestEpoch = 4;
	if ( reconciler.reconcile( &demand, 1, 4, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 ) {
		return false;
	}
	sirStreamingTicket_t second = {};
	if ( reconciler.findTicket( demand.keyUnit, &second ) != SIR_STREAMING_OK || second.nGeneration == first.nGeneration ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 5, &stats ) != SIR_STREAMING_OK ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testAtomicInputAndCapacity( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 1 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 1, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demands[2] = { makeReconcileDemand( 3, 1, SIR_STREAMING_TARGET_ACTIVE ),
	                                    makeReconcileDemand( 4, 1, SIR_STREAMING_TARGET_ACTIVE ) };
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( demands, 2, 1, &stats ) != SIR_STREAMING_OUT_OF_CAPACITY || coordinator.getStats().nOccupied != 0 ) {
		return false;
	}
	demands[0].nConsumerId = 17;
	if ( reconciler.reconcile( demands, 1, 1, &stats ) != SIR_STREAMING_INVALID_ARGUMENT || coordinator.getStats().nOccupied != 0 ) {
		return false;
	}
	demands[0].nConsumerId = 90;
	if ( reconciler.reconcile( demands, 1, 1, &stats ) != SIR_STREAMING_OK || coordinator.getStats().nOccupied != 1 ) {
		return false;
	}

	demands[0].nRequestEpoch = 2;
	if ( reconciler.reconcile( demands, 1, 2, &stats ) != SIR_STREAMING_OK ) {
		return false;
	}
	sirStreamingTicket_t ticket = {};
	if ( reconciler.findTicket( demands[0].keyUnit, &ticket ) != SIR_STREAMING_OK ) {
		return false;
	}
	if ( coordinator.updateDemand( ticket, makeReconcileDemand( 88, 4, SIR_STREAMING_TARGET_ACTIVE ) ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	if ( coordinator.updateDemand( ticket, makeReconcileDemand( 3, 1, SIR_STREAMING_TARGET_ACTIVE ) ) != SIR_STREAMING_INVALID_ARGUMENT ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 3, &stats ) != SIR_STREAMING_OK || stats.nTracked != 0 ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testWorldPartitionEndToEndDemand( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	alignas( 64 ) uint8_t szWorld[8192] = {};
	const sirWorldPartitionConfig_t config = { 12, 90, 1, 3, false };
	const sirWorldCellDescriptor_t cells[3] = { { 12, 0, 0, 0, 0, 0 }, { 13, 0, 1, 0, 0, 0 }, { 14, 0, 2, 0, 0, 0 } };
	SIRWorldPartition partition;
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( partition.initialize( szWorld, sizeof( szWorld ), config, cells ) != SIR_WORLD_PARTITION_OK ) {
		return false;
	}
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 3 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 3, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirWorldStreamingSource_t source = {};
	source.nSourceId = 1;
	source.nGeneration = 1;
	source.iGridX = 0;
	source.nRadiusCells = 0;
	source.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	sirStreamingDemand_t demands[3] = {};
	uint32_t nCount = 0;
	sirStreamingReconcileStats_t stats = {};
	if ( partition.gather( &source, 1, 0, 1, demands, 3, &nCount ) != SIR_WORLD_PARTITION_OK || nCount != 1 ) {
		return false;
	}
	if ( reconciler.reconcile( demands, nCount, 1, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 ) {
		return false;
	}

	source.iGridX = 1;
	if ( partition.gather( &source, 1, 0, 2, demands, 3, &nCount ) != SIR_WORLD_PARTITION_OK || nCount != 1 ) {
		return false;
	}
	if ( reconciler.reconcile( demands, nCount, 2, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nCancelled != 1 ||
	     coordinator.getStats().nOccupied != 1 ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 3, &stats ) != SIR_STREAMING_OK || stats.nTracked != 0 ) {
		return false;
	}
	partition.shutdown();
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

static bool testPendingCapacityAndDuplicateSet( void ) noexcept
{
	alignas( 64 ) uint8_t szCoordinator[8192] = {};
	alignas( 64 ) uint8_t szLedger[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	if ( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) != SIR_STREAMING_OK ||
	     reconciler.initialize( &coordinator, szLedger, sizeof( szLedger ), 2, 90, 1 ) != SIR_STREAMING_OK ) {
		return false;
	}

	sirStreamingDemand_t demands[2] = { makeReconcileDemand( 1, 1, SIR_STREAMING_TARGET_ACTIVE ),
	                                    makeReconcileDemand( 2, 1, SIR_STREAMING_TARGET_ACTIVE ) };
	sirStreamingReconcileStats_t stats = {};
	if ( reconciler.reconcile( demands, 2, 1, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 2 ) {
		return false;
	}
	sirStreamingTicket_t pinned = {};
	if ( reconciler.findTicket( demands[1].keyUnit, &pinned ) != SIR_STREAMING_OK ||
	     coordinator.advance( pinned, SIR_STREAMING_STAGE_PREPARING ) != SIR_STREAMING_OK ) {
		return false;
	}

	demands[0].nRequestEpoch = 2;
	demands[1] = makeReconcileDemand( 3, 2, SIR_STREAMING_TARGET_ACTIVE );
	if ( reconciler.reconcile( demands, 2, 2, &stats ) != SIR_STREAMING_OK || stats.nCancelled != 1 || stats.nDeferred != 1 ||
	     stats.nTracked != 2 || stats.nRetirePending != 1 ) {
		return false;
	}
	if ( coordinator.getStats().nCancellationPending != 1 ) {
		return false;
	}

	demands[0].nRequestEpoch = 3;
	demands[1].nRequestEpoch = 3;
	if ( reconciler.reconcile( demands, 2, 3, &stats ) != SIR_STREAMING_OK || stats.nDeferred != 1 || stats.nSubmitted != 0 ) {
		return false;
	}
	if ( coordinator.advance( pinned, SIR_STREAMING_STAGE_PREPARED ) != SIR_STREAMING_OK ||
	     coordinator.advance( pinned, SIR_STREAMING_STAGE_RETIRING ) != SIR_STREAMING_OK ||
	     coordinator.advance( pinned, SIR_STREAMING_STAGE_EMPTY ) != SIR_STREAMING_OK ) {
		return false;
	}

	demands[0].nRequestEpoch = 4;
	demands[1].nRequestEpoch = 4;
	if ( reconciler.reconcile( demands, 2, 4, &stats ) != SIR_STREAMING_OK || stats.nSubmitted != 1 || stats.nDeferred != 0 ) {
		return false;
	}
	sirStreamingDemand_t duplicate[2] = { demands[0], demands[0] };
	duplicate[0].nRequestEpoch = 5;
	duplicate[1].nRequestEpoch = 5;
	if ( reconciler.reconcile( duplicate, 2, 5, &stats ) != SIR_STREAMING_DUPLICATE || coordinator.getStats().nOccupied != 2 ) {
		return false;
	}
	if ( reconciler.reconcile( nullptr, 0, 5, &stats ) != SIR_STREAMING_OK || stats.nTracked != 0 || stats.nCancelled != 2 ) {
		return false;
	}
	return reconciler.shutdown() == SIR_STREAMING_OK && coordinator.shutdown() == SIR_STREAMING_OK;
}

int main( void )
{
	if ( !testStableTicketAndQueuedReconciliation() ) {
		return 1;
	}
	if ( !testInFlightCancelReentry() ) {
		return 2;
	}
	if ( !testDemotionAndPromotion() ) {
		return 3;
	}
	if ( !testAtomicInputAndCapacity() ) {
		return 4;
	}
	if ( !testWorldPartitionEndToEndDemand() ) {
		return 5;
	}
	if ( !testPendingCapacityAndDuplicateSet() ) {
		return 6;
	}
	return 0;
}
