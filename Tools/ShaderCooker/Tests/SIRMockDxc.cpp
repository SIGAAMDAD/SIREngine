/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <cstring>
#include <fstream>
#include <string>

int main( int nArgs, const char *const *pszArgs )
{
	std::string sOutput;
	std::string sInput;
	std::string sProfile;
	bool bSpirv = false;
	bool bCompilerFlags = false;
	bool bForceCorrupt = false;
	bool bForceFailure = false;
	bool bNoOutput = false;
	bool bShortOutput = false;
	bool bUnaligned = false;
	for ( int iArg = 1; iArg < nArgs; ++iArg ) {
		const std::string sArg = pszArgs[iArg];
		if ( sArg == "-Fo" && iArg + 1 < nArgs ) {
			sOutput = pszArgs[++iArg];
		} else if ( sArg == "-T" && iArg + 1 < nArgs ) {
			sProfile = pszArgs[++iArg];
		} else if ( sArg == "-spirv" ) {
			bSpirv = true;
		} else if ( sArg == "-HV" && iArg + 1 < nArgs ) {
			bCompilerFlags = std::strcmp( pszArgs[++iArg], "2021" ) == 0;
		} else if ( sArg == "-fspv-target-env=vulkan1.3" || sArg == "-O3" || sArg == "-nologo" ) {
			continue;
		} else if ( sArg == "-E" && iArg + 1 < nArgs ) {
			if ( std::strcmp( pszArgs[++iArg], "main" ) != 0 ) {
				return 4;
			}
		} else if ( sArg == "-D" && iArg + 1 < nArgs ) {
			const std::string sDefinition = pszArgs[++iArg];
			bForceCorrupt |= sDefinition == "CORRUPT=1";
			bForceFailure |= sDefinition == "FAIL=1";
			bNoOutput |= sDefinition == "NO_OUTPUT=1";
			bShortOutput |= sDefinition == "SHORT=1";
			bUnaligned |= sDefinition == "UNALIGNED=1";
		} else if ( iArg == nArgs - 1 ) {
			sInput = sArg;
		}
	}
	if ( sOutput.empty() || sInput.empty() || sProfile != "cs_6_6" || !bCompilerFlags ) {
		return 2;
	}
	std::ifstream input( sInput, std::ios::binary );
	if ( !input || input.peek() == EOF ) {
		return 3;
	}
	if ( bForceFailure ) {
		return 9;
	}
	if ( bNoOutput ) {
		return 0;
	}
	std::ofstream output( sOutput, std::ios::binary );
	const char szSpirvMagic[4] = { 3, 2, 0x23, 7 };
	const char *pszMagic = bForceCorrupt ? "BAD!" : bSpirv ? szSpirvMagic : "DXBC";
	output.write( pszMagic, 4 );
	const char szPadding[28] = {};
	if ( !bShortOutput ) {
		output.write( szPadding, sizeof( szPadding ) );
	}
	if ( bUnaligned ) {
		output.put( 0 );
	}
	return output ? 0 : 5;
}
