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
#include <Tools/StringCatalogCooker/SIRStringCatalogCooker.h>
#include <algorithm>
#include <limits.h>
#include <string.h>

static uint64_t sirCookAlign8( uint64_t nValue )
{
	return ( nValue + 7 ) & ~7ULL;
}

static void sirCookWrite32( uint8_t *pBytes, uint32_t nValue )
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static void sirCookWrite64( uint8_t *pBytes, uint64_t nValue )
{
	sirCookWrite32( pBytes, ( uint32_t )nValue );
	sirCookWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirCookHash( const uint8_t *pBytes, size_t nBytes, uint64_t nSeed )
{
	uint64_t nHash = nSeed;
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * SIR_REGISTRATION_HASH_PRIME;
	}
	return nHash;
}

static uint32_t sirCookStableId( const std::string& sName )
{
	const uint64_t nHash = sirCookHash( ( const uint8_t * )sName.data(), sName.size(), SIR_REGISTRATION_HASH_BASIS );
	return ( uint32_t )( nHash ^ ( nHash >> 32 ) );
}

static bool sirCookModuleNameValid( const sirRegistrationCookDefinition_t& definition )
{
	if ( definition.nOwnerId == 0 ) {
		return true;
	}
	const size_t nDot = definition.sName.find( '.' );
	if ( nDot == std::string::npos ) {
		return false;
	}
	if ( definition.sName.compare( 0, nDot, "sir" ) == 0 || definition.sName.compare( 0, nDot, "console" ) == 0 ) {
		return false;
	}
	return true;
}

static bool sirCookValidUtf8( const std::string& sText )
{
	const uint8_t *pBytes = ( const uint8_t * )sText.data();
	for ( size_t iByte = 0; iByte < sText.size(); ) {
		const uint8_t nFirst = pBytes[iByte];
		if ( nFirst < 0x80 ) {
			++iByte;
			continue;
		}
		uint32_t nCodePoint = 0;
		uint32_t nParts = 0;
		if ( nFirst >= 0xc2 && nFirst <= 0xdf ) {
			nCodePoint = nFirst & 0x1f;
			nParts = 1;
		} else if ( nFirst >= 0xe0 && nFirst <= 0xef ) {
			nCodePoint = nFirst & 0x0f;
			nParts = 2;
		} else if ( nFirst >= 0xf0 && nFirst <= 0xf4 ) {
			nCodePoint = nFirst & 7;
			nParts = 3;
		} else {
			return false;
		}
		if ( nParts > sText.size() - iByte - 1 ) {
			return false;
		}
		for ( uint32_t iPart = 0; iPart < nParts; ++iPart ) {
			const uint8_t nPart = pBytes[iByte + iPart + 1];
			if ( ( nPart & 0xc0 ) != 0x80 ) {
				return false;
			}
			nCodePoint = ( nCodePoint << 6 ) | ( nPart & 0x3f );
		}
		if ( ( nParts == 2 && nCodePoint < 0x800 ) || ( nParts == 3 && nCodePoint < 0x10000 ) ) {
			return false;
		}
		if ( ( nCodePoint >= 0xd800 && nCodePoint <= 0xdfff ) || nCodePoint > 0x10ffff ) {
			return false;
		}
		iByte += nParts + 1;
	}
	return true;
}

static bool sirCookDefaultValid( const sirRegistrationCookDefinition_t& definition )
{
	if ( definition.eType >= SIR_CVAR_TYPE_COUNT || !sirValidateCVarFlags( definition.nFlags ) ) {
		return false;
	}
	if ( definition.eType == SIR_CVAR_UTF8_STRING ) {
		return definition.sDefaultString.size() <= 4096 && sirCookValidUtf8( definition.sDefaultString );
	}
	if ( !definition.sDefaultString.empty() ) {
		return false;
	}
	if ( definition.eType == SIR_CVAR_BOOL ) {
		return definition.nDefaultBits <= 1;
	}
	if ( definition.eType == SIR_CVAR_I32 || definition.eType == SIR_CVAR_U32 || definition.eType == SIR_CVAR_F32 ||
	     definition.eType == SIR_CVAR_ENUM_U32 ) {
		return ( definition.nDefaultBits >> 32 ) == 0;
	}
	return true;
}

