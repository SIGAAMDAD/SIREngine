#include <Core/SIRApplication.h>
#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Platform/Window/SIRWindowService.h>
#include <Scripting/Core/SIRScriptServer.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/SIRInputBackend.h>
#include <Core/Platform/SIRPlatformTime.h>
#if defined( _WIN32 )
#include <Core/Platform/Window/SIRNativeWindowDrivers.h>
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#include <Core/Platform/Input/Windows/SIRWindowsXInputBackend.h>
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
#include <Core/Platform/Input/Windows/SIRWindowsGameInputBackend.h>
#endif
#elif defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Window/SIRNativeWindowDrivers.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#include <Core/Platform/Input/Linux/SIRLinuxEvdevBackend.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <limits.h>

typedef struct sirNativeWindowPumpContext_s
{
	SIRWindowService *pWindowService;
#if defined( SIR_INPUT_X11_AVAILABLE ) && !defined( _WIN32 )
	SIRInputService *pInput;
	SIRLinuxEvdevBackend *pControllers;
	bool bDiscoveryAttempted;
#endif
} sirNativeWindowPumpContext_t;

static sirAppWindowPumpResult_t sirPumpNativeWindows( void *pContext )
{
	sirNativeWindowPumpContext_t *pPump = ( sirNativeWindowPumpContext_t * )pContext;
#if defined( SIR_INPUT_X11_AVAILABLE ) && !defined( _WIN32 )
	if ( !pPump->bDiscoveryAttempted ) {
		pPump->bDiscoveryAttempted = true;
		( void )pPump->pControllers->discover( *pPump->pInput );
	}
#endif
	const sirWindowResult_t eResult = pPump->pWindowService->pump();
	if ( eResult != SIR_WINDOW_OK && eResult != SIR_WINDOW_BUSY ) {
		return SIR_APP_WINDOW_ERROR;
	}
	return pPump->pWindowService->hasLiveWindows() ? SIR_APP_WINDOW_CONTINUE : SIR_APP_WINDOW_STOP;
}

static bool sirParseFrameCount( const char *pszValue, uint64_t *pFrames ) noexcept
{
	if ( pszValue == nullptr || *pszValue == '\0' || pFrames == nullptr ) {
		return false;
	}
	uint64_t nFrames = 0;
	for ( const char *pChar = pszValue; *pChar != '\0'; ++pChar ) {
		if ( *pChar < '0' || *pChar > '9' ) {
			return false;
		}
		const uint64_t nDigit = ( uint64_t )( *pChar - '0' );
		if ( nFrames > ( UINT64_MAX - nDigit ) / 10 ) {
			return false;
		}
		nFrames = nFrames * 10 + nDigit;
	}
	if ( nFrames == 0 ) {
		return false;
	}
	*pFrames = nFrames;
	return true;
}

static int sirRunHeadless( uint64_t nMaxFrames ) noexcept
{
	SIRApplication application;
	if ( nMaxFrames == 0 || !application.initialize() ) {
		return 1;
	}
	const sirAppRunResult_t eRun = application.run( nMaxFrames );
	const sirSystemResult_t eShutdown = application.shutdown();
	return eRun == SIR_APP_RUN_OK && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
}

