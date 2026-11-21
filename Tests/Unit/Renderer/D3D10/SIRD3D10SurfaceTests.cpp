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

#include <Renderer/Drivers/D3D10/SIRD3D10Surfaces.h>
#include <Renderer/SIRGraphicsTypes.h>
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
	std::printf( "D3D10 Surface validation tests (CPU mock)\n" );

	// Test surface handle validation
	{
		sirGraphicsHandle_t hSurface = {};
		hSurface.nSlot = 0;
		hSurface.nGeneration = 1;
		hSurface.nDeviceGeneration = 1;
		hSurface.eType = SIR_GRAPHICS_HANDLE_SURFACE;

		sirD3D10Surface_t *pSurface = sirD3D10GetSurface( 1, hSurface );
		sirD3D10Require( pSurface == nullptr, "unallocated surface returns null" );
	}

	// Test surface handle with wrong generation
	{
		sirGraphicsHandle_t hSurface = {};
		hSurface.nSlot = 0;
		hSurface.nGeneration = 1;
		hSurface.nDeviceGeneration = 2; // Mismatch
		hSurface.eType = SIR_GRAPHICS_HANDLE_SURFACE;

		sirD3D10Surface_t *pSurface = sirD3D10GetSurface( 1, hSurface );
		sirD3D10Require( pSurface == nullptr, "stale device generation rejected" );
	}

	// Test surface handle with wrong type
	{
		sirGraphicsHandle_t hSurface = {};
		hSurface.nSlot = 0;
		hSurface.nGeneration = 1;
		hSurface.nDeviceGeneration = 1;
		hSurface.eType = SIR_GRAPHICS_HANDLE_BUFFER; // Wrong type

		sirD3D10Surface_t *pSurface = sirD3D10GetSurface( 1, hSurface );
		sirD3D10Require( pSurface == nullptr, "wrong handle type rejected" );
	}

	// Test sync interval mapping
	{
		sirD3D10Require( sirD3D10MapSyncInterval( SIR_GRAPHICS_PRESENT_FIFO ) == 1, "FIFO sync interval" );
		sirD3D10Require( sirD3D10MapSyncInterval( SIR_GRAPHICS_PRESENT_IMMEDIATE ) == 0, "IMMEDIATE sync interval" );
		sirD3D10Require( sirD3D10MapSyncInterval( SIR_GRAPHICS_PRESENT_MAILBOX ) == 1, "MAILBOX falls back to FIFO" );
	}

	std::printf( "\nAll D3D10 Surface validation tests passed!\n" );
	return 0;
}