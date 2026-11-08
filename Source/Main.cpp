#include <Core/SIRApplication.h>
#include <Core/Input/SIRInputService.h>
#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Platform/Window/SIRNativeWindowHost.h>
#if defined( SIR_INPUT_WAYLAND_AVAILABLE )
#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#include <Core/Platform/Linux/SIRWaylandInputBridge.h>
#include <Core/Platform/Linux/SIRLinuxEvdevBackend.h>
#endif
#if defined( _WIN32 )
#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>
#include <Core/Platform/Windows/SIRWin32InputBridge.h>
#include <Core/Platform/Windows/SIRWindowsRawBackend.h>
#include <Core/Platform/Windows/SIRWindowsXInputBackend.h>
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
#include <Core/Platform/Windows/SIRWindowsGameInputBackend.h>
#endif
#elif defined( SIR_WINDOW_X11_AVAILABLE )
#include <Core/Platform/Window/Linux/SIRWindowDriverX11.h>
#include <Core/Platform/Linux/SIRX11InputBridge.h>
#include <Core/Platform/Linux/SIRLinuxEvdevBackend.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#if defined( _WIN32 ) || defined( SIR_WINDOW_X11_AVAILABLE )
typedef struct sirNativeWindowPumpContext_s
{
	SIRWindowSystem *pWindows;
#if defined( _WIN32 )
	SIRWin32InputBridge *pWinInput;
#endif
#if defined( SIR_WINDOW_X11_AVAILABLE ) && !defined( _WIN32 )
	SIRLinuxEvdevBackend *pEvdev;
	SIRX11InputBridge *pBridge;
	SIRInputService *pInput;
	bool bDiscoveryAttempted;
#endif
} sirNativeWindowPumpContext_t;

static bool sirPumpNativeWindows( void *pContext )
{
	sirNativeWindowPumpContext_t *pPump = ( sirNativeWindowPumpContext_t * )pContext;
#if defined( SIR_WINDOW_X11_AVAILABLE ) && !defined( _WIN32 )
	if ( pPump->pBridge != nullptr && !pPump->pBridge->beginFrame() ) {
		return false;
	}
	if ( pPump->pInput != nullptr && !pPump->bDiscoveryAttempted ) {
		pPump->bDiscoveryAttempted = true;
		( void )pPump->pEvdev->discover( *pPump->pInput );
	}
#endif
	const sirWindowResult_t ePump = pPump->pWindows->pumpEvents( 256 );
#if defined( _WIN32 )
	if ( pPump->pWinInput != nullptr ) {
		pPump->pWinInput->finishPump();
	}
#endif
	return ePump == SIR_WINDOW_OK;
}

#if defined( _WIN32 )
static sirWindowWin32Dispatch_t sirDispatchWin32Input( const sirWindowWin32Message_t& message, void *pContext ) noexcept
{
	SIRWin32InputBridge *pInput = ( SIRWin32InputBridge * )pContext;
	sirWindowWin32Dispatch_t result = {};
	if ( !pInput->acceptsWindow( message.nWindow ) ) {
		return result;
	}
	result.nResult = pInput->processWindowMessage( message, &result.bHandled );
	return result;
}
#else
static void sirDispatchX11Input( const XEvent& event, void *pContext ) noexcept
{
	SIRX11InputBridge *pInput = ( SIRX11InputBridge * )pContext;
	pInput->processEvent( event );
}
#endif
#endif

#if defined( SIR_INPUT_WAYLAND_AVAILABLE ) && !defined( _WIN32 )
typedef struct sirWaylandInputPumpContext_s
{
	SIRWindowSystem *pWindows;
	SIRWaylandInputBridge *pBridge;
	SIRInputService *pInput;
	SIRLinuxEvdevBackend *pEvdev;
	bool bDiscoveryAttempted;
} sirWaylandInputPumpContext_t;

static bool sirPumpWaylandInputWindows( void *pContext )
{
	sirWaylandInputPumpContext_t *pPump = ( sirWaylandInputPumpContext_t * )pContext;
	if ( pPump->pBridge->isBound() && !pPump->pBridge->beginFrame() ) {
		return false;
	}
	if ( !pPump->bDiscoveryAttempted ) {
		pPump->bDiscoveryAttempted = true;
		( void )pPump->pEvdev->discover( *pPump->pInput );
	}
	return pPump->pWindows->pumpEvents( 256 ) == SIR_WINDOW_OK;
}

