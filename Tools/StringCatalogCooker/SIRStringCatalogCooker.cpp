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

#include <Tools/StringCatalogCooker/SIRStringCatalogCooker.h>
#include <Core/Registration/SIRRegistrationTypes.h>
#include <limits.h>
#include <string.h>

static void sirStringCookWrite16( uint8_t *pBytes, uint16_t nValue )
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirStringCookWrite32( uint8_t *pBytes, uint32_t nValue )
{
	sirStringCookWrite16( pBytes, ( uint16_t )nValue );
	sirStringCookWrite16( pBytes + 2, ( uint16_t )( nValue >> 16 ) );
}

static void sirStringCookWrite64( uint8_t *pBytes, uint64_t nValue )
{
	sirStringCookWrite32( pBytes, ( uint32_t )nValue );
	sirStringCookWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirStringCookHash( const uint8_t *pBytes, size_t nBytes, uint64_t nSeed )
{
	uint64_t nHash = nSeed;
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * SIR_REGISTRATION_HASH_PRIME;
	}
	return nHash;
}

bool sirCookStringCatalog( const std::vector<std::string>& names, std::vector<uint8_t>& output, uint64_t *pContentHash )
{
	if ( pContentHash == nullptr || names.size() > SIR_REGISTRATION_MAX_RECORDS ) {
		return false;
	}
	size_t nTextBytes = 0;
	std::string sPrevious;
	for ( const std::string& sName : names ) {
		if ( !sirValidateConsoleName( sName.data(), sName.size() ) || ( !sPrevious.empty() && sName <= sPrevious ) ) {
			return false;
		}
		for ( char nCharacter : sName ) {
			if ( nCharacter >= 'A' && nCharacter <= 'Z' ) {
				return false;
			}
		}
		nTextBytes += sName.size() + 1;
		if ( nTextBytes > UINT32_MAX ) {
			return false;
		}
		sPrevious = sName;
	}
	const size_t nRecordBytes = names.size() * 8;
	std::vector<uint8_t> result( 48 + nRecordBytes + nTextBytes, 0 );
	memcpy( result.data(), "SIRS", 4 );
	sirStringCookWrite16( result.data() + 4, 1 );
	sirStringCookWrite32( result.data() + 8, ( uint32_t )names.size() );
	sirStringCookWrite32( result.data() + 12, ( uint32_t )nRecordBytes );
	sirStringCookWrite32( result.data() + 24, ( uint32_t )nTextBytes );
	sirStringCookWrite32( result.data() + 44, 48 );
	uint64_t nCorpus = SIR_REGISTRATION_HASH_BASIS;
	uint32_t nOffset = 0;
	for ( size_t iName = 0; iName < names.size(); ++iName ) {
		const std::string& sName = names[iName];
		uint8_t *pRecord = result.data() + 48 + iName * 8;
		sirStringCookWrite32( pRecord, nOffset );
		sirStringCookWrite16( pRecord + 4, ( uint16_t )sName.size() );
		sirStringCookWrite16( pRecord + 6, ( uint16_t )( 0x8000u | sName.size() ) );
		memcpy( result.data() + 48 + nRecordBytes + nOffset, sName.data(), sName.size() );
		nCorpus = sirStringCookHash( ( const uint8_t * )sName.data(), sName.size(), nCorpus );
		const uint8_t nSeparator = 0xff;
		nCorpus = sirStringCookHash( &nSeparator, 1, nCorpus );
		nOffset += ( uint32_t )sName.size() + 1;
	}
	sirStringCookWrite64( result.data() + 28, sirStringCookHash( result.data() + 48, result.size() - 48,
	                                                       SIR_REGISTRATION_HASH_BASIS ) );
	sirStringCookWrite64( result.data() + 36, nCorpus );
	output.swap( result );
	*pContentHash = nCorpus;
	return true;
}
