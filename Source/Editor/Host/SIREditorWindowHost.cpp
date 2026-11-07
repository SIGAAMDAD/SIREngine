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

#include <Editor/Host/SIREditorWindowHost.h>
#include <Core/SIRApplication.h>
#include <Core/Platform/Window/SIRWindowSystem.h>

static bool sirEditorPumpWindows( void *pContext )
{
	SIRWindowSystem *pWindows = ( SIRWindowSystem * )pContext;
	return pWindows->pumpEvents( 256 ) == SIR_WINDOW_OK;
}

int sirEditorRunWindowHost( SIRWindowDriver& driver ) noexcept
{
	SIRWindowSystem windows( driver );
	SIRApplication app;
	if ( !app.registerSystem( &windows ) || !app.attachWindowPump( sirEditorPumpWindows, &windows ) || !app.initialize() ) {
		( void )app.shutdown();
		return 1;
	}

	sirWindowHandle_t hWindow = {};
	const sirWindowCreateDesc_t desc = { "SIREngine Editor", 1280, 720 };
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
