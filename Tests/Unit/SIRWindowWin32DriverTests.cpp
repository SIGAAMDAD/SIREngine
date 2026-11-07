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
	if ( event.eKind == SIR_WINDOW_EVENT_CLOSE_REQUESTED ) {
		++pCount->nClose;
	}
}

static bool sirTestWin32BoundedNativeEvents( void ) noexcept
{
	SIRWindowDriverWin32 driver;
	if ( driver.initialize() != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowNativeView_t view = {};
	if ( driver.createWindow( { "pump-budget", 640, 480 }, 0, &view ) != SIR_WINDOW_OK ) {
		driver.shutdown();
		return false;
	}
	for ( uint32_t i = 0; i < 6; ++i ) {
		( void )SendMessageW( ( HWND )view.nNativeWindow, WM_SETFOCUS, 0, 0 );
		( void )SendMessageW( ( HWND )view.nNativeWindow, WM_KILLFOCUS, 0, 0 );
	}
	( void )SendMessageW( ( HWND )view.nNativeWindow, WM_CLOSE, 0, 0 );
	sirWin32NativePumpCount_t count = {};
	const bool bFirst = driver.pumpEvents( 1, sirCountNativeWin32, &count ) == SIR_WINDOW_OK && count.nDelivered == 1 &&
	                    count.nClose == 1;
	for ( uint32_t i = 0; i < 32; ++i ) {
		( void )driver.pumpEvents( 2, sirCountNativeWin32, &count );
	}
	const bool bClosed = count.nClose == 1;
	const bool bDestroyed = driver.destroyWindow( 0 ) == SIR_WINDOW_OK;
	driver.shutdown();
	return bFirst && bClosed && bDestroyed;
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
	if ( first.createWindow( description, &firstWindow ) != SIR_WINDOW_OK ||
	     second.createWindow( description, &secondWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t view = {};
	if ( second.acquireNativeLease( secondWindow, &lease, &view ) != SIR_WINDOW_OK || view.ePlatform != SIR_WINDOW_PLATFORM_WIN32 ||
	     view.nNativeWindow == 0 || !secondDriver.setMessageSink( sirConsumeNativeClose, &second ) ) {
		return false;
	}
	// Even a subscriber that claims WM_CLOSE cannot consume the native close.
	( void )SendMessageW( ( HWND )view.nNativeWindow, WM_CLOSE, 0, 0 );
	sirWindowSnapshot_t snapshot = {};
	sirWindowNativeLease_t rejected = {};
	sirWindowNativeView_t rejectedView = {};
	if ( second.pumpEvents( 32 ) != SIR_WINDOW_OK || second.querySnapshot( secondWindow, &snapshot ) != SIR_WINDOW_OK ||
	     !snapshot.bCloseRequested || snapshot.bReady || second.destroyWindow( secondWindow ) != SIR_WINDOW_BUSY ||
	     second.acquireNativeLease( secondWindow, &rejected, &rejectedView ) != SIR_WINDOW_NOT_READY ||
	     second.releaseNativeLease( lease ) != SIR_WINDOW_OK || second.destroyWindow( secondWindow ) != SIR_WINDOW_OK ||
	     second.shutdown() != SIR_SYSTEM_OK ) {
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
	if ( !sirTestConcurrentWin32WindowClassOwners() || !sirTestWin32BoundedNativeEvents() ) {
		fputs( "SIRWindowWin32DriverTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowWin32DriverTests PASS" );
	return 0;
}
