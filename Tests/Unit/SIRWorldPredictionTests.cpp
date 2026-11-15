/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Prediction/World/SIRWorldTrajectoryPredictor.h>

#include <stdio.h>
#include <string.h>

#define REQUIRE( x )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( x ) ) {                                                                                                                    \
			fprintf( stderr, "prediction assertion %u: %s\n", __LINE__, #x );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testPredictivePartition( void ) noexcept
{
	sirWorldTrajectorySource_t actor = {};
	actor.source.nSourceId = 11;
	actor.source.nGeneration = 1;
	actor.source.iGridX = 1;
	actor.source.nDeadlineTick = 10;
	actor.source.nPriority = 100;
	actor.source.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	actor.iVelocityX16 = 65536;
	actor.nWorldGeneration = 9;
	const sirWorldPredictionConfig_t config = { 9, 3, 1, 2 };
	const sirWorldCoAccessHint_t hint = { 2, 0, 0, 8, 0, 0, 20 };
	uint32_t nRequired = 0;
	sirWorldStreamingSource_t szPredicted[16] = {};
	memset( szPredicted, 0xCC, sizeof( szPredicted ) );
	uint8_t szBefore[sizeof( szPredicted )] = {};
	memcpy( szBefore, szPredicted, sizeof( szPredicted ) );
	REQUIRE( SIRWorldTrajectoryPredictor::plan( &actor, 1, &hint, 1, config, szPredicted, 2, &nRequired ) ==
	         SIR_WORLD_PREDICTION_OUT_OF_CAPACITY );
	REQUIRE( nRequired == 5 );
	REQUIRE( memcmp( szBefore, szPredicted, sizeof( szPredicted ) ) == 0 );
	REQUIRE( SIRWorldTrajectoryPredictor::plan( &actor, 1, &hint, 1, config, szPredicted, 16, &nRequired ) == SIR_WORLD_PREDICTION_OK );
	REQUIRE( szPredicted[0].iGridX == 1 && szPredicted[0].eTarget == SIR_STREAMING_TARGET_ACTIVE );
	REQUIRE( szPredicted[1].iGridX == 2 && szPredicted[1].eTarget == SIR_STREAMING_TARGET_PREPARED );
	REQUIRE( szPredicted[2].iGridX == 8 && szPredicted[2].nPriority == 20 );
	REQUIRE( szPredicted[3].iGridX == 3 && szPredicted[4].iGridX == 4 );

	sirWorldCellDescriptor_t cells[11] = {};
	for ( uint32_t iCell = 0; iCell < 11; ++iCell ) {
		cells[iCell].nCellId = iCell + 100;
		cells[iCell].iGridX = ( int32_t )iCell;
	}
	cells[0].nFlags = SIR_WORLD_CELL_FLAG_ALWAYS_LOADED;
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRWorldPartition partition;
	const sirWorldPartitionConfig_t partitionConfig = { 55, 13, 9, 11, false };
	REQUIRE( partition.initialize( szStorage, sizeof( szStorage ), partitionConfig, cells ) == SIR_WORLD_PARTITION_OK );
	sirStreamingDemand_t demands[16] = {};
	uint32_t nDemands = 0;
	REQUIRE( partition.gather( szPredicted, nRequired, 0, 1, demands, 16, &nDemands ) == SIR_WORLD_PARTITION_OK );
	REQUIRE( nDemands == 6 );
	REQUIRE( demands[0].keyUnit.nUnitId == 100 && demands[0].eTarget == SIR_STREAMING_TARGET_ACTIVE );
	REQUIRE( demands[1].keyUnit.nUnitId == 101 && demands[1].eTarget == SIR_STREAMING_TARGET_ACTIVE );
	REQUIRE( demands[5].keyUnit.nUnitId == 108 && demands[5].nPriority == 20 );

	actor.nWorldGeneration = 8;
	REQUIRE( SIRWorldTrajectoryPredictor::plan( &actor, 1, &hint, 1, config, szPredicted, 16, &nRequired ) ==
	         SIR_WORLD_PREDICTION_STALE_WORLD );
	actor.nWorldGeneration = 9;
	actor.bTeleported = true;
	actor.source.iGridX = 9;
	REQUIRE( SIRWorldTrajectoryPredictor::plan( &actor, 1, &hint, 1, config, szPredicted, 16, &nRequired ) == SIR_WORLD_PREDICTION_OK );
	REQUIRE( nRequired == 4 );
	for ( uint32_t i = 0; i < nRequired; ++i ) {
		REQUIRE( szPredicted[i].iGridX == 9 );
	}
	partition.shutdown();
	return true;
}

int main( void )
{
	return testPredictivePartition() ? 0 : 1;
}
