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

#include <Core/Registration/SIRRegistrationStringBridge.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static uint32_t s_nAllocations = 0;
static uint32_t s_nDeallocations = 0;

static void *sirBridgeAllocate( void *, size_t nBytes, size_t )
{
	++s_nAllocations;
	return malloc( nBytes );
}

static void sirBridgeDeallocate( void *, void *pMemory, size_t, size_t )
{
	++s_nDeallocations;
	free( pMemory );
}

static void sirBridgeWrite16( uint8_t *pBytes, uint16_t nValue )
{
	pBytes[0] = ( uint8_t )nValue;
	pBytes[1] = ( uint8_t )( nValue >> 8 );
}

static void sirBridgeWrite32( uint8_t *pBytes, uint32_t nValue )
{
	sirBridgeWrite16( pBytes, ( uint16_t )nValue );
	sirBridgeWrite16( pBytes + 2, ( uint16_t )( nValue >> 16 ) );
}

static void sirBridgeWrite64( uint8_t *pBytes, uint64_t nValue )
{
	sirBridgeWrite32( pBytes, ( uint32_t )nValue );
	sirBridgeWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirBridgeHashBytes( const uint8_t *pBytes, size_t nBytes, uint64_t nSeed )
{
	uint64_t nHash = 14695981039346656037ULL;
	if ( nSeed != 0 ) {
		nHash = nSeed;
	}
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * 1099511628211ULL;
	}
	return nHash;
}

static std::vector<uint8_t> sirBridgeCookedCatalog( const char *pszName, uint64_t *pCatalogHash )
{
	const size_t nNameBytes = strlen( pszName );
	std::vector<uint8_t> bytes( 56 + nNameBytes + 1, 0 );
	memcpy( bytes.data(), "SIRS", 4 );
	sirBridgeWrite16( bytes.data() + 4, SIR_STRING_CATALOG_VERSION );
	sirBridgeWrite32( bytes.data() + 8, 1 );
	sirBridgeWrite32( bytes.data() + 12, 8 );
	sirBridgeWrite32( bytes.data() + 24, ( uint32_t )nNameBytes + 1 );
	sirBridgeWrite32( bytes.data() + 44, 48 );
	sirBridgeWrite16( bytes.data() + 52, ( uint16_t )nNameBytes );
	sirBridgeWrite16( bytes.data() + 54, ( uint16_t )( 0x8000u | nNameBytes ) );
	memcpy( bytes.data() + 56, pszName, nNameBytes );
	const uint64_t nBlobHash = sirBridgeHashBytes( bytes.data() + 48, bytes.size() - 48, 0 );
	sirBridgeWrite64( bytes.data() + 28, nBlobHash );
	uint64_t nCatalogHash = sirBridgeHashBytes( bytes.data() + 56, nNameBytes, 0 );
	const uint8_t nSeparator = 0xff;
	nCatalogHash = sirBridgeHashBytes( &nSeparator, 1, nCatalogHash );
	sirBridgeWrite64( bytes.data() + 36, nCatalogHash );
	*pCatalogHash = nCatalogHash;
	return bytes;
}

