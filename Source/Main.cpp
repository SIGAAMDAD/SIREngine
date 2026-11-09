#include <Core/SIRApplication.h>
#include <Scripting/Core/SIRScriptServer.h>
#include <Core/Platform/Input/SIRInputWindow.h>
#include <Core/Platform/Input/SIRInputService.h>
#if defined( _WIN32 )
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#include <Core/Platform/Input/Windows/SIRWindowsXInputBackend.h>
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
#include <Core/Platform/Input/Windows/SIRWindowsGameInputBackend.h>
#endif
#elif defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#include <Core/Platform/Input/Linux/SIRLinuxEvdevBackend.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#if defined( _WIN32 ) || defined( SIR_INPUT_X11_AVAILABLE )
typedef struct sirInputNativePumpContext_s
{
	SIRInputWindow *pWindow;
#if defined( SIR_INPUT_X11_AVAILABLE ) && !defined( _WIN32 )
	SIRLinuxEvdevBackend *pEvdev;
	bool bDiscoveryAttempted;

#endif
} sirInputNativePumpContext_t;

static bool sirPumpInputWindow( SIRInputService& input, void *pContext )
{
	sirInputNativePumpContext_t *pPump = ( sirInputNativePumpContext_t * )pContext;
#if defined( SIR_INPUT_X11_AVAILABLE ) && !defined( _WIN32 )
	if ( !pPump->bDiscoveryAttempted ) {
		pPump->bDiscoveryAttempted = true;
		( void )pPump->pEvdev->discover( input );
	}
#endif
	return pPump->pWindow->pump( input );
}
#endif

static int sirRunNativeInputWindow( void )
{
#if defined( _WIN32 ) || defined( SIR_INPUT_X11_AVAILABLE )
	SIRInputService input;
	SIRInputWindow window;
#if defined( _WIN32 )
	SIRWindowsRawBackend desktop;
	SIRWindowsXInputBackend legacyController;
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	SIRWindowsGameInputBackend modernController;
#endif
#else
	SIRLinuxX11Backend desktop;
	SIRLinuxEvdevBackend nativeControllers;
	( void )setlocale( LC_CTYPE, "" );
	const char *pszCookedProfiles = getenv( "SIR_INPUT_CONTROLLER_PROFILE_DB" );
	if ( pszCookedProfiles != nullptr && pszCookedProfiles[0] != 0 && !nativeControllers.loadCookedProfilesFile( pszCookedProfiles ) ) {
		fputs( "Unable to load cooked controller profiles\n", stderr );
		return 1;
	}
#endif
	if ( !window.open( input, desktop ) ) {
		fputs( "Unable to start native input window\n", stderr );
		return 1;
	}

#if defined( _WIN32 )
	// Only one Windows controller API is active at a time. This prevents the
	// same physical XInput-compatible pad from producing duplicate events.
#if defined( SIR_INPUT_GAMEINPUT_AVAILABLE )
	const bool bModern = modernController.initialize();
	if ( bModern && !input.attach( modernController ) ) {
		modernController.stop( input );
		window.close();
		input.shutdown();
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
	if ( !input.attach( nativeControllers ) ) {
		window.close();
		input.shutdown();
		return 1;
	}
#endif

	sirInputNativePumpContext_t context = {};
	context.pWindow = &window;
#if defined( SIR_INPUT_X11_AVAILABLE ) && !defined( _WIN32 )
	context.pEvdev = &nativeControllers;
#endif

	window.setTextEnabled( true );
	SIRApplication application;
	if ( !application.attachInput( input, sirPumpInputWindow, &context ) || !application.initialize() ) {
		window.close();
		input.shutdown();
		return 1;
	}

	while ( window.isOpen() ) {
		if ( !application.tick() ) {
			break;
		}
	}
	application.shutdown();
	window.close();
	return 0;
#else
	fputs( "Native input window backend unavailable on this build\n", stderr );
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
	if ( nArgs > 1 ) {
		if ( strcmp( pszArgs[1], "--version" ) == 0 ) {
			puts( "SIREngine native foundation " SIR_VERSION );
			return 0;
		}

		if ( strcmp( pszArgs[1], "--help" ) == 0 ) {
			puts( "Usage: SIREngine [--version|--help|--input-window|--scripting-smoke]" );
			return 0;
		}

		if ( strcmp( pszArgs[1], "--scripting-smoke" ) == 0 ) {
			return sirRunNativeScriptingSmoke();
		}

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
	application.shutdown();
	return bTicked ? 0 : 1;
}
