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

#include <cstdint>
#include <fstream>
#include <string>

int main( int nArgs, const char *const *pszArgs )
{
	std::string sProfile;
	std::string sOutput;
	std::string sInput;
	bool bCorrupt = false;
	bool bWrongVersion = false;
	bool bFailure = false;
	for ( int iArg = 1; iArg < nArgs; ++iArg ) {
		const std::string sArgument = pszArgs[iArg];
		if ( ( sArgument == "/T" || sArgument == "/Fo" || sArgument == "/E" || sArgument == "/D" ) && iArg + 1 < nArgs ) {
			const std::string sValue = pszArgs[++iArg];
			if ( sArgument == "/T" ) {
				sProfile = sValue;
			} else if ( sArgument == "/Fo" ) {
				sOutput = sValue;
			} else if ( sArgument == "/E" && sValue != "main" ) {
				return 2;
			} else if ( sArgument == "/D" ) {
				bCorrupt |= sValue == "CORRUPT=1";
				bWrongVersion |= sValue == "WRONG_VERSION=1";
				bFailure |= sValue == "FAIL=1";
			}
		} else if ( sArgument == "/nologo" || sArgument == "/O3" ) {
			continue;
		} else if ( iArg == nArgs - 1 ) {
			sInput = sArgument;
		} else {
			return 2;
		}
	}
	if ( sOutput.empty() || sInput.empty() || sProfile.size() != 6 ||
	     ( sProfile.substr( 0, 3 ) != "vs_" && sProfile.substr( 0, 3 ) != "ps_" ) ||
	     ( sProfile != "vs_2_0" && sProfile != "vs_3_0" && sProfile != "ps_2_0" && sProfile != "ps_3_0" ) ) {
		return 2;
	}
	std::ifstream input( sInput, std::ios::binary );
	if ( !input || input.peek() == EOF || bFailure ) {
		return 9;
	}
	uint32_t nVersion = ( sProfile[0] == 'v' ? 0xfffe0000U : 0xffff0000U ) |
	                    ( ( uint32_t )( bWrongVersion ? ( sProfile[3] == '2' ? 3 : 2 ) : sProfile[3] - '0' ) << 8 );
	if ( bCorrupt ) {
		nVersion = 0;
	}
	const uint32_t nEnd = 0x0000ffffU;
	std::ofstream output( sOutput, std::ios::binary );
	output.write( reinterpret_cast<const char *>( &nVersion ), sizeof( nVersion ) );
	output.write( reinterpret_cast<const char *>( &nEnd ), sizeof( nEnd ) );
	return output ? 0 : 5;
}
