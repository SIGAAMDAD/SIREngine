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

#include <Tools/RegistrationCooker/SIRRegistrationCooker.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

static bool sirParseCookInteger( const std::string& sValue, uint64_t *pValue )
{
	if ( sValue.empty() ) {
		return false;
	}
	char *pszEnd = nullptr;
	errno = 0;
	const unsigned long long nValue = strtoull( sValue.c_str(), &pszEnd, 0 );
	if ( errno != 0 || pszEnd == sValue.c_str() || *pszEnd != '\0' || sValue[0] == '-' ) {
		return false;
	}
	*pValue = ( uint64_t )nValue;
	return true;
}

static bool sirReadCookValue( std::istringstream& line, uint64_t *pValue )
{
	std::string sText;
	if ( !( line >> sText ) ) {
		return false;
	}
	return sirParseCookInteger( sText, pValue );
}

static bool sirParseCookLine( const std::string& sLine, sirRegistrationCookDefinition_t *pDefinition )
{
	std::istringstream line( sLine );
	std::string sKind;
	if ( !( line >> sKind >> pDefinition->sName ) ) {
		return false;
	}
	uint64_t nFirst = 0;
	uint64_t nSecond = 0;
	uint64_t nThird = 0;
	uint64_t nFourth = 0;
	if ( !sirReadCookValue( line, &nFirst ) || !sirReadCookValue( line, &nSecond ) || !sirReadCookValue( line, &nThird ) ||
	     !sirReadCookValue( line, &nFourth ) ) {
		return false;
	}
	if ( nSecond > UINT32_MAX || nThird > UINT32_MAX || nFourth > UINT32_MAX ) {
		return false;
	}
	pDefinition->nSchemaId = 0;
	pDefinition->nFlags = 0;
	pDefinition->nCapabilities = 0;
	pDefinition->nDefaultBits = 0;
	pDefinition->nBindingId = 0;
	pDefinition->nOwnerId = 0;
	pDefinition->eType = SIR_CVAR_BOOL;
	if ( sKind == "command" ) {
		pDefinition->eKind = SIR_REGISTRATION_COMMAND;
		pDefinition->nBindingId = nFirst;
		pDefinition->nCapabilities = ( uint32_t )nSecond;
		pDefinition->nOwnerId = ( uint32_t )nThird;
		pDefinition->nSchemaId = ( uint32_t )nFourth;
		std::string sExtra;
		return !( line >> sExtra );
	}
	if ( sKind != "cvar" || nFirst > UINT32_MAX || nSecond >= SIR_CVAR_TYPE_COUNT ) {
		return false;
	}
	pDefinition->eKind = SIR_REGISTRATION_CVAR;
	pDefinition->nSchemaId = ( uint32_t )nFirst;
	pDefinition->eType = ( sirCVarType_t )nSecond;
	pDefinition->nFlags = ( uint32_t )nThird;
	pDefinition->nOwnerId = ( uint32_t )nFourth;
	if ( pDefinition->eType == SIR_CVAR_UTF8_STRING ) {
		std::getline( line >> std::ws, pDefinition->sDefaultString );
		return true;
	}
	return sirReadCookValue( line, &pDefinition->nDefaultBits ) && ( line >> std::ws ).eof();
}

static bool sirWriteCookFile( const std::string& sPath, const std::vector<uint8_t>& bytes )
{
	std::ofstream file( sPath, std::ios::binary | std::ios::trunc );
	if ( !file ) {
		return false;
	}
	file.write( ( const char * )bytes.data(), ( std::streamsize )bytes.size() );
	file.close();
	return ( bool )file;
}

int main( int argc, char **argv )
{
	if ( argc != 4 ) {
		std::cerr << "Usage: SIRRegistrationCooker <manifest> <strings.sirs> <registrations.sirc>\n";
		return 2;
	}
	std::ifstream input( argv[1] );
	if ( !input ) {
		std::cerr << "Unable to open manifest\n";
		return 2;
	}
	std::vector<sirRegistrationCookDefinition_t> definitions;
	std::string sLine;
	uint32_t nLine = 0;
	while ( std::getline( input, sLine ) ) {
		++nLine;
		const size_t nStart = sLine.find_first_not_of( " \t\r" );
		if ( nStart == std::string::npos || sLine[nStart] == '#' ) {
			continue;
		}
		sirRegistrationCookDefinition_t definition = {};
		if ( !sirParseCookLine( sLine, &definition ) ) {
			std::cerr << "Invalid manifest record at line " << nLine << '\n';
			return 2;
		}
		definitions.push_back( definition );
	}
	std::vector<uint8_t> strings;
	std::vector<uint8_t> registrations;
	if ( !sirCookRegistrationCatalog( definitions, strings, registrations ) ) {
		std::cerr << "Cook failed: invalid definitions, duplicates, or capacity\n";
		return 1;
	}
	if ( !sirWriteCookFile( argv[2], strings ) || !sirWriteCookFile( argv[3], registrations ) ) {
		std::cerr << "Unable to write cooked output\n";
		return 1;
	}
	return 0;
}
