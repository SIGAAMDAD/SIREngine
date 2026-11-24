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
#if defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Linux/Window/SIRWindowDriverX11.h>
#include <Core/Platform/Linux/Input/SIRX11InputBridge.h>
#include <Core/Platform/Linux/Input/SIRLinuxEvdevBackend.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#if defined( SIR_INPUT_X11_AVAILABLE )
typedef struct sirNativeWindowPumpContext_s
{
	SIRWindowSystem *pWindows;
#if defined( SIR_INPUT_X11_AVAILABLE )
	SIRLinuxEvdevBackend *pEvdev;
	SIRX11InputBridge *pBridge;
	SIRInputService *pInput;
	bool bDiscoveryAttempted;
#endif
} sirNativeWindowPumpContext_t;

static SIRAppWindowPumpResult sirPumpNativeWindows( void *pContext )
{
	sirNativeWindowPumpContext_t *pPump = static_cast<sirNativeWindowPumpContext_t *>(pContext);
#if defined( SIR_INPUT_X11_AVAILABLE )
	if ( pPump->pBridge != nullptr && !pPump->pBridge->beginFrame() ) {
		return SIRAppWindowPumpResult::SIR_APP_WINDOW_ERROR;
	}
	if ( pPump->pInput != nullptr && !pPump->bDiscoveryAttempted ) {
		pPump->bDiscoveryAttempted = true;
		static_cast<void>(pPump->pEvdev->discover( *pPump->pInput ));
	}
#endif
	const sirWindowResult ePump = pPump->pWindows->pumpEvents( 256 );
	return ePump == sirWindowResult::SIR_WINDOW_OK || ePump == sirWindowResult::SIR_WINDOW_EVENTS_LOST ? SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE : SIRAppWindowPumpResult::SIR_APP_WINDOW_ERROR;
}

static void sirDispatchX11Input( const XEvent& event, void *pContext ) noexcept
{
	SIRX11InputBridge *pInput = static_cast<SIRX11InputBridge *>(pContext);
	pInput->processEvent( event );
}
#endif

#if defined( SIR_INPUT_WAYLAND_AVAILABLE )
typedef struct sirWaylandInputPumpContext_s
{
	SIRWindowSystem *pWindows;
	SIRWaylandInputBridge *pBridge;
	SIRInputService *pInput;
	SIRLinuxEvdevBackend *pEvdev;
	bool bDiscoveryAttempted;
} sirWaylandInputPumpContext_t;

static SIRAppWindowPumpResult sirPumpWaylandInputWindows( void *pContext )
{
	sirWaylandInputPumpContext_t *pPump = static_cast<sirWaylandInputPumpContext_t *>(pContext);
	if ( pPump->pBridge->isBound() && !pPump->pBridge->beginFrame() ) {
		return SIRAppWindowPumpResult::SIR_APP_WINDOW_ERROR;
	}
	if ( !pPump->bDiscoveryAttempted ) {
		pPump->bDiscoveryAttempted = true;
		static_cast<void>(pPump->pEvdev->discover( *pPump->pInput ));
	}
	const sirWindowResult ePump = pPump->pWindows->pumpEvents( 256 );
	return ePump == sirWindowResult::SIR_WINDOW_OK || ePump == sirWindowResult::SIR_WINDOW_EVENTS_LOST ? SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE : SIRAppWindowPumpResult::SIR_APP_WINDOW_ERROR;
}

