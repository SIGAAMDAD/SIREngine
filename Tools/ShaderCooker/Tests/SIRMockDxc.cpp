/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
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
	std::string sReflection;
	std::string sStage;
	bool bSpirv = false;
	bool bSlang = false;
	bool bCompilerFlags = false;
	bool bFxc = false;
	bool bForceCorrupt = false;
	bool bForceFailure = false;
	bool bNoOutput = false;
	bool bShortOutput = false;
	bool bUnaligned = false;
	for ( int iArg = 1; iArg < nArgs; ++iArg ) {
		const std::string sArg = pszArgs[iArg];
		if ( ( sArg == "-Fo" || sArg == "/Fo" || sArg == "-o" ) && iArg + 1 < nArgs ) {
			sOutput = pszArgs[++iArg];
			bSlang |= sArg == "-o";
		} else if ( ( sArg == "-T" || sArg == "/T" ) && iArg + 1 < nArgs ) {
			sProfile = pszArgs[++iArg];
		} else if ( sArg == "-profile" && iArg + 1 < nArgs ) {
			bSlang = true;
			sProfile = pszArgs[++iArg];
		} else if ( sArg == "-target" && iArg + 1 < nArgs ) {
			bSpirv = std::strcmp( pszArgs[++iArg], "spirv" ) == 0;
		} else if ( sArg == "-stage" && iArg + 1 < nArgs ) {
			sStage = pszArgs[++iArg];
		} else if ( sArg == "-reflection-json" && iArg + 1 < nArgs ) {
			sReflection = pszArgs[++iArg];
		} else if ( sArg == "-spirv" ) {
			bSpirv = true;
		} else if ( sArg == "-HV" && iArg + 1 < nArgs ) {
			bCompilerFlags = std::strcmp( pszArgs[++iArg], "2021" ) == 0;
		} else if ( sArg == "-fspv-target-env=vulkan1.3" || sArg == "-O3" || sArg == "-nologo" || sArg == "/O3" || sArg == "/nologo" ) {
			if ( sArg == "-O3" ) {
				bCompilerFlags = true;
			}
			if ( sArg == "/O3" || sArg == "/nologo" ) {
				bFxc = true;
				bCompilerFlags = true;
			}
			continue;
		} else if ( ( sArg == "-E" || sArg == "/E" || sArg == "-entry" ) && iArg + 1 < nArgs ) {
			if ( std::strcmp( pszArgs[++iArg], "main" ) != 0 ) {
				return 4;
			}
		} else if ( ( sArg == "-D" || sArg == "/D" ) && iArg + 1 < nArgs ) {
			const std::string sDefinition = pszArgs[++iArg];
			bForceCorrupt |= sDefinition == "CORRUPT=1";
			bForceFailure |= sDefinition == "FAIL=1";
			bNoOutput |= sDefinition == "NO_OUTPUT=1";
			bShortOutput |= sDefinition == "SHORT=1";
			bUnaligned |= sDefinition == "UNALIGNED=1";
		} else if ( sArg.rfind( "-D", 0 ) == 0 && sArg.size() > 2 ) {
			const std::string sDefinition = sArg.substr( 2 );
			bForceCorrupt |= sDefinition == "CORRUPT=1";
			bForceFailure |= sDefinition == "FAIL=1";
			bNoOutput |= sDefinition == "NO_OUTPUT=1";
			bShortOutput |= sDefinition == "SHORT=1";
			bUnaligned |= sDefinition == "UNALIGNED=1";
		} else if ( iArg == nArgs - 1 ) {
			sInput = sArg;
		}
	}
	const bool bLegacy = sProfile == "vs_3_0" || sProfile == "ps_3_0" || sProfile == "vs_1_1" || sProfile == "ps_1_1";
	const bool bFxcProfile = bLegacy || sProfile == "vs_5_0" || sProfile == "ps_5_0" || sProfile == "cs_5_0" || sProfile == "gs_5_0" ||
	                         sProfile == "hs_5_0" || sProfile == "ds_5_0" || sProfile == "vs_4_0" || sProfile == "ps_4_0" ||
	                         sProfile == "gs_4_0";
	if ( sOutput.empty() || sInput.empty() || !bCompilerFlags ||
	     ( bSlang ? !bSpirv || sProfile != "sm_6_6" || sStage != "compute" || sReflection.empty()
	              : bFxc ? !bFxcProfile || bSpirv : sProfile != "cs_6_6" ) ) {
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
	if ( bSlang ) {
		std::ofstream reflection( sReflection, std::ios::binary );
		reflection << "{\"entryPoints\":[{\"name\":\"main\"}]}\n";
		if ( !reflection ) {
			return 5;
		}
	}
	std::ofstream output( sOutput, std::ios::binary );
	const char szSpirvMagic[4] = { 3, 2, 0x23, 7 };
	const char *pszMagic = bForceCorrupt ? "BAD!" : bSpirv ? szSpirvMagic : "DXBC";
	if ( bLegacy && !bForceCorrupt ) {
		// Shader model token: 0xfffeMMmm (vertex) or 0xffffMMmm (pixel).
		unsigned char szToken[4] = { ( unsigned char )( sProfile.back() - '0' ), ( unsigned char )( sProfile[sProfile.size() - 3] - '0' ),
		                             ( unsigned char )( sProfile[0] == 'v' ? 0xfe : 0xff ), 0xff };
		output.write( ( const char * )szToken, 4 );
		if ( !bShortOutput ) {
			const unsigned char szEnd[4] = { 0xff, 0xff, 0, 0 };
			output.write( ( const char * )szEnd, 4 );
		}
	} else {
		output.write( pszMagic, 4 );
		const char szPadding[28] = {};
		if ( !bShortOutput ) {
			output.write( szPadding, sizeof( szPadding ) );
		}
	}
	if ( bUnaligned ) {
		output.put( 0 );
	}
	return output ? 0 : 5;
}
