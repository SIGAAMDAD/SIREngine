#include <Core/SIRApplication.h>
#include <Scripting/Core/SIRScriptServer.h>
#include <limits.h>
#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

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
		const uint64_t nDigit = static_cast<uint64_t>( *pChar - '0' );
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
	const SIRAppRunResult eRun = application.run( nMaxFrames );
	const SIRSystemResult eShutdown = application.shutdown();
	return eRun == SIRAppRunResult::SIR_APP_RUN_OK && eShutdown == SIRSystemResult::SIR_SYSTEM_OK ? 0 : 1;
}

static int sirRunNativeScriptingSmoke( void )
{
	SIRScriptServer scripting;
	if ( scripting.bindings().publish() != SIRScriptResult::SIR_SCRIPT_OK ) {
		fputs( "Scripting binding publication failed\n", stderr );
		return 1;
	}
	puts( "Sir Scroll native scripting ready" );
	return 0;
}

int main( int nArgs, char **pszArgs )
{
	bool bHeadless = false;
	bool bInputWindow = false;
	bool bInputMulti = false;
	bool bWindowNoInput = false;
	bool bWindowMulti = false;
	uint64_t nFrames = 0;
	if ( nArgs == 2 && strcmp( pszArgs[1], "--version" ) == 0 ) {
		puts( "SIREngine native foundation " SIR_VERSION );
		return 0;
	}
	if ( nArgs == 2 && strcmp( pszArgs[1], "--help" ) == 0 ) {
		puts( "Usage: SIREngine [--version|--help|--scripting-smoke|--headless --frames N|--frames "
		      "N|--input-window|--input-multi|--window-no-input|--window-multi]" );
		return 0;
	}
	if ( nArgs == 2 && strcmp( pszArgs[1], "--scripting-smoke" ) == 0 ) {
		return sirRunNativeScriptingSmoke();
	}
	for ( int iArg = 1; iArg < nArgs; ++iArg ) {
		if ( strcmp( pszArgs[iArg], "--headless" ) == 0 ) {
			bHeadless = true;
		} else if ( strcmp( pszArgs[iArg], "--input-window" ) == 0 ) {
			bInputWindow = true;
		} else if ( strcmp( pszArgs[iArg], "--input-multi" ) == 0 ) {
			bInputMulti = true;
		} else if ( strcmp( pszArgs[iArg], "--window-no-input" ) == 0 ) {
			bWindowNoInput = true;
		} else if ( strcmp( pszArgs[iArg], "--window-multi" ) == 0 ) {
			bWindowMulti = true;
		} else if ( strcmp( pszArgs[iArg], "--frames" ) == 0 && iArg + 1 < nArgs && nFrames == 0 ) {
			if ( !sirParseFrameCount( pszArgs[++iArg], &nFrames ) ) {
				fputs( "Invalid frame count\n", stderr );
				return 2;
			}
		} else {
			fputs( "Unknown option\n", stderr );
			return 2;
		}
	}
	const uint32_t nModes =
	    static_cast<uint32_t>(bInputWindow) + static_cast<uint32_t>(bInputMulti) + static_cast<uint32_t>(bWindowNoInput) + static_cast<uint32_t>(bWindowMulti) + static_cast<uint32_t>(bHeadless);
	if ( nModes > 1 || ( bHeadless && nFrames == 0 ) || ( ( bWindowNoInput || bWindowMulti ) && nFrames != 0 ) ) {
		fputs( "Invalid window mode or frame count\n", stderr );
		return 2;
	}
	if ( bHeadless ) {
		return sirRunHeadless( nFrames );
	}
	if ( bWindowNoInput ) {
		const sirWindowCreateDesc_t window{ "SIREngine native window", 800, 450 };
		return SIRNativeWindowHost::runNative( &window, 1 );
	}
	if ( bWindowMulti ) {
		const sirWindowCreateDesc_t windows[] = { { "SIREngine primary", 800, 450 }, { "SIREngine auxiliary", 640, 400 } };
		return SIRNativeWindowHost::runNative( windows, 2 );
	}
	return SIRNativeWindowHost::runInput( bInputMulti, nFrames );
}
