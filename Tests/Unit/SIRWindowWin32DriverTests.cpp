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
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
#include <Core/Platform/Windows/Window/SIRWindowDriverWin32.h>
#include <Core/Platform/Windows/Input/SIRWin32InputBridge.h>
#include <Core/Platform/Windows/Input/SIRWindowsRawBackend.h>
#include <assert.h>
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

static sirWindowWin32Dispatch_t sirConsumeNativeClose( const sirWindowWin32Message_t& message, void *pContext ) noexcept
{
	( void )pContext;
	return { 0, message.nMessage == WM_CLOSE };
}

typedef struct sirWin32NativePumpCount_s
{
	uint32_t nDelivered;
	uint32_t nClose;
} sirWin32NativePumpCount_t;

static void sirCountNativeWin32( void *pContext, const sirWindowBackendEvent_t& event ) noexcept
{
	sirWin32NativePumpCount_t *pCount = ( sirWin32NativePumpCount_t * )pContext;
	++pCount->nDelivered;
	if ( event.eKind == sirWindowEventKind::SIR_WINDOW_EVENT_CLOSE_REQUESTED ) {
		++pCount->nClose;
	}
}

static bool sirTestWin32BoundedNativeEvents( void ) noexcept
{
	SIRWindowDriverWin32 driver;
	if ( driver.initialize() != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowNativeView_t view = {};
	if ( driver.createWindow( { "pump-budget", 640, 480 }, 0, &view ) != sirWindowResult::SIR_WINDOW_OK ) {
		driver.shutdown();
		return false;
	}
	for ( uint32_t i = 0; i < 6; ++i ) {
		( void )SendMessageW( ( HWND )view.nNativeWindow, WM_SETFOCUS, 0, 0 );
		( void )SendMessageW( ( HWND )view.nNativeWindow, WM_KILLFOCUS, 0, 0 );
	}
	( void )SendMessageW( ( HWND )view.nNativeWindow, WM_CLOSE, 0, 0 );
	sirWin32NativePumpCount_t count = {};
	const bool bFirst = driver.pumpEvents( 1, sirCountNativeWin32, &count ) == sirWindowResult::SIR_WINDOW_OK && count.nDelivered == 1 && count.nClose == 1;
	for ( uint32_t i = 0; i < 32; ++i ) {
		( void )driver.pumpEvents( 2, sirCountNativeWin32, &count );
	}
	const bool bClosed = count.nClose == 1;
	const bool bDestroyed = driver.destroyWindow( 0 ) == sirWindowResult::SIR_WINDOW_OK;
	driver.shutdown();
	return bFirst && bClosed && bDestroyed;
}

static sirWindowWin32Dispatch_t sirRouteWin32Input( const sirWindowWin32Message_t& message, void *pContext ) noexcept
{
	SIRWin32InputBridge *pBridge = ( SIRWin32InputBridge * )pContext;
	sirWindowWin32Dispatch_t result = {};
	result.nResult = pBridge->processWindowMessage( message, &result.bHandled );
	return result;
}

static bool sirTestWin32MultiwindowInputRouting( void ) noexcept
{
	SIRWindowDriverWin32 driver;
	SIRWindowSystem windows( driver );
	SIRInputService input;
	SIRWindowsRawBackend native;
	SIRWin32InputBridge bridge;
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t first = {}, second = {};
	if ( windows.createWindow( { "first-input", 640, 480 }, &first ) != sirWindowResult::SIR_WINDOW_OK ||
	     windows.createWindow( { "second-input", 640, 480 }, &second ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	SIRNativeWindowLease firstLease, secondLease;
	sirWindowNativeView_t firstView = {}, secondView = {};
	if ( sirAcquireTestLease( windows, first, firstLease, firstView ) != sirWindowResult::SIR_WINDOW_OK ||
	     sirAcquireTestLease( windows, second, secondLease, secondView ) != sirWindowResult::SIR_WINDOW_OK || !bridge.bind( input, native, firstView ) ||
	     !bridge.registerWindow( secondView ) || bridge.registerWindow( secondView ) || !bridge.acceptsWindow( firstView.nNativeWindow ) ||
	     !bridge.acceptsWindow( secondView.nNativeWindow ) || !driver.setMessageSink( sirRouteWin32Input, &bridge ) ) {
		return false;
	}
	if ( !input.begin( 1 ) ) {
		return false;
	}
	( void )SendMessageW( ( HWND )secondView.nNativeWindow, WM_SETFOCUS, 0, 0 );
	bridge.finishPump();
	if ( !input.publish() || !bridge.unregisterWindow( secondView.nNativeWindow ) || bridge.acceptsWindow( secondView.nNativeWindow ) ||
	     !bridge.acceptsWindow( firstView.nNativeWindow ) || bridge.unregisterWindow( secondView.nNativeWindow ) ) {
		return false;
	}
	( void )driver.setMessageSink( nullptr, nullptr );
	bridge.unbind();
	if ( secondLease.reset() != sirWindowResult::SIR_WINDOW_OK || firstLease.reset() != sirWindowResult::SIR_WINDOW_OK ||
	     windows.destroyWindow( second ) != sirWindowResult::SIR_WINDOW_OK || windows.destroyWindow( first ) != sirWindowResult::SIR_WINDOW_OK ||
	     windows.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	input.shutdown();
	return true;
}

// A consumer that marks ordinary focus/size messages handled must not
// prevent the platform owner from recording native state transitions.
static sirWindowWin32Dispatch_t sirConsumeNativeState( const sirWindowWin32Message_t& message, void *pContext ) noexcept
{
	( void )pContext;
	const bool bHandled = message.nMessage == WM_SIZE || message.nMessage == WM_SETFOCUS || message.nMessage == WM_KILLFOCUS;
	return { 0, bHandled };
}

static sirWindowWin32Dispatch_t sirTryToConsumeForegroundRawInput( const sirWindowWin32Message_t& message, void *pContext ) noexcept
{
	( void )pContext;
	return { 0x7342, message.nMessage == WM_INPUT };
}

static bool sirTestWindowStateCannotBeSwallowedByInput( void ) noexcept
{
	SIRWindowDriverWin32 driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	if ( windows.createWindow( { "native-state-owner", 640, 480 }, &hWindow ) != sirWindowResult::SIR_WINDOW_OK ||
	     !driver.setMessageSink( sirConsumeNativeState, &windows ) ) {
		return false;
	}
	SIRNativeWindowLease lease;
	sirWindowNativeView_t view = {};
	if ( sirAcquireTestLease( windows, hWindow, lease, view ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	const HWND hNative = ( HWND )view.nNativeWindow;
	( void )SendMessageW( hNative, WM_SIZE, SIZE_RESTORED, MAKELPARAM( 853, 491 ) );
	( void )SendMessageW( hNative, WM_KILLFOCUS, 0, 0 );
	( void )SendMessageW( hNative, WM_SETFOCUS, 0, 0 );
	sirWindowSnapshot_t snapshot = {};
	if ( windows.pumpEvents( 256 ) != sirWindowResult::SIR_WINDOW_OK || windows.querySnapshot( hWindow, &snapshot ) != sirWindowResult::SIR_WINDOW_OK ||
	     snapshot.nWidth != 853 || snapshot.nHeight != 491 || !snapshot.bFocused ) {
		return false;
	}
	assert( snapshot.nWidth == 853 );
	// Raw foreground input uses DefWindowProcW to release system-owned
	// HRAWINPUT state even when the consumer attempts to swallow it.
	if ( !driver.setMessageSink( sirTryToConsumeForegroundRawInput, &windows ) ||
	     SendMessageW( hNative, WM_INPUT, RIM_INPUT, 0 ) == ( LRESULT )0x7342 ) {
		return false;
	}
	( void )driver.setMessageSink( nullptr, nullptr );
	return lease.reset() == sirWindowResult::SIR_WINDOW_OK && windows.destroyWindow( hWindow ) == sirWindowResult::SIR_WINDOW_OK &&
	       windows.shutdown() == SIR_SYSTEM_OK;
}

static bool sirTestFullscreenRoundTrip( void ) noexcept
{
	SIRWindowDriverWin32 driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t handle = {};
	if ( windows.createWindow( { "fullscreen-roundtrip", 640, 480 }, &handle ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	SIRNativeWindowLease lease;
	sirWindowNativeView_t view = {};
	if ( sirAcquireTestLease( windows, handle, lease, view ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	const HWND hWindow = ( HWND )view.nNativeWindow;
	WINDOWPLACEMENT original = {};
	original.length = sizeof( original );
	const DWORD nOriginalStyle = ( DWORD )GetWindowLongPtrW( hWindow, GWL_STYLE );
	if ( !GetWindowPlacement( hWindow, &original ) ||
	     windows.mutateWindow( handle, { sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_ENTER, nullptr, 0, 0 } ) != sirWindowResult::SIR_WINDOW_OK ||
	     ( ( DWORD )GetWindowLongPtrW( hWindow, GWL_STYLE ) & WS_OVERLAPPEDWINDOW ) != 0 ||
	     windows.mutateWindow( handle, { sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_ENTER, nullptr, 0, 0 } ) != sirWindowResult::SIR_WINDOW_OK ||
	     windows.mutateWindow( handle, { sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_EXIT, nullptr, 0, 0 } ) != sirWindowResult::SIR_WINDOW_OK ||
	     ( DWORD )GetWindowLongPtrW( hWindow, GWL_STYLE ) != nOriginalStyle ||
	     windows.mutateWindow( handle, { sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_EXIT, nullptr, 0, 0 } ) != sirWindowResult::SIR_WINDOW_OK ||
	     lease.reset() != sirWindowResult::SIR_WINDOW_OK || windows.destroyWindow( handle ) != sirWindowResult::SIR_WINDOW_OK ||
	     windows.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

static bool sirTestWindowClassRetirementOrder( void ) noexcept
{
	SIRWindowDriverWin32 first;
	SIRWindowDriverWin32 second;
	if ( first.initialize() != sirWindowResult::SIR_WINDOW_OK || second.initialize() != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	first.shutdown();
	sirWindowNativeView_t view = {};
	if ( second.createWindow( { "remaining-owner", 320, 240 }, 0, &view ) != sirWindowResult::SIR_WINDOW_OK || view.nNativeWindow == 0 ||
	     second.destroyWindow( 0 ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	second.shutdown();
	return true;
}

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
	if ( first.createWindow( description, &firstWindow ) != sirWindowResult::SIR_WINDOW_OK ||
	     second.createWindow( description, &secondWindow ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	SIRNativeWindowLease lease;
	sirWindowNativeView_t view = {};
	if ( sirAcquireTestLease( second, secondWindow, lease, view ) != sirWindowResult::SIR_WINDOW_OK || view.ePlatform != sirWindowPlatform::SIR_WINDOW_PLATFORM_WIN32 ||
	     view.nNativeWindow == 0 || !secondDriver.setMessageSink( sirConsumeNativeClose, &second ) ) {
		return false;
	}
	// Even a subscriber that claims WM_CLOSE cannot consume the native close.
	( void )SendMessageW( ( HWND )view.nNativeWindow, WM_CLOSE, 0, 0 );
	sirWindowSnapshot_t snapshot = {};
	SIRNativeWindowLease rejected;
	sirWindowNativeView_t rejectedView = {};
	if ( second.pumpEvents( 32 ) != sirWindowResult::SIR_WINDOW_OK || second.querySnapshot( secondWindow, &snapshot ) != sirWindowResult::SIR_WINDOW_OK ||
	     !snapshot.bCloseRequested || snapshot.bReady || second.destroyWindow( secondWindow ) != sirWindowResult::SIR_WINDOW_BUSY ||
	     sirAcquireTestLease( second, secondWindow, rejected, rejectedView ) != sirWindowResult::SIR_WINDOW_NOT_READY ||
	     lease.reset() != sirWindowResult::SIR_WINDOW_OK || second.destroyWindow( secondWindow ) != sirWindowResult::SIR_WINDOW_OK ||
	     second.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	SIRNativeWindowLease firstLease;
	sirWindowNativeView_t firstView = {};
	if ( sirAcquireTestLease( first, firstWindow, firstLease, firstView ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowSnapshot_t beforeTopology = {}, afterTopology = {};
	if ( first.querySnapshot( firstWindow, &beforeTopology ) != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	( void )SendMessageW( ( HWND )firstView.nNativeWindow, WM_DISPLAYCHANGE, 32, MAKELPARAM( 1920, 1080 ) );
	if ( first.pumpEvents( 32 ) != sirWindowResult::SIR_WINDOW_OK || first.querySnapshot( firstWindow, &afterTopology ) != sirWindowResult::SIR_WINDOW_OK ||
	     afterTopology.nMetricsRevision <= beforeTopology.nMetricsRevision || afterTopology.nWidth == 0 || afterTopology.nHeight == 0 ||
	     firstLease.reset() != sirWindowResult::SIR_WINDOW_OK ) {
		return false;
	}
	// Shutting down the second user must not unregister the first user's class.
	const sirWindowMutation_t title = { sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE, "still-registered", 0, 0 };
	if ( first.mutateWindow( firstWindow, title ) != sirWindowResult::SIR_WINDOW_OK || first.pumpEvents( 32 ) != sirWindowResult::SIR_WINDOW_OK ||
	     first.destroyWindow( firstWindow ) != sirWindowResult::SIR_WINDOW_OK || first.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestConcurrentWin32WindowClassOwners() || !sirTestWin32BoundedNativeEvents() || !sirTestWin32MultiwindowInputRouting() ||
	     !sirTestFullscreenRoundTrip() || !sirTestWindowClassRetirementOrder() || !sirTestWindowStateCannotBeSwallowedByInput() ) {
		fputs( "SIRWindowWin32DriverTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowWin32DriverTests PASS" );
	return 0;
}
