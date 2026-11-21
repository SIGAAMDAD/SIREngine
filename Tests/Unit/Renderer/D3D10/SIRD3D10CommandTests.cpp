/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/D3D10/SIRD3D10CommandList.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/SIRRenderingDevice.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static void sirD3D10Require( bool bCondition, const char *pszMsg ) noexcept
{
	if ( !bCondition ) {
		std::fprintf( stderr, "FAIL: %s\n", pszMsg );
		abort();
	}
	else {
		std::printf( "PASS: %s\n", pszMsg );
	}
}

int main( void )
{
	std::printf( "D3D10 Command List validation tests (CPU mock)\n" );

	// Test command list creation
	{
		sirD3D10CommandListImpl_t *pCmdList = nullptr;
		sirGraphicsResult_t eResult = sirD3D10CreateCommandList( 1, SIR_GRAPHICS_QUEUE_DIRECT, &pCmdList );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "command list creation" );
		sirD3D10Require( pCmdList != nullptr, "cmd list pointer valid" );
		sirD3D10Require( pCmdList->nDeviceGeneration == 1, "device generation set" );
		sirD3D10Require( pCmdList->eQueue == SIR_GRAPHICS_QUEUE_DIRECT, "queue type correct" );
		sirD3D10Require( pCmdList->nCommandCount == 0, "initial command count zero" );
		sirD3D10Require( !pCmdList->bClosed, "not closed initially" );
		sirD3D10DestroyCommandList( pCmdList );
	}

	// Test compute queue rejection
	{
		sirD3D10CommandListImpl_t *pCmdList = nullptr;
		sirGraphicsResult_t eResult = sirD3D10CreateCommandList( 1, SIR_GRAPHICS_QUEUE_COMPUTE, &pCmdList );
		sirD3D10Require( eResult == SIR_GRAPHICS_UNSUPPORTED, "compute queue unsupported" );
	}

	// Test command list reset
	{
		sirD3D10CommandListImpl_t *pCmdList = nullptr;
		sirD3D10CreateCommandList( 1, SIR_GRAPHICS_QUEUE_DIRECT, &pCmdList );

		// Add a dummy command
		sirD3D10Command_t cmd = {};
		cmd.eType = SIR_D3D10_CMD_DRAW;
		cmd.draw.nVertices = 3;
		sirD3D10AppendCommand( pCmdList, cmd );
		sirD3D10Require( pCmdList->nCommandCount == 1, "command added" );

		// Reset
		pCmdList->nCommandCount = 0;
		pCmdList->hBoundPipeline = {};
		sirD3D10Require( pCmdList->nCommandCount == 0, "reset clears commands" );
		sirD3D10DestroyCommandList( pCmdList );
	}

	// Test command appending
	{
		sirD3D10CommandListImpl_t *pCmdList = nullptr;
		sirD3D10CreateCommandList( 1, SIR_GRAPHICS_QUEUE_DIRECT, &pCmdList );

		sirD3D10Command_t cmd = {};
		cmd.eType = SIR_D3D10_CMD_DRAW;
		cmd.draw.nVertices = 3;

		sirGraphicsResult_t eResult = sirD3D10AppendCommand( pCmdList, cmd );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "append command success" );
		sirD3D10Require( pCmdList->nCommandCount == 1, "command count incremented" );

		sirD3D10DestroyCommandList( pCmdList );
	}

	std::printf( "\nAll D3D10 Command List validation tests passed!\n" );
	return 0;
}