static int sirRunWaylandInputWindow( bool bMultiwindow, uint64_t nMaxFrames )
{
	SIRInputService input;
	SIRWindowDriverWayland driver;
	SIRWaylandInputBridge bridge;
	SIRLinuxEvdevBackend nativeControllers;
	SIRWindowSystem windows( driver );
	sirWaylandInputPumpContext_t context = { &windows, &bridge, &input, &nativeControllers, false };
	const char *pszCookedProfiles = getenv( "SIR_INPUT_CONTROLLER_PROFILE_DB" );
	if ( pszCookedProfiles != nullptr && pszCookedProfiles[0] != 0 && !nativeControllers.loadCookedProfilesFile( pszCookedProfiles ) ) {
		fputs( "Unable to load cooked controller profiles\n", stderr );
		return 1;
	}
	SIRApplication app;
	if ( !app.attachInput( input ) || !app.registerSystem( &windows ) || !app.attachWindowPump( sirPumpWaylandInputWindows, &context ) ||
	     !app.initialize() ) {
		static_cast<void>(app.shutdown());
		return 1;
	}
	sirWindowHandle_t handles[2];
	memset( handles, 0, sizeof( handles ) );
	SIRNativeWindowLease leases[2];
	uintptr_t surfaces[2];
	memset( surfaces, 0, sizeof( surfaces ) );
	const uint32_t nRequested = bMultiwindow ? 2 : 1;
	uint32_t nCreated = 0;
	bool bSucceeded = input.attach( nativeControllers );
	for ( uint32_t i = 0; bSucceeded && i < nRequested; ++i ) {
		const sirWindowCreateDesc_t desc = { i == 0 ? "SIREngine native input" : "SIREngine auxiliary input", i == 0 ? 800U : 640U,
		                                     i == 0 ? 450U : 400U };
		if ( windows.createWindow( desc, &handles[i] ) != sirWindowResult::SIR_WINDOW_OK ) {
			bSucceeded = false;
			break;
		}
		++nCreated;
	}
	uint32_t nOpen = nCreated;
	uint64_t nFrame = 0;
	while ( bSucceeded && nOpen != 0 && ( nMaxFrames == 0 || nFrame < nMaxFrames ) ) {
		++nFrame;
		if ( !app.tick() ) {
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
			if ( snapshot.bCloseRequested ) {
				if ( leases[i].isValid() ) {
					bridge.unregisterWindow( surfaces[i] );
					if ( leases[i].reset() != sirWindowResult::SIR_WINDOW_OK ) {
						bSucceeded = false;
					}
				}
				if ( windows.destroyWindow( handles[i] ) != sirWindowResult::SIR_WINDOW_OK ) {
					bSucceeded = false;
				} else {
					handles[i] = {};
					--nOpen;
				}
				continue;
			}
			if ( snapshot.bReady && !leases[i].isValid() && driver.borrowSeat() != nullptr ) {
				sirWindowNativeView_t view {};
				if ( windows.acquireNativeLease( handles[i], leases[i] ) != sirWindowResult::SIR_WINDOW_OK ) {
					bSucceeded = false;
					break;
				}
				view = leases[i].getView();
				surfaces[i] = view.nNativeWindow;
				if ( !bridge.isBound() ) {
					bSucceeded = bridge.bind( input, driver, view );
					if ( bSucceeded ) {
						bridge.setTextEnabled( true );
					}
				} else {
					bSucceeded = bridge.registerWindow( view );
				}
				if ( !bSucceeded ) {
					static_cast<void>(leases[i].reset());
				}
			}
		}
	}
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
	return bSucceeded && app.shutdown() == SIRSystemResult::SIR_SYSTEM_OK ? 0 : 1;
}
#endif

int32_t SIRNativeWindowHost::runInput( bool bMultiwindow, uint64_t nMaxFrames ) noexcept
{
#if defined( SIR_INPUT_WAYLAND_AVAILABLE )
	const char *pszWaylandDisplay = getenv( "WAYLAND_DISPLAY" );
	if ( pszWaylandDisplay != nullptr && pszWaylandDisplay[0] != 0 ) {
		return sirRunWaylandInputWindow( bMultiwindow, nMaxFrames );
	}
#endif

#if defined( SIR_INPUT_X11_AVAILABLE )
	SIRInputService input;
	SIRWindowDriverX11 driver;
	SIRX11InputBridge bridge;
	SIRLinuxX11Backend desktop;
	SIRLinuxEvdevBackend nativeControllers;
	static_cast<void>(setlocale( LC_CTYPE, "" ));
	const char *pszCookedProfiles = getenv( "SIR_INPUT_CONTROLLER_PROFILE_DB" );
	if ( pszCookedProfiles != nullptr && pszCookedProfiles[0] != 0 && !nativeControllers.loadCookedProfilesFile( pszCookedProfiles ) ) {
		fputs( "Unable to load cooked controller profiles\n", stderr );
		return 1;
	}
	SIRWindowSystem windows( driver );
	sirNativeWindowPumpContext_t context {};
	context.pWindows = &windows;
#if defined( SIR_INPUT_X11_AVAILABLE )
	context.pInput = &input;
	context.pEvdev = &nativeControllers;
	context.pBridge = &bridge;
#endif
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
	if ( !driver.setEventSink( sirDispatchX11Input, &bridge ) || !input.attach( nativeControllers ) ) {
		static_cast<void>(driver.setEventSink( nullptr, nullptr ));
		bridge.unbind();
		static_cast<void>(leases[0].reset());
		static_cast<void>(windows.destroyWindow( hWindow ));
		static_cast<void>(application.shutdown());
		return 1;
	}
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
	static_cast<void>(driver.setEventSink( nullptr, nullptr ));
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
#else
	fputs( "Native input window backend unavailable on this build\n", stderr );
	return 1;
#endif
}

