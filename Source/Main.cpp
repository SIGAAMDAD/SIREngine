#include <Core/SIRApplication.h>

#include <stdio.h>
#include <string.h>

int main( int nArgs, char **pszArgs )
{
	if ( nArgs > 1 ) {
		if ( strcmp( pszArgs[1], "--version" ) == 0 ) {
			puts( "SIREngine native foundation " SIR_VERSION );
			return 0;
		}

		if ( strcmp( pszArgs[1], "--help" ) == 0 ) {
			puts( "Usage: SIREngine [--version|--help]" );
			return 0;
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
