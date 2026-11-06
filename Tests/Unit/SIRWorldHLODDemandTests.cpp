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

#include <World/HLOD/SIRWorldHLODDemand.h>

#include <stdio.h>

#define SIR_DEMAND_CHECK( expression )                                                                                                     \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "HLOD demand failure: %s at line %d\n", #expression, __LINE__ );                                              \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static sirWorldHLODDemand_t sirDemand( uint32_t nNode, uint64_t nRevision, uint32_t nWorld ) noexcept
{
	return { { ( uint64_t )nNode + 1, 3, 0 }, nRevision, nWorld, nNode };
}

int main( void )
{
	sirWorldHLODDemand_t szPrior[] = { sirDemand( 1, 7, 4 ), sirDemand( 3, 7, 4 ) };
	sirWorldHLODDemand_t szNext[] = { sirDemand( 1, 7, 4 ), sirDemand( 2, 7, 4 ), sirDemand( 3, 8, 4 ) };
	sirWorldHLODDemandOperation_t szOperations[6] = {};
	uint32_t nCount = 99;
	sirWorldHLODDemandDeltaInput_t input = { szPrior, 2, szNext, 3 };
	sirWorldHLODDemandDeltaOutput_t output = { szOperations, 6, &nCount };

	// Unchanged node 1 is ignored. Add node 2; replace the changed revision at node 3.
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_DEMAND_CHECK( nCount == 3 );
	SIR_DEMAND_CHECK( szOperations[0].eAction == SIR_WORLD_HLOD_DEMAND_REQUEST && szOperations[0].demand.nNodeIndex == 2 );
	SIR_DEMAND_CHECK( szOperations[1].eAction == SIR_WORLD_HLOD_DEMAND_CANCEL && szOperations[1].demand.nNodeIndex == 3 );
	SIR_DEMAND_CHECK( szOperations[1].demand.nSourceRevision == 7 );
	SIR_DEMAND_CHECK( szOperations[2].eAction == SIR_WORLD_HLOD_DEMAND_REQUEST && szOperations[2].demand.nSourceRevision == 8 );

	// Identical snapshots produce no work.
	input.pPrevious = szNext;
	input.nPreviousCount = 3;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_OK && nCount == 0 );

	// A new world generation cancels every old request before its replacement.
	for ( uint32_t i = 0; i < 3; ++i ) {
		szNext[i].nWorldGeneration = 5;
	}
	sirWorldHLODDemand_t szOld[] = { sirDemand( 1, 7, 4 ), sirDemand( 2, 7, 4 ), sirDemand( 3, 8, 4 ) };
	input.pPrevious = szOld;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_DEMAND_CHECK( nCount == 6 );
	for ( uint32_t i = 0; i < 3; ++i ) {
		SIR_DEMAND_CHECK( szOperations[i * 2].eAction == SIR_WORLD_HLOD_DEMAND_CANCEL );
		SIR_DEMAND_CHECK( szOperations[i * 2 + 1].eAction == SIR_WORLD_HLOD_DEMAND_REQUEST );
		SIR_DEMAND_CHECK( szOperations[i * 2 + 1].demand.nWorldGeneration == 5 );
	}

	// Insufficient capacity must not modify output or count.
	nCount = 77;
	szOperations[0].demand.nNodeIndex = 123;
	output.nCapacity = 5;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_CAPACITY );
	SIR_DEMAND_CHECK( nCount == 77 && szOperations[0].demand.nNodeIndex == 123 );
	output.nCapacity = 6;

	// Invalid source revisions, mixed epochs, and nonascending node identities fail transactionally.
	szNext[1].nSourceRevision = 0;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	szNext[1].nSourceRevision = 7;
	szNext[1].nWorldGeneration = 4;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	szNext[1].nWorldGeneration = 5;
	szNext[1].nNodeIndex = 1;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	szNext[1].nNodeIndex = 2;
	SIR_DEMAND_CHECK( nCount == 77 && szOperations[0].demand.nNodeIndex == 123 );

	// Empty current batch cancels all pending requests, never the loaded resources.
	input.pCurrent = nullptr;
	input.nCurrentCount = 0;
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_DEMAND_CHECK( nCount == 3 && szOperations[0].eAction == SIR_WORLD_HLOD_DEMAND_CANCEL );

	// Integrate with the actual HLOD selector's emitted missing-child demand.
	sirWorldHLODNode_t szNodes[] = { { { 11, 3, 0 }, 7, 1, 2 }, { { 12, 3, 0 }, 7, 0, 0 }, { { 13, 3, 0 }, 7, 0, 0 } };
	sirWorldHLODResidency_t szResidency[] = { { 7, 6, true }, { 7, 6, true }, { 7, 6, false } };
	float szErrors[] = { 100.0f, 0.0f, 0.0f };
	sirWorldHLODInput_t cutInput = { szNodes, szResidency, szErrors, 3, 6, 10.0f };
	sirWorldHLODScratch_t szScratch[3] = {};
	uint32_t szSelected[3] = {};
	sirWorldHLODDemand_t szDemands[3] = {};
	uint32_t nSelected = 0;
	uint32_t nDemands = 0;
	sirWorldHLODOutput_t cut = { szScratch, 3, szSelected, 3, &nSelected, szDemands, 3, &nDemands };
	SIR_DEMAND_CHECK( sirWorldHLODPlan( cutInput, cut ) == SIR_WORLD_HLOD_OK );
	SIR_DEMAND_CHECK( nSelected == 1 && szSelected[0] == 0 && nDemands == 1 && szDemands[0].nNodeIndex == 2 );
	input = { nullptr, 0, szDemands, nDemands };
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_DEMAND_CHECK( nCount == 1 && szOperations[0].eAction == SIR_WORLD_HLOD_DEMAND_REQUEST );
	sirWorldHLODDemand_t priorMissing = szDemands[0];
	szResidency[2].bReady = true;
	SIR_DEMAND_CHECK( sirWorldHLODPlan( cutInput, cut ) == SIR_WORLD_HLOD_OK && nDemands == 0 );
	input = { &priorMissing, 1, nullptr, 0 };
	SIR_DEMAND_CHECK( sirWorldHLODReconcileDemands( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_DEMAND_CHECK( nCount == 1 && szOperations[0].eAction == SIR_WORLD_HLOD_DEMAND_CANCEL );
	return 0;
}
