#include <Core/SIRApplication.h>
#include <Core/Platform/Input/SIRInputWindow.h>
#include <Core/Platform/Input/SIRInputService.h>
#if defined( _WIN32 )
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#elif defined( SIR_INPUT_X11_AVAILABLE )
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#endif

#include <stdio.h>
#include <string.h>

static bool sirPumpInputWindow( SIRInputService &input, void *pContext )
{
	return ( ( SIRInputWindow * )pContext )->pump( input );
}

static int sirRunNativeInputWindow( void )
{
#if defined( _WIN32 ) || defined( SIR_INPUT_X11_AVAILABLE )
	SIRInputService input;
	SIRInputWindow window;
#if defined( _WIN32 )
	SIRWindowsRawBackend desktop;
#else
	SIRLinuxX11Backend desktop;
#endif
	if ( !window.open( input, desktop ) ) {
		fputs( "Unable to start native input window\n", stderr );
		return 1;
	}

	SIRApplication application;
	if ( !application.attachInput( input, sirPumpInputWindow, &window ) || !application.initialize() ) {
		window.close();
		input.shutdown();
		return 1;
	}

	while ( window.isOpen() ) {
		if ( !application.tick() ) {
			break;
		}
	}
	window.close();
	application.shutdown();
	return 0;
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
			puts( "Usage: SIREngine [--version|--help|--input-window]" );
			return 0;
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
