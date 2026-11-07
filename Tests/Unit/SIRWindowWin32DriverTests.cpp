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

#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>
#include <stdio.h>

static bool sirTestConcurrentWin32WindowClassOwners( void ) noexcept
{
	SIRWindowDriverWin32 firstDriver;
	SIRWindowDriverWin32 secondDriver;
	SIRWindowSystem first( firstDriver );
	SIRWindowSystem second( secondDriver );
	if ( first.initialize() != SIR_SYSTEM_OK || second.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	const sirWindowCreateDesc_t description = { "native-window-registration", 640, 480 };
	sirWindowHandle_t firstWindow = {};
	sirWindowHandle_t secondWindow = {};
	if ( first.createWindow( description, &firstWindow ) != SIR_WINDOW_OK ||
	     second.createWindow( description, &secondWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t view = {};
	if ( second.acquireNativeLease( secondWindow, &lease, &view ) != SIR_WINDOW_OK || view.ePlatform != SIR_WINDOW_PLATFORM_WIN32 ||
	     view.nNativeWindow == 0 || second.releaseNativeLease( lease ) != SIR_WINDOW_OK ||
	     second.destroyWindow( secondWindow ) != SIR_WINDOW_OK || second.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	// Shutting down the second user must not unregister the first user's class.
	const sirWindowMutation_t title = { SIR_WINDOW_MUTATION_TITLE, "still-registered", 0, 0 };
	if ( first.mutateWindow( firstWindow, title ) != SIR_WINDOW_OK || first.pumpEvents( 32 ) != SIR_WINDOW_OK ||
	     first.destroyWindow( firstWindow ) != SIR_WINDOW_OK || first.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestConcurrentWin32WindowClassOwners() ) {
		fputs( "SIRWindowWin32DriverTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowWin32DriverTests PASS" );
	return 0;
}