int main( void )
{
	SIRStringPool pool;
	SIRStringAtom atom( 999 );
	const sirRegistrationNameRequest_t request = { "R.SHADOWS", 9, 0 };
	assert( sirRegistrationInternName( pool, request, &atom ) == SIR_REGISTRATION_NOT_INITIALIZED );
	assert( !atom );

	sirContainerAllocator_t allocator = { nullptr, sirBridgeAllocate, sirBridgeDeallocate, 1, 17 };
	sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	config.nHashBuckets = 64;
	config.nMaxNames = 8;
	config.nDynamicTextBytes = 96;
	config.bAdaptiveBudgets = false;
	config.bTieredCache = false;
	config.nShortSlots = 2;
	config.nLongSlots = 1;
	config.nShortBytes = 128;
	config.nLongBytes = 1024;
	assert( pool.initialize( &allocator, config ) );
	const uint32_t nInitAllocations = s_nAllocations;
	assert( sirRegistrationInternName( pool, request, &atom ) == SIR_REGISTRATION_OK );
	assert( atom );
	SIRStringAtom found;
	assert( sirRegistrationFindName( pool, "r.shadows", 9, &found ) == SIR_REGISTRATION_OK );
	assert( found == atom );
	assert( sirRegistrationFindName( pool, "R.SHADOWS", 9, &found ) == SIR_REGISTRATION_OK );
	assert( found == atom );
	assert( sirRegistrationFindName( pool, "does.not.exist", 14, &found ) == SIR_REGISTRATION_NOT_FOUND );
	assert( !found );
	assert( sirRegistrationFindName( pool, "some^bad", 8, &found ) == SIR_REGISTRATION_INVALID_NAME );
	assert( sirRegistrationInternName( pool, { "console.clear", 13, 88 }, &found ) == SIR_REGISTRATION_DENIED );
	assert( sirRegistrationInternName( pool, { "sir.debug", 9, 88 }, &found ) == SIR_REGISTRATION_DENIED );
	assert( sirRegistrationInternName( pool, { "unqualified", 11, 88 }, &found ) == SIR_REGISTRATION_DENIED );
	assert( sirRegistrationInternName( pool, { "mod.test", 8, 88 }, &found ) == SIR_REGISTRATION_OK );
	assert( sirRegistrationInternName( pool, { "MOD.TEST", 8, 88 }, &found ) == SIR_REGISTRATION_OK );
	assert( sirRegistrationFindName( pool, "mod.test", 8, &found ) == SIR_REGISTRATION_OK );
	assert( sirRegistrationFindName( pool, "mod.test", 8, nullptr ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	assert( sirRegistrationInternName( pool, request, nullptr ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	assert( sirRegistrationCookedName( pool, 0, 0, &found ) == SIR_REGISTRATION_BAD_SCHEMA );
	assert( sirRegistrationCookedName( pool, 0, 1, &found ) == SIR_REGISTRATION_BAD_SCHEMA );
	assert( s_nAllocations == nInitAllocations );
	const SIRStringAtom stale = atom;
	assert( pool.shutdown() );
	assert( s_nDeallocations == s_nAllocations );

	// Cooked ordinal mapping is hash-qualified; malformed or noncanonical cooked names never enter registration.
	uint32_t nWritten = 0;
	char szName[128] = {};
	uint64_t nCatalogHash = 0;
	std::vector<uint8_t> cooked = sirBridgeCookedCatalog( "render.gamma", &nCatalogHash );
	const size_t nCookedRequired = SIRStringPool::requiredBytes( config, cooked.data(), cooked.size() );
	assert( nCookedRequired != 0 );
	std::vector<uint8_t> cookedMemory( nCookedRequired );
	assert( pool.initialize( { cookedMemory.data(), cookedMemory.size(), cooked.data(), cooked.size() }, config ) );
	assert( sirRegistrationCookedName( pool, 0, nCatalogHash + 1, &found ) == SIR_REGISTRATION_BAD_SCHEMA );
	assert( sirRegistrationCookedName( pool, 1, nCatalogHash, &found ) == SIR_REGISTRATION_BAD_SCHEMA );
	assert( sirRegistrationCookedName( pool, 0, nCatalogHash, &found ) == SIR_REGISTRATION_OK );
	assert( pool.decodeInto( found, szName, sizeof( szName ), nWritten ) == SIR_STRING_OK );
	assert( strcmp( szName, "render.gamma" ) == 0 );
	assert( pool.shutdown() );

	cooked = sirBridgeCookedCatalog( "Render.Gamma", &nCatalogHash );
	const size_t nUpperRequired = SIRStringPool::requiredBytes( config, cooked.data(), cooked.size() );
	assert( nUpperRequired != 0 );
	std::vector<uint8_t> upperMemory( nUpperRequired );
	assert( pool.initialize( { upperMemory.data(), upperMemory.size(), cooked.data(), cooked.size() }, config ) );
	assert( sirRegistrationCookedName( pool, 0, nCatalogHash, &found ) == SIR_REGISTRATION_INVALID_NAME );
	assert( pool.shutdown() );

	char szText[128] = {};
	assert( pool.decodeInto( stale, szText, sizeof( szText ), nWritten ) == SIR_STRING_NOT_INITIALIZED );
	return 0;
}