static int sirRunNativeWindow( uint64_t nMaxFrames ) noexcept
{
#if defined( _WIN32 ) || defined( SIR_INPUT_X11_AVAILABLE )
	SIRInputService input;
#if defined( _WIN32 )
	SIRWin32WindowDriver driver;
	SIRWindowSystem windows;
	SIRWindowsRawBackend desktop;
	SIRWindowsXInputBackend legacyController;
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	SIRWindowsGameInputBackend modernController;
#endif
#else
	SIRX11WindowDriver driver;
	SIRWindowSystem windows;
	SIRLinuxX11Backend desktop;
	SIRLinuxEvdevBackend nativeControllers;
	( void )setlocale( LC_CTYPE, "" );
	const char *pszCookedProfiles = getenv( "SIR_INPUT_CONTROLLER_PROFILE_DB" );
	if ( pszCookedProfiles != nullptr && pszCookedProfiles[0] != '\0' && !nativeControllers.loadCookedProfilesFile( pszCookedProfiles ) ) {
		fputs( "Unable to load cooked controller profiles\n", stderr );
		return 1;
	}
#endif

	const sirWindowDesc_t desc = { "SIREngine", 1280, 720, true };
	SIRWindowService windowService( windows, driver, desc, &input );
	if ( !windowService.setDesktopBackend( desktop ) ) {
		return 1;
	}

#if defined( _WIN32 )
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	const bool bModern = modernController.initialize();
	if ( bModern ) {
		if ( !input.attach( modernController ) ) {
			input.shutdown();
			return 1;
		}
	} else if ( legacyController.initialize() ) {
		( void )input.attach( legacyController );
	}
#else
	if ( legacyController.initialize() ) {
		( void )input.attach( legacyController );
	}
#endif
#else
	if ( !input.attach( nativeControllers ) ) {
		input.shutdown();
		return 1;
	}
#endif

	SIRApplication application;
	sirNativeWindowPumpContext_t context = {};
	context.pWindowService = &windowService;
#if defined( SIR_INPUT_X11_AVAILABLE ) && !defined( _WIN32 )
	context.pControllers = &nativeControllers;
	context.pInput = &input;
#endif
	const bool bRegistered = application.registerSystem( &windowService ) && application.attachInput( input ) &&
	                         application.attachWindowPump( sirPumpNativeWindows, &context );
	if ( !bRegistered || !application.initialize() ) {
		( void )application.shutdown();
		input.shutdown();
		return 1;
	}
	const sirAppRunResult_t eRun = application.run( nMaxFrames );
	const sirSystemResult_t eShutdown = application.shutdown();
	return eRun != SIR_APP_RUN_ERROR && eShutdown == SIR_SYSTEM_OK ? 0 : 1;
#else
	( void )nMaxFrames;
	fputs( "Native window backend unavailable on this build\n", stderr );
	return 1;
#endif
}

// Verify that the native engine executable links and initializes scripting.
static int sirRunNativeScriptingSmoke( void )
{
	SIRScriptServer scripting;
	if ( scripting.bindings().publish() != SIR_SCRIPT_OK ) {
		fputs( "Scripting binding publication failed\n", stderr );
		return 1;
	}
	puts( "Sir Scroll native scripting ready" );
	return 0;
}

int main( int nArgs, char **pszArgs )
{
	bool bHeadless = false;
	uint64_t nFrames = 0;
	for ( int iArg = 1; iArg < nArgs; ++iArg ) {
		if ( strcmp( pszArgs[iArg], "--version" ) == 0 && nArgs == 2 ) {
			puts( "SIREngine native foundation " SIR_VERSION );
			return 0;
		}
		if ( strcmp( pszArgs[iArg], "--help" ) == 0 && nArgs == 2 ) {
			puts( "Usage: SIREngine [--version|--help|--scripting-smoke|--headless --frames N|--frames N|--input-window]" );
			return 0;
		}
		if ( strcmp( pszArgs[iArg], "--scripting-smoke" ) == 0 && nArgs == 2 ) {
			return sirRunNativeScriptingSmoke();
		}
		if ( strcmp( pszArgs[iArg], "--headless" ) == 0 ) {
			bHeadless = true;
			continue;
		}
		if ( strcmp( pszArgs[iArg], "--input-window" ) == 0 ) {
			continue;
		}
		if ( strcmp( pszArgs[iArg], "--frames" ) == 0 && iArg + 1 < nArgs && nFrames == 0 ) {
			if ( !sirParseFrameCount( pszArgs[++iArg], &nFrames ) ) {
				fputs( "Invalid frame count\n", stderr );
				return 2;
			}
			continue;
		}
		fputs( "Unknown option\n", stderr );
		return 2;
	}
	if ( bHeadless ) {
		return sirRunHeadless( nFrames );
	}
	return sirRunNativeWindow( nFrames );
}
