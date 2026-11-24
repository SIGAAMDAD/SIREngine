/* SIREngine MPLv2 Source Code. Copyright (C) 2023-2027 Noah Van Til.
 * This Source Code Form is subject to the Mozilla Public License, v2.0. */
#include <Core/SIRApplication.h>
#include <Core/Input/SIRInputService.h>
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
#include <Core/Platform/Shared/Window/SIRNativeWindowLease.h>
#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#if defined( SIR_INPUT_WAYLAND_AVAILABLE )
#include <Core/Platform/Linux/Window/SIRWindowDriverWayland.h>
#include <Core/Platform/Linux/Input/SIRWaylandInputBridge.h>
#include <Core/Platform/Linux/Input/SIRLinuxEvdevBackend.h>
#endif
#include <Core/Platform/Windows/Window/SIRWindowDriverWin32.h>
#include <Core/Platform/Windows/Input/SIRWin32InputBridge.h>
#include <Core/Platform/Windows/Input/SIRWindowsRawBackend.h>
#include <Core/Platform/Windows/Input/SIRWindowsXInputBackend.h>
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
#include <Core/Platform/Windows/Input/SIRWindowsGameInputBackend.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

typedef struct sirNativeWindowPumpContext_s
{
	SIRWindowSystem *pWindows;
	SIRWin32InputBridge *pWinInput;
} sirNativeWindowPumpContext_t;

static SIRAppWindowPumpResult sirPumpNativeWindows( void *pContext )
{
	sirNativeWindowPumpContext_t *pPump = static_cast<sirNativeWindowPumpContext_t *>(pContext);
	const sirWindowResult ePump = pPump->pWindows->pumpEvents( 256 );
	if ( pPump->pWinInput != nullptr ) {
		pPump->pWinInput->finishPump();
	}
	return ePump == sirWindowResult::SIR_WINDOW_OK || ePump == sirWindowResult::SIR_WINDOW_EVENTS_LOST ? SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE : SIRAppWindowPumpResult::SIR_APP_WINDOW_ERROR;
}

static sirWindowWin32Dispatch_t sirDispatchWin32Input( const sirWindowWin32Message_t& message, void *pContext ) noexcept
{
	SIRWin32InputBridge *pInput = static_cast<SIRWin32InputBridge *>(pContext);
	sirWindowWin32Dispatch_t result {};
	if ( !pInput->acceptsWindow( message.nWindow ) ) {
		return result;
	}
	result.nResult = pInput->processWindowMessage( message, &result.bHandled );
	return result;
}


