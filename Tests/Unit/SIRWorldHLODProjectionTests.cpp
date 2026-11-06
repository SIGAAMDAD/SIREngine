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

#include <World/HLOD/SIRWorldHLODProjection.h>

#include <math.h>
#include <stdio.h>

#define SIR_HLOD_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "HLOD projection failure at line %d: %s\n", __LINE__, #expression );                                          \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static bool sirHLODNear( float fA, float fB ) noexcept
{
	return fabsf( fA - fB ) < 0.05f;
}

int main( void )
{
	sirWorldHLODManifestBounds_t szBounds[3] = {
	    { { -2.0f, -2.0f, 10.0f }, { 2.0f, 2.0f, 12.0f } },
	    { { -2.0f, -2.0f, 10.0f }, { 0.0f, 2.0f, 12.0f } },
	    { { 0.0f, -2.0f, 10.0f }, { 2.0f, 2.0f, 12.0f } },
	};
	float szGeomErrors[3] = { 1.0f, 0.2f, 0.2f };
	sirWorldHLODProjectionInput_t projection = {};
	projection.pBounds = szBounds;
	projection.pGeometricErrors = szGeomErrors;
	projection.nNodeCount = 3;
	projection.view.fCameraPosition[2] = 0.0;
	projection.view.fCameraForward[2] = 1.0;
	projection.view.fVerticalParameter = 1.5707963267948966;
	projection.view.fNearPlane = 1.0;
	projection.view.nViewportHeightPixels = 1000;
	projection.view.eMode = SIR_WORLD_HLOD_PROJECTION_PERSPECTIVE;
	float szPixelErrors[3] = { -1.0f, -1.0f, -1.0f };
	sirWorldHLODProjectionOutput_t pixelOutput = { szPixelErrors, 3 };

	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 50.0f ) );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[1], 10.0f ) && sirHLODNear( szPixelErrors[2], 10.0f ) );
	projection.view.fCameraForward[2] = 10.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 50.0f ) );
	projection.view.fCameraForward[2] = 1.0;

	// Moving toward the object increases projected error; moving away decreases it.
	projection.view.fCameraPosition[2] = -15.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 20.0f ) );
	projection.view.fCameraPosition[2] = -90.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 5.0f ) );

	projection.view.eMode = SIR_WORLD_HLOD_PROJECTION_ORTHOGRAPHIC;
	projection.view.fVerticalParameter = 10.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 100.0f ) );
	projection.view.fCameraPosition[2] = 0.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 100.0f ) );

	// Error is conservative inside the bounding box and for off-frustum boxes.
	projection.view.eMode = SIR_WORLD_HLOD_PROJECTION_PERSPECTIVE;
	projection.view.fVerticalParameter = 1.5707963267948966;
	projection.view.fCameraPosition[2] = 10.5;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 500.0f ) );

	// Precision survives a large origin offset represented in double.
	projection.view.fCameraPosition[2] = 0.0;
	projection.view.fCameraPosition[0] = 100000000.0;
	for ( uint32_t iNode = 0; iNode < 3; ++iNode ) {
		szBounds[iNode].fMinimum[0] += 100000000.0f;
		szBounds[iNode].fMaximum[0] += 100000000.0f;
	}
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( sirHLODNear( szPixelErrors[0], 50.0f ) );
	projection.view.fCameraPosition[0] = 0.0;

	// A malformed record must not overwrite preceding valid projected errors.
	szPixelErrors[0] = 1234.0f;
	szPixelErrors[1] = 5678.0f;
	szGeomErrors[2] = NAN;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	SIR_HLOD_CHECK( szPixelErrors[0] == 1234.0f && szPixelErrors[1] == 5678.0f );
	szGeomErrors[2] = 0.2f;
	szBounds[2].fMaximum[1] = -3.0f;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	szBounds[2].fMaximum[1] = 2.0f;
	pixelOutput.nCapacity = 2;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	pixelOutput.nCapacity = 3;
	projection.view.fCameraForward[2] = 0.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	projection.view.fCameraForward[2] = 1.0;
	projection.view.fVerticalParameter = 3.14159265358979323846;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	projection.view.fVerticalParameter = 1.5707963267948966;

	// Feed the actual projected errors into the planner and exercise split hysteresis.
	for ( uint32_t iNode = 0; iNode < 3; ++iNode ) {
		szBounds[iNode].fMinimum[0] -= 100000000.0f;
		szBounds[iNode].fMaximum[0] -= 100000000.0f;
	}
	projection.view.fCameraPosition[2] = -15.0;
	SIR_HLOD_CHECK( sirWorldHLODProjectErrors( projection, pixelOutput ) == SIR_WORLD_HLOD_OK );
	sirWorldHLODNode_t szNodes[] = { { { 11, 3, 0 }, 7, 1, 2 }, { { 12, 3, 0 }, 7, 0, 0 }, { { 13, 3, 0 }, 7, 0, 0 } };
	sirWorldHLODResidency_t szResidency[] = { { 7, 11, true }, { 7, 11, true }, { 7, 11, true } };
	sirWorldHLODScratch_t szScratch[3] = {};
	uint32_t szSelected[3] = {};
	sirWorldHLODDemand_t szDemands[3] = {};
	uint32_t nSelected = 0;
	uint32_t nDemands = 0;
	sirWorldHLODInput_t cutInput = { szNodes, szResidency, szPixelErrors, 3, 11, 30.0f };
	sirWorldHLODOutput_t cutOutput = { szScratch, 3, szSelected, 3, &nSelected, szDemands, 3, &nDemands };

	SIR_HLOD_CHECK( sirWorldHLODPlan( cutInput, cutOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 );
	uint8_t szPreviouslySplit[] = { 1, 0, 0 };
	sirWorldHLODHistory_t history = { szPreviouslySplit, 11, 15.0f };
	SIR_HLOD_CHECK( sirWorldHLODPlanHysteretic( cutInput, history, cutOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 2 && szSelected[0] == 1 && szSelected[1] == 2 );
	SIR_HLOD_CHECK( szScratch[0].bActive && szScratch[0].bUseChildren );

	// A lost child switches to resident parent and requests the missing detail
	// even while the error sits between the refine and coarsen thresholds.
	szResidency[2].bReady = false;
	SIR_HLOD_CHECK( sirWorldHLODPlanHysteretic( cutInput, history, cutOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 && nDemands == 1 && szDemands[0].nNodeIndex == 2 );

	// Previous-world split history must be ignored after a generation change.
	history.nWorldGeneration = 10;
	SIR_HLOD_CHECK( sirWorldHLODPlanHysteretic( cutInput, history, cutOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 && nDemands == 0 );
	history.nWorldGeneration = 11;
	history.fCoarsenProjectedError = 31.0f;
	SIR_HLOD_CHECK( sirWorldHLODPlanHysteretic( cutInput, history, cutOutput ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	history.fCoarsenProjectedError = NAN;
	SIR_HLOD_CHECK( sirWorldHLODPlanHysteretic( cutInput, history, cutOutput ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	return 0;
}