static bool sirCookInputValid( sirRegistrationCookDefinition_t& definition )
{
	char szNormalized[SIR_REGISTRATION_MAX_NAME_BYTES + 1] = {};
	if ( !sirNormalizeConsoleName( definition.sName.data(), definition.sName.size(), szNormalized, sizeof( szNormalized ) ) ) {
		return false;
	}
	definition.sName = szNormalized;
	if ( !sirCookModuleNameValid( definition ) ) {
		return false;
	}
	if ( definition.eKind == SIR_REGISTRATION_COMMAND ) {
		return definition.nBindingId != 0 && definition.nFlags == 0 && ( definition.nCapabilities & ~511u ) == 0;
	}
	if ( definition.eKind == SIR_REGISTRATION_CVAR ) {
		return definition.nSchemaId != 0 && sirCookDefaultValid( definition );
	}
	return false;
}

static bool sirCookLessName( const sirRegistrationCookDefinition_t& a, const sirRegistrationCookDefinition_t& b )
{
	return a.sName < b.sName;
}

static void sirCookWriteSection( uint8_t *pHeader, uint64_t nOffset, uint32_t nCount, uint32_t nStride )
{
	sirCookWrite64( pHeader, nCount != 0 ? nOffset : 0 );
	sirCookWrite32( pHeader + 8, nCount );
	sirCookWrite32( pHeader + 12, nCount != 0 ? nStride : 0 );
}