static int sirRunWaylandInputWindow( bool bMultiwindow )
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
		( void )app.shutdown();
		return 1;
	}
	sirWindowHandle_t handles[2] = {};
	sirWindowNativeLease_t leases[2] = {};
	uintptr_t surfaces[2] = {};
	const uint32_t nRequested = bMultiwindow ? 2 : 1;
	uint32_t nCreated = 0;
	bool bSucceeded = input.attach( nativeControllers );
	for ( uint32_t i = 0; bSucceeded && i < nRequested; ++i ) {
		const sirWindowCreateDesc_t desc = { i == 0 ? "SIREngine native input" : "SIREngine auxiliary input", i == 0 ? 800U : 640U,
		                                     i == 0 ? 450U : 400U };
		if ( windows.createWindow( desc, &handles[i] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
			break;
		}
		++nCreated;
	}
	uint32_t nOpen = nCreated;
	while ( bSucceeded && nOpen != 0 ) {
		if ( !app.tick() ) {
			bSucceeded = false;
			break;
		}
		for ( uint32_t i = 0; i < nCreated && bSucceeded; ++i ) {
			if ( handles[i].nGeneration == 0 ) {
				continue;
			}
			sirWindowSnapshot_t snapshot = {};
			if ( windows.querySnapshot( handles[i], &snapshot ) != SIR_WINDOW_OK ) {
				bSucceeded = false;
				break;
			}
			if ( snapshot.bCloseRequested ) {
				if ( leases[i].nToken != 0 ) {
					bridge.unregisterWindow( surfaces[i] );
					if ( windows.releaseNativeLease( leases[i] ) != SIR_WINDOW_OK ) {
						bSucceeded = false;
					}
					leases[i] = {};
				}
				if ( windows.destroyWindow( handles[i] ) != SIR_WINDOW_OK ) {
					bSucceeded = false;
				} else {
					handles[i] = {};
					--nOpen;
				}
				continue;
			}
			if ( snapshot.bReady && leases[i].nToken == 0 && driver.borrowSeat() != nullptr ) {
				sirWindowNativeView_t view = {};
				if ( windows.acquireNativeLease( handles[i], &leases[i], &view ) != SIR_WINDOW_OK ) {
					bSucceeded = false;
					break;
				}
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
					( void )windows.releaseNativeLease( leases[i] );
					leases[i] = {};
				}
			}
		}
	}
	bridge.unbind();
	while ( nCreated != 0 ) {
		--nCreated;
		if ( leases[nCreated].nToken != 0 && windows.releaseNativeLease( leases[nCreated] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
		if ( handles[nCreated].nGeneration != 0 && windows.destroyWindow( handles[nCreated] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
	}
	return bSucceeded && app.shutdown() == SIR_SYSTEM_OK ? 0 : 1;
}
#endif

static int sirRunNativeInputWindow( bool bMultiwindow )
{
#if defined( SIR_INPUT_WAYLAND_AVAILABLE ) && !defined( _WIN32 )
	const char *pszWaylandDisplay = getenv( "WAYLAND_DISPLAY" );
	if ( pszWaylandDisplay != nullptr && pszWaylandDisplay[0] != 0 ) {
		return sirRunWaylandInputWindow( bMultiwindow );
	}
#endif

#if defined( _WIN32 ) || defined( SIR_WINDOW_X11_AVAILABLE )
	SIRInputService input;
#if defined( _WIN32 )
	SIRWindowDriverWin32 driver;
	SIRWin32InputBridge bridge;
	SIRWindowsRawBackend desktop;
	SIRWindowsXInputBackend legacyController;
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	SIRWindowsGameInputBackend modernController;
#endif
#else
	SIRWindowDriverX11 driver;
	SIRX11InputBridge bridge;
	SIRLinuxX11Backend desktop;
	SIRLinuxEvdevBackend nativeControllers;
	( void )setlocale( LC_CTYPE, "" );
	const char *pszCookedProfiles = getenv( "SIR_INPUT_CONTROLLER_PROFILE_DB" );
	if ( pszCookedProfiles != nullptr && pszCookedProfiles[0] != 0 && !nativeControllers.loadCookedProfilesFile( pszCookedProfiles ) ) {
		fputs( "Unable to load cooked controller profiles\n", stderr );
		return 1;
	}
#endif
	SIRWindowSystem windows( driver );
	sirNativeWindowPumpContext_t context = {};
	context.pWindows = &windows;
#if defined( _WIN32 )
	context.pWinInput = &bridge;
#endif
#if defined( SIR_WINDOW_X11_AVAILABLE ) && !defined( _WIN32 )
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
	sirWindowHandle_t hWindow = {};
	const sirWindowCreateDesc_t desc = { "SIREngine native input", 800, 450 };
	if ( windows.createWindow( desc, &hWindow ) != SIR_WINDOW_OK ) {
		fputs( "Unable to create native window\n", stderr );
		( void )application.shutdown();
		return 1;
	}
	sirWindowNativeLease_t lease = {};
	sirWindowNativeView_t view = {};
	if ( windows.acquireNativeLease( hWindow, &lease, &view ) != SIR_WINDOW_OK || !bridge.bind( input, desktop, view ) ) {
		fputs( "Unable to attach native input bridge\n", stderr );
		if ( lease.nToken != 0 ) {
			( void )windows.releaseNativeLease( lease );
		}
		( void )windows.destroyWindow( hWindow );
		( void )application.shutdown();
		return 1;
	}
#if defined( _WIN32 )
	if ( !driver.setMessageSink( sirDispatchWin32Input, &bridge ) ) {
		bridge.unbind();
		( void )windows.releaseNativeLease( lease );
		( void )windows.destroyWindow( hWindow );
		( void )application.shutdown();
		return 1;
	}
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	const bool bModern = modernController.initialize();
	if ( bModern && !input.attach( modernController ) ) {
		modernController.stop( input );
		( void )driver.setMessageSink( nullptr, nullptr );
		bridge.unbind();
		( void )windows.releaseNativeLease( lease );
		( void )windows.destroyWindow( hWindow );
		( void )application.shutdown();
		return 1;
	}
	if ( !bModern && legacyController.initialize() ) {
		( void )input.attach( legacyController );
	}
#else
	if ( legacyController.initialize() ) {
		( void )input.attach( legacyController );
	}
#endif
#else
	if ( !driver.setEventSink( sirDispatchX11Input, &bridge ) || !input.attach( nativeControllers ) ) {
		( void )driver.setEventSink( nullptr, nullptr );
		bridge.unbind();
		( void )windows.releaseNativeLease( lease );
		( void )windows.destroyWindow( hWindow );
		( void )application.shutdown();
		return 1;
	}
#endif
	bridge.setTextEnabled( true );
	sirWindowHandle_t handles[2] = { hWindow, {} };
	sirWindowNativeLease_t leases[2] = { lease, {} };
	sirWindowNativeView_t views[2] = { view, {} };
	uint32_t nCreated = 1;
	bool bSucceeded = true;
	if ( bMultiwindow ) {
		if ( windows.createWindow( { "SIREngine auxiliary input", 640, 400 }, &handles[1] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
		} else {
			nCreated = 2;
			if ( windows.acquireNativeLease( handles[1], &leases[1], &views[1] ) != SIR_WINDOW_OK || !bridge.registerWindow( views[1] ) ) {
				bSucceeded = false;
			}
		}
	}
	uint32_t nOpen = nCreated;
	while ( bSucceeded && nOpen != 0 ) {
		if ( !application.tick() ) {
			bSucceeded = false;
			break;
		}
		for ( uint32_t i = 0; i < nCreated && bSucceeded; ++i ) {
			if ( handles[i].nGeneration == 0 ) {
				continue;
			}
			sirWindowSnapshot_t snapshot = {};
			if ( windows.querySnapshot( handles[i], &snapshot ) != SIR_WINDOW_OK ) {
				bSucceeded = false;
				break;
			}
			if ( !snapshot.bCloseRequested ) {
				continue;
			}
			if ( !bridge.unregisterWindow( views[i].nNativeWindow ) ) {
				bSucceeded = false;
			}
			if ( leases[i].nToken != 0 && windows.releaseNativeLease( leases[i] ) != SIR_WINDOW_OK ) {
				bSucceeded = false;
			}
			leases[i] = {};
			if ( windows.destroyWindow( handles[i] ) != SIR_WINDOW_OK ) {
				bSucceeded = false;
			} else {
				handles[i] = {};
				--nOpen;
			}
		}
	}
#if defined( _WIN32 )
	( void )driver.setMessageSink( nullptr, nullptr );
#else
	( void )driver.setEventSink( nullptr, nullptr );
#endif
	bridge.unbind();
	while ( nCreated != 0 ) {
		--nCreated;
		if ( leases[nCreated].nToken != 0 && windows.releaseNativeLease( leases[nCreated] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
		if ( handles[nCreated].nGeneration != 0 && windows.destroyWindow( handles[nCreated] ) != SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
	}
	const sirSystemResult_t eShutdown = application.shutdown();
	return bSucceeded && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
#else
	fputs( "Native input window backend unavailable on this build\n", stderr );
	return 1;
#endif
}

int main( int nArgs, char **pszArgs )
{
	if ( nArgs > 1 ) {
		if ( strcmp( pszArgs[1], "--version" ) == 0 ) {
			puts( "SIREngine native foundation " SIR_VERSION );
			return 0;
		}
		if ( strcmp( pszArgs[1], "--help" ) == 0 ) {
			puts( "Usage: SIREngine [--version|--help|--input-window|--input-multi|--window-no-input|--window-multi]" );
			return 0;
		}
#if defined( _WIN32 ) || defined( SIR_WINDOW_X11_AVAILABLE ) || defined( SIR_WINDOW_WAYLAND_AVAILABLE )
		if ( strcmp( pszArgs[1], "--window-no-input" ) == 0 ) {
			return sirRunNativeWindowHost( { "SIREngine native window", 800, 450 } );
		}
		if ( strcmp( pszArgs[1], "--window-multi" ) == 0 ) {
			const sirWindowCreateDesc_t windows[] = { { "SIREngine primary", 800, 450 }, { "SIREngine auxiliary", 640, 400 } };
			return sirRunNativeWindowHosts( windows, 2 );
		}
#endif

		if ( strcmp( pszArgs[1], "--input-window" ) == 0 ) {
			return sirRunNativeInputWindow( false );
		}
		if ( strcmp( pszArgs[1], "--input-multi" ) == 0 ) {
			return sirRunNativeInputWindow( true );
		}
		fputs( "Unknown option\n", stderr );
		return 2;
	}
	SIRApplication application;
	if ( !application.initialize() ) {
		return 1;
	}
	const bool bTicked = application.tick();
	( void )application.shutdown();
	return bTicked ? 0 : 1;
}
