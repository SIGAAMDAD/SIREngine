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

#include <World/HLOD/SIRWorldHLOD.h>

#include <math.h>
#include <stdio.h>

#define SIR_HLOD_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "HLOD failure: %s at line %d\n", #expression, __LINE__ );                                                     \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

int main( void )
{
	sirWorldHLODNode_t szNodes[] = {
	    { { 1, 3, 0 }, 7, 1, 2 }, { { 2, 3, 0 }, 7, 3, 2 }, { { 3, 3, 0 }, 7, 0, 0 }, { { 4, 3, 0 }, 7, 0, 0 }, { { 5, 3, 0 }, 7, 0, 0 },
	};
	sirWorldHLODResidency_t szResidency[] = {
	    { 7, 3, true }, { 7, 3, true }, { 7, 3, true }, { 7, 3, true }, { 7, 3, true },
	};
	float szErrors[] = { 100.0f, 100.0f, 0.0f, 0.0f, 0.0f };
	sirWorldHLODInput_t input = { szNodes, szResidency, szErrors, 5, 3, 10.0f };
	sirWorldHLODScratch_t szScratch[5] = {};
	uint32_t szSelected[5] = { 99, 99, 99, 99, 99 };
	sirWorldHLODDemand_t szDemands[5] = {};
	uint32_t nSelected = 99;
	uint32_t nDemands = 99;
	sirWorldHLODOutput_t output = { szScratch, 5, szSelected, 5, &nSelected, szDemands, 5, &nDemands };

	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 3 && szSelected[0] == 2 && szSelected[1] == 3 && szSelected[2] == 4 );
	SIR_HLOD_CHECK( nDemands == 0 );

	// A missing grandchild preserves its resident parent and emits a versioned demand.
	szResidency[4].bReady = false;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 2 && szSelected[0] == 1 && szSelected[1] == 2 );
	SIR_HLOD_CHECK( nDemands == 1 && szDemands[0].nNodeIndex == 4 && szDemands[0].nSourceRevision == 7 );

	// A missing intermediate page forces a root fallback, requesting only that page.
	szResidency[1].bReady = false;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 );
	SIR_HLOD_CHECK( nDemands == 1 && szDemands[0].nNodeIndex == 1 && szDemands[0].nWorldGeneration == 3 );

	// A nonresident parent may be covered by resident grandchildren.
	szResidency[1].bReady = true;
	szResidency[4].bReady = true;
	szResidency[0].bReady = false;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 3 && szSelected[0] == 2 && szSelected[1] == 3 && szSelected[2] == 4 );

	// If no cut exists, no output data is committed.
	szResidency[1].bReady = false;
	szResidency[4].bReady = false;
	nSelected = 77;
	nDemands = 88;
	szSelected[0] = 99;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_NO_RESIDENT_CUT );
	SIR_HLOD_CHECK( nSelected == 77 && nDemands == 88 && szSelected[0] == 99 );

	szResidency[0].bReady = true;
	szResidency[1].bReady = true;
	szResidency[4].bReady = true;
	output.nSelectedCapacity = 2;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_CAPACITY );
	SIR_HLOD_CHECK( nSelected == 77 && nDemands == 88 && szSelected[0] == 99 );
	output.nSelectedCapacity = 5;

	// Request capacity must also commit atomically with the render cut.
	szResidency[4].bReady = false;
	output.nDemandCapacity = 0;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_CAPACITY );
	SIR_HLOD_CHECK( nSelected == 77 && nDemands == 88 && szSelected[0] == 99 );
	output.nDemandCapacity = 5;

	// Stale world/source revisions must not make a missing page appear ready.
	szResidency[4].nWorldGeneration = 2;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 2 && nDemands == 1 && szDemands[0].nNodeIndex == 4 );
	szResidency[4].nWorldGeneration = 3;
	szResidency[4].nSourceRevision = 6;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 2 && nDemands == 1 );
	szResidency[4].nSourceRevision = 7;
	szResidency[4].bReady = true;

	// Low projected error retains the coarse page, with no demand emission.
	input.fMaxProjectedError = 200.0f;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 && nDemands == 0 );
	input.fMaxProjectedError = 10.0f;

	// Structural rejection, including overlapping ranges and invalid geometry identity.
	szNodes[0].nFirstChild = 4;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	szNodes[0].nFirstChild = 1;
	szNodes[1].nFirstChild = 2;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	szNodes[1].nFirstChild = 3;
	szErrors[3] = NAN;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	szErrors[3] = 0.0f;
	szNodes[2].keyGeometry.m_nAssetId = 0;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	szNodes[2].keyGeometry.m_nAssetId = 3;
	output.nScratchCapacity = 4;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	output.nScratchCapacity = 5;
	input.nWorldGeneration = 0;
	SIR_HLOD_CHECK( sirWorldHLODPlan( input, output ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	return 0;
}
