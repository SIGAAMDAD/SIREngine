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

#include <Core/Registration/SIRRegistrationTypes.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <vector>

static void sirTestWrite16( uint8_t *pBytes, uint16_t nValue )
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirTestWrite32( uint8_t *pBytes, uint32_t nValue )
{
	sirTestWrite16( pBytes, ( uint16_t )nValue );
	sirTestWrite16( pBytes + 2, ( uint16_t )( nValue >> 16 ) );
}

static void sirTestWrite64( uint8_t *pBytes, uint64_t nValue )
{
	sirTestWrite32( pBytes, ( uint32_t )nValue );
	sirTestWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirTestHash( const uint8_t *pBytes, size_t nBytes )
{
	uint64_t nHash = SIR_REGISTRATION_HASH_BASIS;
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * SIR_REGISTRATION_HASH_PRIME;
	}
	return nHash;
}

static void sirTestRefresh( std::vector<uint8_t>& bytes )
{
	const uint64_t nDescriptorHash = sirTestHash( bytes.data() + 128, 32 );
	sirTestWrite64( bytes.data() + 96, nDescriptorHash );
	sirTestWrite64( bytes.data() + 104, sirTestHash( bytes.data() + 120, bytes.size() - 120 ) );
}

static std::vector<uint8_t> sirTestValidCatalog( void )
{
	std::vector<uint8_t> bytes( 160, 0 );
	sirTestWrite32( bytes.data(), SIR_REGISTRATION_CATALOG_MAGIC );
	sirTestWrite16( bytes.data() + 4, SIR_REGISTRATION_CATALOG_VERSION );
	sirTestWrite16( bytes.data() + 6, SIR_REGISTRATION_CATALOG_HEADER_BYTES );
	sirTestWrite32( bytes.data() + 8, SIR_REGISTRATION_CATALOG_BYTE_ORDER );
	sirTestWrite32( bytes.data() + 12, 1 );
	sirTestWrite64( bytes.data() + 16, bytes.size() );
	sirTestWrite64( bytes.data() + 24, 120 );
	sirTestWrite32( bytes.data() + 32, 1 );
	sirTestWrite32( bytes.data() + 36, 8 );
	sirTestWrite64( bytes.data() + 40, 128 );
	sirTestWrite32( bytes.data() + 48, 1 );
	sirTestWrite32( bytes.data() + 52, 32 );
	sirTestWrite64( bytes.data() + 88, 0x123456789abcdef0ULL );
	sirTestWrite32( bytes.data() + 120, 17 );
	sirTestWrite32( bytes.data() + 124, 981 );
	sirTestWrite32( bytes.data() + 128, 0 );
	sirTestWrite64( bytes.data() + 136, 0xbeef );
	sirTestRefresh( bytes );
	return bytes;
}

int main( void )
{
	static_assert( sizeof( sirCVarHandle_t ) == 8, "CVar handle is eight bytes" );
	static_assert( sizeof( sirCommandHandle_t ) == 8, "command handle is eight bytes" );
	assert( sirCVarSourcePriority( SIR_CVAR_SOURCE_ENGINE_DEFAULT ) == 0 );
	assert( sirCVarSourcePriority( SIR_CVAR_SOURCE_SERVER_AUTHORITY ) == 70 );
	assert( sirCVarSourcePriority( ( sirCVarSource_t )255 ) == 0 );
	assert( sirValidateCVarFlags( SIR_CVAR_FLAG_ARCHIVE | SIR_CVAR_FLAG_LATCHED ) );
	assert( !sirValidateCVarFlags( SIR_CVAR_FLAG_ARCHIVE | SIR_CVAR_FLAG_TRANSIENT ) );
	assert( !sirValidateCVarFlags( SIR_CVAR_FLAG_REPLICATED | SIR_CVAR_FLAG_SENSITIVE ) );
	assert( !sirValidateCVarFlags( 1u << 31 ) );
	char szName[128] = {};
	assert( sirValidateConsoleName( "R.Texture_1", 11 ) );
	assert( sirNormalizeConsoleName( "R.Texture_1", 11, szName, sizeof( szName ) ) );
	assert( strcmp( szName, "r.texture_1" ) == 0 );
	assert( !sirValidateConsoleName( "bad..key", 8 ) );
	assert( !sirValidateConsoleName( "1bad", 4 ) );
	assert( !sirValidateConsoleName( "bad-thing", 9 ) );
	assert( !sirValidateConsoleName( "x.", 2 ) );
	assert( !sirValidateConsoleName( "", 0 ) );
	assert( !sirNormalizeConsoleName( "foo", 3, szName, 3 ) );
	std::vector<uint8_t> bytes = sirTestValidCatalog();
	const uint64_t nHash = 0x123456789abcdef0ULL;
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_OK );
	assert( sirValidateRegistrationCatalog( nullptr, 160, nHash ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	assert( sirValidateRegistrationCatalog( bytes.data(), 12, nHash ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), 0 ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash + 1 ) == SIR_REGISTRATION_BAD_SCHEMA );
	bytes[0] = 0;
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_MAGIC );
	bytes = sirTestValidCatalog();
	bytes[4] = 2;
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_VERSION );
	bytes = sirTestValidCatalog();
	bytes[8] = 0;
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_BYTE_ORDER );
	bytes = sirTestValidCatalog();
	sirTestWrite64( bytes.data() + 16, 170 );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_BOUNDS );
	bytes = sirTestValidCatalog();
	sirTestWrite64( bytes.data() + 40, 122 );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_ALIGNMENT );
	bytes = sirTestValidCatalog();
	sirTestWrite64( bytes.data() + 40, 120 );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_OVERLAPPING_SECTIONS );
	bytes = sirTestValidCatalog();
	sirTestWrite32( bytes.data() + 48, 0xffffffffu );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_BOUNDS );
	bytes = sirTestValidCatalog();
	sirTestWrite32( bytes.data() + 52, 16 );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_SCHEMA );
	bytes = sirTestValidCatalog();
	bytes[129] ^= 1;
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_HASH );
	bytes = sirTestValidCatalog();
	sirTestWrite64( bytes.data() + 112, 99 );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_SCHEMA );
	bytes = sirTestValidCatalog();
	sirTestWrite64( bytes.data() + 96, 0 );
	assert( sirValidateRegistrationCatalog( bytes.data(), bytes.size(), nHash ) == SIR_REGISTRATION_BAD_HASH );
	return 0;
}
