#if !defined( SIR_INPUT_X11_AVAILABLE )
#error Native X11 tests require X11
#endif
#include <Core/Platform/Window/SIRNativeWindowDrivers.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#include <Renderer/SIRGraphicsWindowBridge.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <unistd.h>

static bool sirWaitForX11State( SIRWindowSystem& windows, sirWindowHandle_t hWindow, sirWindowState_t eWanted, uint32_t nWidth = 0,
                                uint32_t nHeight = 0 )
{
	for ( uint32_t iAttempt = 0; iAttempt < 64; ++iAttempt ) {
		const sirWindowResult_t ePump = windows.pump();
		if ( ePump != SIR_WINDOW_OK && ePump != SIR_WINDOW_BUSY ) {
			return false;
		}
		sirWindowSnapshot_t snapshot = {};
		if ( windows.snapshot( hWindow, &snapshot ) && snapshot.eState == eWanted && ( nWidth == 0 || snapshot.nWidth == nWidth ) &&
		     ( nHeight == 0 || snapshot.nHeight == nHeight ) ) {
			return true;
		}
		( void )usleep( 1000 );
	}
	return false;
}

static bool sirTestTwoNativeWindows( void )
{
	SIRX11WindowDriver driver;
	SIRWindowSystem windows;
	if ( windows.initialize( driver ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowHandle_t hA = {};
	sirWindowHandle_t hB = {};
	if ( windows.create( { "SIREngine X11 A", 320, 240, true }, &hA ) != SIR_WINDOW_OK ||
	     windows.create( { "SIREngine X11 B", 400, 300, true }, &hB ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirGraphicsWindowBorrow_t borrowA = {};
	sirGraphicsWindowBorrow_t borrowB = {};
	if ( !sirGraphicsAcquireWindowSurface( windows, hA, &borrowA ) || !sirGraphicsAcquireWindowSurface( windows, hB, &borrowB ) ||
	     borrowA.surface.pDisplayOrInstance != borrowB.surface.pDisplayOrInstance || borrowA.surface.pWindow == borrowB.surface.pWindow ) {
		return false;
	}
	Display *pDisplay = ( Display * )borrowA.surface.pDisplayOrInstance;
	const Window hNativeA = ( Window )( uintptr_t )borrowA.surface.pWindow;
	const Window hNativeB = ( Window )( uintptr_t )borrowB.surface.pWindow;
	XResizeWindow( pDisplay, hNativeB, 512, 384 );
	XSync( pDisplay, False );
	if ( !sirWaitForX11State( windows, hB, SIR_WINDOW_READY, 512, 384 ) ) {
		return false;
	}
	sirWindowSnapshot_t snapshot = {};
	if ( !windows.snapshot( hB, &snapshot ) || snapshot.nWidth != 512 || snapshot.nHeight != 384 ) {
		return false;
	}
	XEvent closeEvent = {};
	closeEvent.xclient.type = ClientMessage;
	closeEvent.xclient.display = pDisplay;
	closeEvent.xclient.window = hNativeA;
	closeEvent.xclient.message_type = XInternAtom( pDisplay, "WM_PROTOCOLS", False );
	closeEvent.xclient.format = 32;
	closeEvent.xclient.data.l[0] = ( long )XInternAtom( pDisplay, "WM_DELETE_WINDOW", False );
	XSendEvent( pDisplay, hNativeA, False, NoEventMask, &closeEvent );
	XSync( pDisplay, False );
	if ( !sirWaitForX11State( windows, hA, SIR_WINDOW_CLOSE_REQUESTED ) || windows.activeWindowCount() != 2 ||
	     !windows.snapshot( hA, &snapshot ) || snapshot.eState != SIR_WINDOW_CLOSE_REQUESTED ) {
		return false;
	}
	if ( !sirGraphicsReleaseWindowSurface( windows, &borrowA ) || windows.pump() != SIR_WINDOW_OK || windows.activeWindowCount() != 1 ) {
		return false;
	}
	// A native loss invalidates the logical view immediately, even with a lease.
	XDestroyWindow( pDisplay, hNativeB );
	XSync( pDisplay, False );
	if ( !sirWaitForX11State( windows, hB, SIR_WINDOW_LOST ) || !windows.snapshot( hB, &snapshot ) || snapshot.eState != SIR_WINDOW_LOST ||
	     sirGraphicsAcquireWindowSurface( windows, hB, nullptr ) ) {
		return false;
	}
	if ( !sirGraphicsReleaseWindowSurface( windows, &borrowB ) || windows.pump() != SIR_WINDOW_OK || windows.activeWindowCount() != 0 ||
	     windows.shutdown() != SIR_WINDOW_OK ) {
		return false;
	}
	return true;
}

static bool sirTestNativeInputTarget( void )
{
	SIRX11WindowDriver driver;
	SIRWindowSystem windows;
	SIRInputService input;
	SIRLinuxX11Backend backend;
	if ( windows.initialize( driver ) != SIR_WINDOW_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	if ( windows.create( { "SIREngine X11 Input", 220, 180, true }, &hWindow ) != SIR_WINDOW_OK ||
	     windows.bindInput( hWindow, input, backend ) != SIR_WINDOW_OK || !input.begin( 1 ) || windows.pump() != SIR_WINDOW_OK ||
	     input.activeDeviceCount() != 2 ) {
		return false;
	}
	input.sample();
	if ( !input.publish() || input.frame() == nullptr ) {
		return false;
	}
	input.shutdown();
	return windows.shutdown() == SIR_WINDOW_OK;
}

int main( void )
{
	if ( !sirTestTwoNativeWindows() || !sirTestNativeInputTarget() ) {
		fputs( "SIRWindowX11SystemTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRWindowX11SystemTests PASS" );
	return 0;
}