int32_t SIRNativeWindowHost::runInput( bool bMultiwindow, uint64_t nMaxFrames ) noexcept
{

	SIRInputService input;
	SIRWindowDriverWin32 driver;
	SIRWin32InputBridge bridge;
	SIRWindowsRawBackend desktop;
	SIRWindowsXInputBackend legacyController;
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	SIRWindowsGameInputBackend modernController;
#endif
	SIRWindowSystem windows( driver );
	sirNativeWindowPumpContext_t context {};
	context.pWindows = &windows;
	context.pWinInput = &bridge;
	SIRApplication application;
	if ( !application.attachInput( input ) || !application.registerSystem( &windows ) ||
	     !application.attachWindowPump( sirPumpNativeWindows, &context ) || !application.initialize() ) {
		fputs( "Unable to initialize native window system\n", stderr );
		return 1;
	}
	sirWindowHandle_t hWindow {};
	const sirWindowCreateDesc_t desc = { "SIREngine native input", 800, 450 };
	if ( windows.createWindow( desc, &hWindow ) != sirWindowResult::SIR_WINDOW_OK ) {
		fputs( "Unable to create native window\n", stderr );
		static_cast<void>(application.shutdown());
		return 1;
	}
	SIRNativeWindowLease leases[2];
	if ( windows.acquireNativeLease( hWindow, leases[0] ) != sirWindowResult::SIR_WINDOW_OK || !bridge.bind( input, desktop, leases[0].getView() ) ) {
		fputs( "Unable to attach native input bridge\n", stderr );
		if ( leases[0].isValid() ) {
			static_cast<void>(leases[0].reset());
		}
		static_cast<void>(windows.destroyWindow( hWindow ));
		static_cast<void>(application.shutdown());
		return 1;
	}
	if ( !driver.setMessageSink( sirDispatchWin32Input, &bridge ) ) {
		bridge.unbind();
		static_cast<void>(leases[0].reset());
		static_cast<void>(windows.destroyWindow( hWindow ));
		static_cast<void>(application.shutdown());
		return 1;
	}
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	const bool bModern = modernController.initialize();
	if ( bModern && !input.attach( modernController ) ) {
		modernController.stop( input );
		static_cast<void>(driver.setMessageSink( nullptr, nullptr ));
		bridge.unbind();
		static_cast<void>(leases[0].reset());
		static_cast<void>(windows.destroyWindow( hWindow ));
		static_cast<void>(application.shutdown());
		return 1;
	}
	if ( !bModern && legacyController.initialize() ) {
		static_cast<void>(input.attach( legacyController ));
	}
#else
	if ( legacyController.initialize() ) {
		static_cast<void>(input.attach( legacyController ));
	}
#endif
	bridge.setTextEnabled( true );
	sirWindowHandle_t handles[2] = { hWindow, {} };
	uint32_t nCreated = 1;
	bool bSucceeded = true;
	if ( bMultiwindow ) {
		if ( windows.createWindow( { "SIREngine auxiliary input", 640, 400 }, &handles[1] ) != sirWindowResult::SIR_WINDOW_OK ) {
			bSucceeded = false;
		} else {
			nCreated = 2;
			if ( windows.acquireNativeLease( handles[1], leases[1] ) != sirWindowResult::SIR_WINDOW_OK || !bridge.registerWindow( leases[1].getView() ) ) {
				bSucceeded = false;
			}
		}
	}
	uint32_t nOpen = nCreated;
	uint64_t nFrame = 0;
	while ( bSucceeded && nOpen != 0 && ( nMaxFrames == 0 || nFrame < nMaxFrames ) ) {
		++nFrame;
		if ( !application.tick() ) {
			bSucceeded = false;
			break;
		}
		for ( uint32_t i = 0; i < nCreated && bSucceeded; ++i ) {
			if ( handles[i].nGeneration == 0 ) {
				continue;
			}
			sirWindowSnapshot_t snapshot {};
			if ( windows.querySnapshot( handles[i], &snapshot ) != sirWindowResult::SIR_WINDOW_OK ) {
				bSucceeded = false;
				break;
			}
			if ( !snapshot.bCloseRequested ) {
				continue;
			}
			if ( !bridge.unregisterWindow( leases[i].getView().nNativeWindow ) ) {
				bSucceeded = false;
			}
			if ( leases[i].isValid() && leases[i].reset() != sirWindowResult::SIR_WINDOW_OK ) {
				bSucceeded = false;
			}
			if ( windows.destroyWindow( handles[i] ) != sirWindowResult::SIR_WINDOW_OK ) {
				bSucceeded = false;
			} else {
				handles[i] = {};
				--nOpen;
			}
		}
	}
	static_cast<void>(driver.setMessageSink( nullptr, nullptr ));
	bridge.unbind();
	while ( nCreated != 0 ) {
		--nCreated;
		if ( leases[nCreated].isValid() && leases[nCreated].reset() != sirWindowResult::SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
		if ( handles[nCreated].nGeneration != 0 && windows.destroyWindow( handles[nCreated] ) != sirWindowResult::SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
	}
	const SIRSystemResult eShutdown = application.shutdown();
	return bSucceeded && eShutdown == SIRSystemResult::SIR_SYSTEM_OK ? 0 : 1;
}

