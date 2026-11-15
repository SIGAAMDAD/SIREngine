/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the Mozilla Public License, v2.
===========================================================================
*/

#include <Prediction/World/SIRWorldStreamingPlanner.h>

#include <stdio.h>

#define REQUIRE( b )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( b ) ) {                                                                                                                    \
			fprintf( stderr, "world streaming planner assertion %d: %s\n", __LINE__, #b );                                                 \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

int main( void )
{
	alignas( 64 ) uint8_t szWorldStorage[8192] = {};
	alignas( 64 ) uint8_t szCoordinatorStorage[8192] = {};
	alignas( 64 ) uint8_t szReconcilerStorage[8192] = {};
	sirWorldCellDescriptor_t szCells[10] = {};
	for ( uint32_t iCell = 0; iCell < 10; ++iCell ) {
		szCells[iCell].nCellId = 100 + iCell;
		szCells[iCell].iGridX = ( int32_t )iCell;
	}
	szCells[0].nFlags = SIR_WORLD_CELL_FLAG_ALWAYS_LOADED;

	SIRWorldPartition partition;
	SIRStreamingCoordinator coordinator;
	SIRStreamingDemandReconciler reconciler;
	REQUIRE( partition.initialize( szWorldStorage, sizeof( szWorldStorage ), { 50, 90, 9, 10, false }, szCells ) ==
	         SIR_WORLD_PARTITION_OK );
	REQUIRE( coordinator.initialize( szCoordinatorStorage, sizeof( szCoordinatorStorage ), 10 ) == SIR_STREAMING_OK );
	REQUIRE( reconciler.initialize( &coordinator, szReconcilerStorage, sizeof( szReconcilerStorage ), 10, 90, 9 ) == SIR_STREAMING_OK );

	sirWorldStreamingSource_t szPredicted[16] = {};
	sirStreamingDemand_t szDemands[10] = {};
	SIRWorldStreamingPlanner planner;
	REQUIRE( planner.initialize( &partition, &reconciler, { szPredicted, szDemands, 16, 10 } ) );

	sirWorldTrajectorySource_t actor = {};
	actor.source.nSourceId = 1;
	actor.source.nGeneration = 1;
	actor.source.iGridX = 2;
	actor.source.nRadiusCells = 0;
	actor.source.nPriority = 100;
	actor.source.nDeadlineTick = 5;
	actor.source.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	actor.iVelocityX16 = 65536;
	actor.nWorldGeneration = 9;
	sirWorldStreamingPlanInput_t input = {};
	input.pActors = &actor;
	input.nActors = 1;
	input.prediction = { 9, 2, 1, 1 };
	input.nRequestEpoch = 1;
	sirWorldStreamingPlanStats_t stats = {};
	REQUIRE( planner.update( input, &stats ) == SIR_WORLD_STREAM_PLAN_OK );
	REQUIRE( stats.nPredictedSources == 3 && stats.nPartitionDemands == 4 && stats.reconcile.nSubmitted == 4 );
	const sirStreamingUnitKey_t keyFirst = szDemands[1].keyUnit;
	sirStreamingTicket_t first = {};
	REQUIRE( reconciler.findTicket( keyFirst, &first ) == SIR_STREAMING_OK );

	actor.nWorldGeneration = 8;
	input.nRequestEpoch = 2;
	REQUIRE( planner.update( input, &stats ) == SIR_WORLD_STREAM_PLAN_STALE_WORLD );
	REQUIRE( coordinator.getStats().nOccupied == 4 );
	actor.nWorldGeneration = 9;
	actor.bTeleported = true;
	actor.source.iGridX = 8;
	REQUIRE( planner.update( input, &stats ) == SIR_WORLD_STREAM_PLAN_OK );
	REQUIRE( stats.nPartitionDemands == 2 && stats.reconcile.nCancelled == 3 );
	REQUIRE( reconciler.findTicket( keyFirst, &first ) == SIR_STREAMING_NOT_READY );

	input.nRequestEpoch = 3;
	actor.bTeleported = false;
	actor.source.iGridX = 2;
	REQUIRE( planner.update( input, &stats ) == SIR_WORLD_STREAM_PLAN_OK );
	REQUIRE( stats.nPartitionDemands == 4 );

	input.nRequestEpoch = 4;
	input.prediction.nHorizonTicks = 32;
	REQUIRE( planner.update( input, &stats ) == SIR_WORLD_STREAM_PLAN_PREDICTION_FAILED );
	REQUIRE( stats.ePrediction == SIR_WORLD_PREDICTION_OUT_OF_CAPACITY );
	REQUIRE( coordinator.getStats().nOccupied == 4 );

	planner.shutdown();
	sirStreamingReconcileStats_t cleared = {};
	REQUIRE( reconciler.reconcile( nullptr, 0, 4, &cleared ) == SIR_STREAMING_OK );
	REQUIRE( reconciler.shutdown() == SIR_STREAMING_OK );
	REQUIRE( coordinator.shutdown() == SIR_STREAMING_OK );
	partition.shutdown();
	return 0;
}
