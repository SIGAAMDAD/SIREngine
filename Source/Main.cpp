#include <Core/SIRApplication.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Window/SIRWindowSystem.h>
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#endif
#if defined( _WIN32 )
#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>
#include <Core/Platform/Input/Windows/SIRWin32InputBridge.h>
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#include <Core/Platform/Input/Windows/SIRWindowsXInputBackend.h>
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
#include <Core/Platform/Input/Windows/SIRWindowsGameInputBackend.h>
#endif
#elif defined( SIR_WINDOW_X11_AVAILABLE )
#include <Core/Platform/Window/Linux/SIRWindowDriverX11.h>
#include <Core/Platform/Input/Linux/SIRX11InputBridge.h>
#include <Core/Platform/Input/Linux/SIRLinuxEvdevBackend.h>
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
	result.nResult = pInput->processMessage( message.nMessage, message.nWParam, message.nLParam, &result.bHandled );
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

#if defined( _WIN32 ) || defined( SIR_WINDOW_X11_AVAILABLE ) || defined( SIR_WINDOW_WAYLAND_AVAILABLE )
static bool sirPumpWindowWithoutInput( void *pContext )
{
	SIRWindowSystem *pWindows = ( SIRWindowSystem * )pContext;
	return pWindows->pumpEvents( 256 ) == SIR_WINDOW_OK;
}

static int sirRunWindowWithoutInput( SIRWindowDriver& driver )
{
	SIRWindowSystem windows( driver );
	SIRApplication application;
	if ( !application.registerSystem( &windows ) || !application.attachWindowPump( sirPumpWindowWithoutInput, &windows ) ||
	     !application.initialize() ) {
		fputs( "Unable to initialize native window system\n", stderr );
		return 1;
	}

	sirWindowHandle_t hWindow = {};
	const sirWindowCreateDesc_t desc = { "SIREngine native window", 800, 450 };
	if ( windows.createWindow( desc, &hWindow ) != SIR_WINDOW_OK ) {
		fputs( "Unable to create native window\n", stderr );
		( void )application.shutdown();
		return 1;
	}

	bool bPumpSucceeded = true;
	while ( true ) {
		if ( !application.tick() ) {
			bPumpSucceeded = false;
			break;
		}
		sirWindowSnapshot_t snapshot = {};
		if ( windows.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK ) {
			bPumpSucceeded = false;
			break;
		}
		if ( snapshot.bCloseRequested ) {
			break;
		}
	}
	const sirWindowResult_t eDestroy = windows.destroyWindow( hWindow );
	const sirSystemResult_t eShutdown = application.shutdown();
	return bPumpSucceeded && eDestroy == SIR_WINDOW_OK && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
}

static int sirRunNativeWindowWithoutInput( void )
{
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	const char *pszWayland = getenv( "WAYLAND_DISPLAY" );
	if ( pszWayland != nullptr && pszWayland[0] != 0 ) {
		SIRWindowDriverWayland driver;
		return sirRunWindowWithoutInput( driver );
	}
#endif
#if defined( _WIN32 )
	SIRWindowDriverWin32 driver;
	return sirRunWindowWithoutInput( driver );
#elif defined( SIR_WINDOW_X11_AVAILABLE )
	SIRWindowDriverX11 driver;
	return sirRunWindowWithoutInput( driver );
#elif defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	SIRWindowDriverWayland driver;
	return sirRunWindowWithoutInput( driver );
#else
	return 1;
#endif
}
#endif

static int sirRunNativeInputWindow( void )
{
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
	bool bRunning = true;
	while ( bRunning ) {
		if ( !application.tick() ) {
			break;
		}
		sirWindowSnapshot_t snapshot = {};
		if ( windows.querySnapshot( hWindow, &snapshot ) != SIR_WINDOW_OK || snapshot.bCloseRequested ) {
			bRunning = false;
		}
	}
#if defined( _WIN32 )
	( void )driver.setMessageSink( nullptr, nullptr );
#else
	( void )driver.setEventSink( nullptr, nullptr );
#endif
	bridge.unbind();
	( void )windows.releaseNativeLease( lease );
	const sirWindowResult_t eDestroy = windows.destroyWindow( hWindow );
	const sirSystemResult_t eShutdown = application.shutdown();
	return eDestroy == SIR_WINDOW_OK && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
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
			puts( "Usage: SIREngine [--version|--help|--input-window|--window-no-input]" );
			return 0;
		}
#if defined( _WIN32 ) || defined( SIR_WINDOW_X11_AVAILABLE ) || defined( SIR_WINDOW_WAYLAND_AVAILABLE )
		if ( strcmp( pszArgs[1], "--window-no-input" ) == 0 ) {
			return sirRunNativeWindowWithoutInput();
		}
#endif

		if ( strcmp( pszArgs[1], "--input-window" ) == 0 ) {
			return sirRunNativeInputWindow();
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
