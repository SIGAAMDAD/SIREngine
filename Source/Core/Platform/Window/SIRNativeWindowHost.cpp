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
	return pWindows->pumpEvents( 256 ) == SIR_WINDOW_OK;
}

int sirRunWindowHostWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t& desc ) noexcept
{
	SIRWindowSystem windows( driver );
	SIRApplication app;
	if ( !app.registerSystem( &windows ) || !app.attachWindowPump( sirNativeWindowHostPump, &windows ) || !app.initialize() ) {
		( void )app.shutdown();
		return 1;
	}

	sirWindowHandle_t hWindow = {};
	if ( windows.createWindow( desc, &hWindow ) != SIR_WINDOW_OK ) {
		( void )app.shutdown();
		return 1;
	}

	bool bSucceeded = true;
	while ( true ) {
		if ( !app.tick() ) {
			bSucceeded = false;
			break;
		}
		sirWindowSnapshot_t snapshot = {};
		if ( windows.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
			break;
		}
		if ( snapshot.bCloseRequested ) {
			break;
		}
	}

	const sirWindowResult_t eDestroy = windows.destroyWindow( hWindow );
	const sirSystemResult_t eShutdown = app.shutdown();
	return bSucceeded && eDestroy == SIR_WINDOW_OK && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
}

int sirRunNativeWindowHost( const sirWindowCreateDesc_t& desc ) noexcept
{
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	const char *pszWaylandDisplay = getenv( "WAYLAND_DISPLAY" );
	if ( pszWaylandDisplay != nullptr && pszWaylandDisplay[0] != 0 ) {
		SIRWindowDriverWayland driver;
		return sirRunWindowHostWithDriver( driver, desc );
	}
#endif
#if defined( _WIN32 )
	SIRWindowDriverWin32 driver;
	return sirRunWindowHostWithDriver( driver, desc );
#elif defined( SIR_WINDOW_X11_AVAILABLE )
	SIRWindowDriverX11 driver;
	return sirRunWindowHostWithDriver( driver, desc );
#elif defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	SIRWindowDriverWayland driver;
	return sirRunWindowHostWithDriver( driver, desc );
#else
	fputs( "SIREngine: no native window driver is available in this build\n", stderr );
	return 1;
#endif
}
