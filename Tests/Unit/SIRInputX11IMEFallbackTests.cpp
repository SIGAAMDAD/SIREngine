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

#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/SIRInputWindow.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>

#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>

static bool s_bRejectedNativeIME = false;

// Inject an actual XCreateIC failure at the native window's callsite.
// This test binary never forwards XCreateIC; the ordinary X11 acceptance
// executable independently tests the successful XIM path.
extern "C" XIC __wrap_XCreateIC( XIM pInputMethod, ... )
{
	( void )pInputMethod;
	s_bRejectedNativeIME = true;
	return nullptr;
}

int main( void )
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRInputWindow window;
	bool bOpened = false;
	for ( uint32_t iAttempt = 0; iAttempt < 16 && !bOpened; ++iAttempt ) {
		bOpened = window.open( input, backend );
		if ( !bOpened ) {
			( void )usleep( 1000 );
		}
	}
	if ( !bOpened || !window.isOpen() || !s_bRejectedNativeIME ) {
		fputs( "SIRInputX11IMEFallbackTests failed to exercise unavailable XIM context\n", stderr );
		return 1;
	}
	if ( !input.begin( 1 ) || !window.pump( input ) || input.activeDeviceCount() != 2 ) {
		fputs( "SIRInputX11IMEFallbackTests failed native input without XIM context\n", stderr );
		return 1;
	}
	window.setTextEnabled( true );
	if ( !input.text().snapshot().bEnabled ) {
		fputs( "SIRInputX11IMEFallbackTests failed text-mode fallback\n", stderr );
		return 1;
	}
	window.close();
	input.shutdown();
	puts( "SIRInputX11IMEFallbackTests PASS" );
	return 0;
}