bool sirCookRegistrationCatalog( const std::vector<sirRegistrationCookDefinition_t>& definitions, std::vector<uint8_t>& strings,
                                 std::vector<uint8_t>& registrations )
{
	if ( definitions.size() > SIR_REGISTRATION_MAX_RECORDS ) {
		return false;
	}
	std::vector<sirRegistrationCookDefinition_t> sorted = definitions;
	for ( sirRegistrationCookDefinition_t& definition : sorted ) {
		if ( !sirCookInputValid( definition ) ) {
			return false;
		}
	}
	std::sort( sorted.begin(), sorted.end(), sirCookLessName );
	std::vector<std::string> names;
	names.reserve( sorted.size() );
	uint32_t nCommands = 0;
	uint32_t nCVars = 0;
	std::vector<uint64_t> bindingIds;
	std::vector<uint32_t> schemaIds;
	std::vector<uint32_t> nameIds;
	for ( const sirRegistrationCookDefinition_t& definition : sorted ) {
		if ( !names.empty() && names.back() == definition.sName ) {
			return false;
		}
		names.push_back( definition.sName );
		const uint32_t nStableNameId = sirCookStableId( definition.sName );
		if ( nStableNameId == 0 ) {
			return false;
		}
		nameIds.push_back( nStableNameId );
		if ( definition.eKind == SIR_REGISTRATION_COMMAND ) {
			++nCommands;
			bindingIds.push_back( definition.nBindingId );
		} else {
			++nCVars;
			schemaIds.push_back( definition.nSchemaId );
		}
	}
	std::sort( bindingIds.begin(), bindingIds.end() );
	std::sort( schemaIds.begin(), schemaIds.end() );
	std::sort( nameIds.begin(), nameIds.end() );
	if ( std::adjacent_find( bindingIds.begin(), bindingIds.end() ) != bindingIds.end() ) {
		return false;
	}
	if ( std::adjacent_find( schemaIds.begin(), schemaIds.end() ) != schemaIds.end() ) {
		return false;
	}
	if ( std::adjacent_find( nameIds.begin(), nameIds.end() ) != nameIds.end() ) {
		return false;
	}
	std::vector<uint8_t> cookedStrings;
	uint64_t nStringHash = 0;
	if ( !sirCookStringCatalog( names, cookedStrings, &nStringHash ) ) {
		return false;
	}

	const uint64_t nNamesOffset = names.empty() ? 0 : SIR_REGISTRATION_CATALOG_HEADER_BYTES;
	const uint64_t nCommandsOffset = nCommands ? sirCookAlign8( SIR_REGISTRATION_CATALOG_HEADER_BYTES + names.size() * 8 ) : 0;
	const uint64_t nCVarsOffset = nCVars ? sirCookAlign8( nCommands ? nCommandsOffset + ( uint64_t )nCommands * 32
	                                                                : SIR_REGISTRATION_CATALOG_HEADER_BYTES + names.size() * 8 )
	                                     : 0;
	uint64_t nEnd = SIR_REGISTRATION_CATALOG_HEADER_BYTES + names.size() * 8;
	if ( nCommands ) {
		nEnd = nCommandsOffset + ( uint64_t )nCommands * 32;
	}
	if ( nCVars ) {
		nEnd = nCVarsOffset + ( uint64_t )nCVars * 32;
	}
	uint64_t nBytes = sirCookAlign8( nEnd );
	for ( const sirRegistrationCookDefinition_t& definition : sorted ) {
		if ( definition.eKind == SIR_REGISTRATION_CVAR ) {
			nBytes += sirCookAlign8( definition.eType == SIR_CVAR_UTF8_STRING ? 4 + definition.sDefaultString.size() : 8 );
		}
	}
	if ( nBytes > UINT32_MAX ) {
		return false;
	}
	std::vector<uint8_t> result( ( size_t )nBytes, 0 );
	memcpy( result.data(), "SIRC", 4 );
	result[4] = ( uint8_t )SIR_REGISTRATION_CATALOG_VERSION;
	result[6] = ( uint8_t )SIR_REGISTRATION_CATALOG_HEADER_BYTES;
	sirCookWrite32( result.data() + 8, SIR_REGISTRATION_CATALOG_BYTE_ORDER );
	sirCookWrite32( result.data() + 12, 1 );
	sirCookWrite64( result.data() + 16, nBytes );
	sirCookWriteSection( result.data() + 24, nNamesOffset, ( uint32_t )names.size(), SIR_REGISTRATION_NAME_STRIDE );
	sirCookWriteSection( result.data() + 40, nCommandsOffset, nCommands, SIR_REGISTRATION_COMMAND_STRIDE );
	sirCookWriteSection( result.data() + 56, nCVarsOffset, nCVars, SIR_REGISTRATION_CVAR_STRIDE );
	sirCookWrite64( result.data() + 88, nStringHash );

	uint32_t iCommand = 0;
	uint32_t iCVar = 0;
	uint32_t iName = 0;
	uint64_t nNextDefault = sirCookAlign8( nEnd );
	for ( const sirRegistrationCookDefinition_t& definition : sorted ) {
		uint8_t *pNameRecord = result.data() + nNamesOffset + ( size_t )iName * 8;
		sirCookWrite32( pNameRecord, iName );
		sirCookWrite32( pNameRecord + 4, sirCookStableId( definition.sName ) );
		if ( definition.eKind == SIR_REGISTRATION_COMMAND ) {
			uint8_t *pRecord = result.data() + nCommandsOffset + ( size_t )iCommand * 32;
			sirCookWrite32( pRecord, iName );
			sirCookWrite32( pRecord + 4, UINT32_MAX );
			sirCookWrite64( pRecord + 8, definition.nBindingId );
			sirCookWrite32( pRecord + 20, definition.nCapabilities );
			sirCookWrite32( pRecord + 24, definition.nOwnerId );
			sirCookWrite32( pRecord + 28, definition.nSchemaId );
			++iCommand;
		} else {
			uint8_t *pRecord = result.data() + nCVarsOffset + ( size_t )iCVar * 32;
			sirCookWrite32( pRecord, iName );
			sirCookWrite32( pRecord + 4, UINT32_MAX );
			sirCookWrite32( pRecord + 8, definition.nSchemaId );
			sirCookWrite32( pRecord + 12, ( uint32_t )definition.eType );
			sirCookWrite32( pRecord + 16, definition.nFlags );
			sirCookWrite32( pRecord + 20, ( uint32_t )nNextDefault );
			sirCookWrite32( pRecord + 24, definition.nOwnerId );
			if ( definition.eType == SIR_CVAR_UTF8_STRING ) {
				sirCookWrite32( result.data() + nNextDefault, ( uint32_t )definition.sDefaultString.size() );
				memcpy( result.data() + nNextDefault + 4, definition.sDefaultString.data(), definition.sDefaultString.size() );
				nNextDefault += sirCookAlign8( 4 + definition.sDefaultString.size() );
			} else {
				sirCookWrite64( result.data() + nNextDefault, definition.nDefaultBits );
				nNextDefault += 8;
			}
			++iCVar;
		}
		++iName;
	}
	uint64_t nDescriptorHash = SIR_REGISTRATION_HASH_BASIS;
	if ( nCommands ) {
		nDescriptorHash = sirCookHash( result.data() + nCommandsOffset, ( size_t )nCommands * 32, nDescriptorHash );
	}
	if ( nCVars ) {
		nDescriptorHash = sirCookHash( result.data() + nCVarsOffset, ( size_t )nCVars * 32, nDescriptorHash );
	}
	sirCookWrite64( result.data() + 96, nDescriptorHash );
	sirCookWrite64( result.data() + 104,
	                sirCookHash( result.data() + SIR_REGISTRATION_CATALOG_HEADER_BYTES,
	                             result.size() - SIR_REGISTRATION_CATALOG_HEADER_BYTES, SIR_REGISTRATION_HASH_BASIS ) );
	strings.swap( cookedStrings );
	registrations.swap( result );
	return true;
}
