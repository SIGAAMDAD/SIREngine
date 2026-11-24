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
#include <Core/Platform/Linux/Window/SIRWindowDriverX11.h>
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
#include <Core/Platform/Linux/Input/SIRX11InputBridge.h>
#include <X11/Xlib.h>
#include <stdio.h>


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

static void sirBridgeDispatchEvent( const XEvent& event, void *pContext ) noexcept
{
	( ( SIRX11InputBridge * )pContext )->processEvent( event );
}

int main( void )
{
	SIRInputService input;
	SIRLinuxX11Backend backend;
	SIRX11InputBridge bridge;
	SIRWindowDriverX11 driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return 1;
	}
	const sirWindowCreateDesc_t desc = { "Input bridge", 600, 400 };
	sirWindowHandle_t first = {}, second = {};
	if ( windows.createWindow( desc, &first ) != sirWindowResult::SIR_WINDOW_OK || windows.createWindow( desc, &second ) != sirWindowResult::SIR_WINDOW_OK ) {
		return 2;
	}
	SIRNativeWindowLease lease, secondLease;
	sirWindowNativeView_t view = {}, secondView = {};
	if ( sirAcquireTestLease( windows, first, lease, view ) != sirWindowResult::SIR_WINDOW_OK ||
	     sirAcquireTestLease( windows, second, secondLease, secondView ) != sirWindowResult::SIR_WINDOW_OK || !bridge.bind( input, backend, view ) ||
	     bridge.bind( input, backend, view ) || !bridge.registerWindow( secondView ) || bridge.registerWindow( secondView ) ||
	     !driver.setEventSink( sirBridgeDispatchEvent, &bridge ) ) {
		return 3;
	}
	sirWindowNativeView_t foreign = secondView;
	++foreign.nDisplayOrInstance;
	if ( bridge.registerWindow( foreign ) ) {
		return 12;
	}
	foreign = secondView;
	foreign.ePlatform = sirWindowPlatform::SIR_WINDOW_PLATFORM_WIN32;
	if ( bridge.registerWindow( foreign ) ) {
		return 13;
	}
	bridge.setTextEnabled( true );
	if ( !input.text().snapshot().bEnabled || !input.begin( 1 ) || !bridge.beginFrame() ) {
		return 4;
	}
	Display *pDisplay = ( Display * )view.nDisplayOrInstance;
	XEvent focus = {};
	focus.xfocus.type = FocusIn;
	focus.xfocus.window = ( Window )view.nNativeWindow;
	focus.xfocus.mode = NotifyNormal;
	focus.xfocus.detail = NotifyNonlinear;
	XSendEvent( pDisplay, ( Window )view.nNativeWindow, False, FocusChangeMask, &focus );
	XSync( pDisplay, False );
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( windows.pumpEvents( 128 ) != sirWindowResult::SIR_WINDOW_OK ) {
			return 5;
		}
	}
	if ( input.activeDeviceCount() != 2 ) {
		return 6;
	}
	// Route the one physical keyboard and pointer seat between two windows.
	XEvent key = {};
	key.xkey.type = KeyPress;
	key.xkey.window = ( Window )view.nNativeWindow;
	key.xkey.display = pDisplay;
	key.xkey.keycode = 40;
	bridge.processEvent( key );
	XEvent secondFocus = focus;
	secondFocus.xfocus.window = ( Window )secondView.nNativeWindow;
	bridge.processEvent( secondFocus );
	if ( input.activeDeviceCount() != 2 ) {
		return 9;
	}
	key.xkey.window = ( Window )secondView.nNativeWindow;
	key.xkey.keycode = 41;
	bridge.processEvent( key );
	// Remove the *focused* second window, then verify the backend retargets
	// the surviving first window without reconnecting physical devices.
	if ( !bridge.unregisterWindow( secondView.nNativeWindow ) || bridge.unregisterWindow( secondView.nNativeWindow ) ||
	     input.activeDeviceCount() != 2 ) {
		return 10;
	}
	bridge.processEvent( focus );
	key.xkey.window = ( Window )view.nNativeWindow;
	key.xkey.keycode = 43;
	bridge.processEvent( key );
	input.sample();
	if ( !input.publish() ) {
		return 7;
	}
	const sirInputFrame_t *pFrame = input.frame();
	bool bSawSecondKey = false;
	bool bSawSurvivingKey = false;
	uint32_t nResyncEvents = 0;
	for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
		if ( pFrame->events[i].eSignal == SIR_INPUT_RESYNC ) {
			++nResyncEvents;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_BUTTON && pFrame->events[i].nControl == 41 && pFrame->events[i].nValue == 1 ) {
			bSawSecondKey = true;
		}
		if ( pFrame->events[i].eSignal == SIR_INPUT_BUTTON && pFrame->events[i].nControl == 43 && pFrame->events[i].nValue == 1 ) {
			bSawSurvivingKey = true;
		}
	}
	if ( !bSawSecondKey || !bSawSurvivingKey || nResyncEvents < 4 ) {
		return 11;
	}
	( void )driver.setEventSink( nullptr, nullptr );
	bridge.unbind();
	if ( bridge.isBound() || lease.reset() != sirWindowResult::SIR_WINDOW_OK ||
	     secondLease.reset() != sirWindowResult::SIR_WINDOW_OK || windows.destroyWindow( second ) != sirWindowResult::SIR_WINDOW_OK ||
	     windows.destroyWindow( first ) != sirWindowResult::SIR_WINDOW_OK || windows.shutdown() != SIR_SYSTEM_OK ) {
		return 8;
	}
	input.shutdown();
	puts( "SIRX11InputBridgeTests PASS" );
	return 0;
}
