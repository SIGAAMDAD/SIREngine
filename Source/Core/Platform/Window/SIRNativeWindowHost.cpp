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

#include <Core/Platform/Window/SIRNativeWindowHost.h>
#include <Core/SIRApplication.h>
#include <Core/Platform/Window/SIRWindowSystem.h>
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#endif
#if defined( _WIN32 )
#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>
#elif defined( SIR_WINDOW_X11_AVAILABLE )
#include <Core/Platform/Window/Linux/SIRWindowDriverX11.h>
#endif
#include <stdlib.h>
#include <stdio.h>

static bool sirNativeWindowHostPump( void *pContext )
{
	SIRWindowSystem *pWindows = ( SIRWindowSystem * )pContext;
	const sirWindowResult_t eResult = pWindows->pumpEvents( 256 );
	return eResult == SIR_WINDOW_OK || eResult == SIR_WINDOW_EVENTS_LOST;
}

int sirRunWindowHostsWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept
{
	if ( nWindows > SIR_WINDOW_MAX_WINDOWS || ( nWindows != 0 && pDescs == nullptr ) ) {
		return 1;
	}
	SIRWindowSystem windows( driver );
	SIRApplication app;
	if ( !app.registerSystem( &windows ) || !app.attachWindowPump( sirNativeWindowHostPump, &windows ) || !app.initialize() ) {
		( void )app.shutdown();
		return 1;
	}
	sirWindowHandle_t handles[SIR_WINDOW_MAX_WINDOWS] = {};
	uint32_t nCreated = 0;
	bool bSucceeded = true;
	for ( uint32_t i = 0; i < nWindows; ++i ) {
		if ( windows.createWindow( pDescs[i], &handles[i] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
			break;
		}
		++nCreated;
	}
	uint32_t nOpen = nCreated;
	while ( bSucceeded && nOpen != 0 ) {
		if ( !app.tick() ) {
			bSucceeded = false;
			break;
		}
		for ( uint32_t i = 0; i < nCreated; ++i ) {
			if ( handles[i].nGeneration == 0 ) {
				continue;
			}
			sirWindowSnapshot_t snapshot = {};
			if ( windows.querySnapshot( handles[i], &snapshot ) != SIR_WINDOW_OK ) {
				bSucceeded = false;
				break;
			}
			if ( snapshot.bCloseRequested ) {
				if ( windows.destroyWindow( handles[i] ) != SIR_WINDOW_OK ) {
					bSucceeded = false;
					break;
				}
				handles[i] = {};
				--nOpen;
			}
		}
	}
	while ( nCreated != 0 ) {
		--nCreated;
		if ( handles[nCreated].nGeneration != 0 && windows.destroyWindow( handles[nCreated] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
	}
	const sirSystemResult_t eShutdown = app.shutdown();
	return bSucceeded && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
}

int sirRunWindowHostWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t& desc ) noexcept
{
	return sirRunWindowHostsWithDriver( driver, &desc, 1 );
}

int sirRunNativeWindowHosts( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept
{
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	const char *pszWaylandDisplay = getenv( "WAYLAND_DISPLAY" );
	if ( pszWaylandDisplay != nullptr && pszWaylandDisplay[0] != 0 ) {
		SIRWindowDriverWayland driver;
		return sirRunWindowHostsWithDriver( driver, pDescs, nWindows );
	}
#endif
#if defined( _WIN32 )
	SIRWindowDriverWin32 driver;
	return sirRunWindowHostsWithDriver( driver, pDescs, nWindows );
#elif defined( SIR_WINDOW_X11_AVAILABLE )
	SIRWindowDriverX11 driver;
	return sirRunWindowHostsWithDriver( driver, pDescs, nWindows );
#elif defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	SIRWindowDriverWayland driver;
	return sirRunWindowHostsWithDriver( driver, pDescs, nWindows );
#else
	fputs( "SIREngine: no native window driver is available in this build\n", stderr );
	return 1;
#endif
}

int sirRunNativeWindowHost( const sirWindowCreateDesc_t& desc ) noexcept
{
	return sirRunNativeWindowHosts( &desc, 1 );
}
