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

#include <Core/Platform/Shared/Window/SIRNativeWindowLease.h>
#include <Core/Input/SIRInputService.h>
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
#include <Core/Platform/Linux/Window/SIRWindowDriverX11.h>
#include <Core/Platform/Linux/Input/SIRX11InputBridge.h>
#include <Core/Platform/Linux/Input/SIRLinuxX11Backend.h>

#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>


static sirWindowResult sirAcquireTestLease( SIRWindowSystem& system, sirWindowHandle_t handle, SIRNativeWindowLease& lease, sirWindowNativeView_t& view ) noexcept
{
    if ( lease.isValid() ) {
        SIRNativeWindowLease probe;
        return system.acquireNativeLease( handle, probe );
    }
    const sirWindowResult result = system.acquireNativeLease( handle, lease );
    if ( result == sirWindowResult::SIR_WINDOW_OK ) { view = lease.getView(); }
    return result;
}

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
	SIRWindowDriverX11 driver;
	SIRWindowSystem windows( driver );
	SIRX11InputBridge bridge;
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		fputs( "SIRInputX11IMEFallbackTests cannot initialize native driver\n", stderr );
		return 1;
	}
	const sirWindowCreateDesc_t desc = { "X11 IME fallback", 800, 450 };
	sirWindowHandle_t hWindow = {};
	if ( windows.createWindow( desc, &hWindow ) != sirWindowResult::SIR_WINDOW_OK ) {
		return 1;
	}
	SIRNativeWindowLease lease;
	sirWindowNativeView_t view = {};
	if ( sirAcquireTestLease( windows, hWindow, lease, view ) != sirWindowResult::SIR_WINDOW_OK || !bridge.bind( input, backend, view ) ||
	     !s_bRejectedNativeIME ) {
		fputs( "SIRInputX11IMEFallbackTests did not exercise unavailable XIM context\n", stderr );
		return 1;
	}
	if ( !input.begin( 1 ) || !bridge.beginFrame() || windows.pumpEvents( 128 ) != sirWindowResult::SIR_WINDOW_OK || input.activeDeviceCount() != 2 ) {
		fputs( "SIRInputX11IMEFallbackTests failed native input without XIM context\n", stderr );
		return 1;
	}
	bridge.setTextEnabled( true );
	if ( !input.text().snapshot().bEnabled ) {
		fputs( "SIRInputX11IMEFallbackTests failed text-mode fallback\n", stderr );
		return 1;
	}
	bridge.unbind();
	if ( lease.reset() != sirWindowResult::SIR_WINDOW_OK || windows.destroyWindow( hWindow ) != sirWindowResult::SIR_WINDOW_OK ||
	     windows.shutdown() != SIR_SYSTEM_OK ) {
		return 1;
	}
	input.shutdown();
	puts( "SIRInputX11IMEFallbackTests PASS" );
	return 0;
}
