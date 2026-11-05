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
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <algorithm>
#include <string.h>
#include <vector>

static uint32_t s_nCallbackCount = 0;

static void sirTestCallback( void *pContext, const void *, const void * ) noexcept
{
	uint32_t *pCount = ( uint32_t * )pContext;
	++*pCount;
}

static uint32_t sirTestU32( const uint8_t *pBytes )
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static uint64_t sirTestU64( const uint8_t *pBytes )
{
	return ( uint64_t )sirTestU32( pBytes ) | ( ( uint64_t )sirTestU32( pBytes + 4 ) << 32 );
}

static void sirTestWrite32( uint8_t *pBytes, uint32_t nValue )
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static void sirTestWrite64( uint8_t *pBytes, uint64_t nValue )
{
	sirTestWrite32( pBytes, ( uint32_t )nValue );
	sirTestWrite32( pBytes + 4, ( uint32_t )( nValue >> 32 ) );
}

static uint64_t sirTestHash( const uint8_t *pBytes, size_t nBytes, uint64_t nSeed )
{
	uint64_t nHash = nSeed;
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pBytes[iByte] ) * SIR_REGISTRATION_HASH_PRIME;
	}
	return nHash;
}

static void sirTestRefreshCatalog( std::vector<uint8_t>& bytes )
{
	uint64_t nDescriptorHash = SIR_REGISTRATION_HASH_BASIS;
	for ( uint32_t iGroup = 1; iGroup <= 2; ++iGroup ) {
		const uint8_t *pGroup = bytes.data() + 24 + iGroup * 16;
		const uint64_t nOffset = sirTestU64( pGroup );
		const uint32_t nCount = sirTestU32( pGroup + 8 );
		const uint32_t nStride = sirTestU32( pGroup + 12 );
		if ( nCount != 0 ) {
			nDescriptorHash = sirTestHash( bytes.data() + nOffset, ( size_t )nCount * nStride, nDescriptorHash );
		}
	}
	sirTestWrite64( bytes.data() + 96, nDescriptorHash );
	sirTestWrite64( bytes.data() + 104, sirTestHash( bytes.data() + SIR_REGISTRATION_CATALOG_HEADER_BYTES,
	                                                 bytes.size() - SIR_REGISTRATION_CATALOG_HEADER_BYTES, SIR_REGISTRATION_HASH_BASIS ) );
}

static sirRegistrationCookDefinition_t sirTestCommand( const char *pszName, uint64_t nBindingId, uint32_t nOwner )
{
	sirRegistrationCookDefinition_t value = {};
	value.sName = pszName;
	value.eKind = SIR_REGISTRATION_COMMAND;
	value.nBindingId = nBindingId;
	value.nOwnerId = nOwner;
	value.nCapabilities = SIR_CONSOLE_CAP_EXECUTE | SIR_CONSOLE_CAP_READ;
	return value;
}

static sirRegistrationCookDefinition_t sirTestCVar( const char *pszName, uint32_t nSchemaId, sirCVarType_t eType, uint64_t nDefaultBits )
{
	sirRegistrationCookDefinition_t value = {};
	value.sName = pszName;
	value.eKind = SIR_REGISTRATION_CVAR;
	value.nSchemaId = nSchemaId;
	value.eType = eType;
	value.nDefaultBits = nDefaultBits;
	value.nFlags = SIR_CVAR_FLAG_ARCHIVE;
	return value;
}

static void sirTestCookValidation( void )
{
	std::vector<sirRegistrationCookDefinition_t> definitions;
	definitions.push_back( sirTestCommand( "Help", 1001, 0 ) );
	definitions.push_back( sirTestCVar( "r.gamma", 51, SIR_CVAR_F32, 0x3f800000u ) );
	definitions.push_back( sirTestCommand( "mod.fly", 2001, 88 ) );
	definitions.push_back( sirTestCVar( "r.vsync", 52, SIR_CVAR_BOOL, 1 ) );
	std::vector<uint8_t> cookedStrings;
	std::vector<uint8_t> cookedRegistrations;
	assert( sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	const std::vector<uint8_t> expectedStrings = cookedStrings;
	const std::vector<uint8_t> expectedRegistrations = cookedRegistrations;
	std::reverse( definitions.begin(), definitions.end() );
	assert( sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	assert( cookedStrings == expectedStrings );
	assert( cookedRegistrations == expectedRegistrations );
	assert( sirValidateRegistrationCatalog( cookedRegistrations.data(), cookedRegistrations.size(),
	                                        sirTestU64( cookedStrings.data() + 36 ) ) == SIR_REGISTRATION_OK );

	const auto good = definitions;
	definitions.push_back( sirTestCommand( "HELP", 3001, 0 ) );
	assert( !sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	assert( cookedRegistrations == expectedRegistrations );
	definitions = good;
	for ( sirRegistrationCookDefinition_t& definition : definitions ) {
		if ( definition.eKind == SIR_REGISTRATION_COMMAND && definition.nOwnerId == 88 ) {
			definition.nBindingId = 1001;
		}
	}
	assert( !sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	definitions = good;
	definitions[0].sName = "console.bad";
	definitions[0].nOwnerId = 88;
	assert( !sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	definitions = good;
	definitions[0].sName = "bad..key";
	assert( !sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	definitions = good;
	for ( sirRegistrationCookDefinition_t& definition : definitions ) {
		if ( definition.eKind == SIR_REGISTRATION_CVAR ) {
			definition.nFlags = SIR_CVAR_FLAG_ARCHIVE | SIR_CVAR_FLAG_TRANSIENT;
			break;
		}
	}
	assert( !sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
}

static void sirTestRuntime( void )
{
	std::vector<sirRegistrationCookDefinition_t> definitions;
	definitions.push_back( sirTestCVar( "r.gamma", 51, SIR_CVAR_F32, 0x3f800000u ) );
	definitions.push_back( sirTestCommand( "Help", 1001, 0 ) );
	definitions.push_back( sirTestCommand( "mod.fly", 2001, 88 ) );
	definitions.push_back( sirTestCVar( "r.vsync", 52, SIR_CVAR_BOOL, 1 ) );
	std::vector<uint8_t> cookedStrings;
	std::vector<uint8_t> cookedRegistrations;
	assert( sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistrations ) );
	const uint64_t nStringHash = sirTestU64( cookedStrings.data() + 36 );

	sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	config.nHashBuckets = 64;
	config.nMaxNames = 16;
	config.nDynamicTextBytes = 512;
	config.bAdaptiveBudgets = false;
	config.bTieredCache = false;
	config.nShortSlots = 2;
	config.nLongSlots = 1;
	config.nShortBytes = 128;
	config.nLongBytes = 1024;
	const size_t nRequired = SIRStringPool::requiredBytes( config, cookedStrings.data(), cookedStrings.size() );
	assert( nRequired > 0 );
	std::vector<uint8_t> stringMemory( nRequired );
	SIRStringPool pool;
	assert( pool.initialize( { stringMemory.data(), stringMemory.size(), cookedStrings.data(), cookedStrings.size() }, config ) );

	sirRegistrationEntry_t szEntries[12] = {};
	SIRStringAtom szAtoms[8];
	uint32_t szNameIds[8] = {};
	uint64_t szBindingIds[8] = {};
	sirRegistrationBinding_t szBindings[2] = { { 1001, 0, sirTestCallback, &s_nCallbackCount },
	                                           { 2001, 88, sirTestCallback, &s_nCallbackCount } };
	sirRegistrationCatalogInit_t init = {};
	init.pBytes = cookedRegistrations.data();
	init.nBytes = cookedRegistrations.size();
	init.nStringCatalogHash = nStringHash;
	init.pEntries = szEntries;
	init.nEntryCapacity = 12;
	init.pNames = szAtoms;
	init.pNameIds = szNameIds;
	init.nNameCapacity = 8;
	init.pBindingIds = szBindingIds;
	init.nBindingCapacity = 8;
	init.pBindings = szBindings;
	init.nBindingCount = 2;
	SIRRegistrationCatalog registry;
	assert( registry.initialize( pool, init ) == SIR_REGISTRATION_OK );
	assert( registry.isInitialized() );
	assert( registry.getCookedCount() == 4 );
	assert( registry.getCount() == 4 );
	sirRegistrationHandle_t handle = {};
	assert( registry.find( "HELP", 4, &handle ) == SIR_REGISTRATION_OK );
	const sirRegistrationEntry_t *pEntry = nullptr;
	assert( registry.getEntry( handle, &pEntry ) == SIR_REGISTRATION_OK );
	assert( pEntry->bCooked && pEntry->eKind == SIR_REGISTRATION_COMMAND && pEntry->nBindingId == 1001 );
	assert( pEntry->pfnCallback != nullptr );
	pEntry->pfnCallback( pEntry->pContext, nullptr, nullptr );
	assert( s_nCallbackCount == 1 );
	assert( registry.unregisterDynamic( handle ) == SIR_REGISTRATION_DENIED );
	assert( registry.find( "r.gamma", 7, &handle ) == SIR_REGISTRATION_OK );
	assert( registry.getEntry( handle, &pEntry ) == SIR_REGISTRATION_OK );
	assert( pEntry->eKind == SIR_REGISTRATION_CVAR && pEntry->nSchemaId == 51 );
	assert( sirTestU64( cookedRegistrations.data() + pEntry->nDefaultOffset ) == 0x3f800000u );
	assert( registry.find( "not.registered", 14, &handle ) == SIR_REGISTRATION_NOT_FOUND );

	sirRegistrationDynamicDesc_t desc = {};
	desc.name = { "mod.speed", 9, 88 };
	desc.eKind = SIR_REGISTRATION_CVAR;
	desc.nSchemaId = 53;
	desc.eType = SIR_CVAR_U32;
	assert( registry.registerDynamic( desc, &handle ) == SIR_REGISTRATION_OK );
	const sirRegistrationHandle_t oldHandle = handle;
	assert( registry.getCount() == 5 );
	assert( registry.registerDynamic( desc, &handle ) == SIR_REGISTRATION_DUPLICATE_NAME );
	assert( registry.find( "MOD.SPEED", 9, &handle ) == SIR_REGISTRATION_OK );
	assert( handle.nSlot == oldHandle.nSlot && handle.nGeneration == oldHandle.nGeneration );
	assert( registry.unregisterDynamic( oldHandle ) == SIR_REGISTRATION_OK );
	assert( registry.getEntry( oldHandle, &pEntry ) == SIR_REGISTRATION_STALE_HANDLE );
	assert( registry.find( "mod.speed", 9, &handle ) == SIR_REGISTRATION_NOT_FOUND );
	desc.name = { "mod.accel", 9, 88 };
	desc.nSchemaId = 54;
	assert( registry.registerDynamic( desc, &handle ) == SIR_REGISTRATION_OK );
	assert( handle.nSlot == oldHandle.nSlot && handle.nGeneration != oldHandle.nGeneration );
	assert( registry.getEntry( oldHandle, &pEntry ) == SIR_REGISTRATION_STALE_HANDLE );
	desc.name = { "sir.debug", 9, 88 };
	desc.nSchemaId = 55;
	assert( registry.registerDynamic( desc, &handle ) == SIR_REGISTRATION_DENIED );
	assert( registry.unregisterModule( 88 ) == 2 );
	assert( registry.getCount() == 3 );
	assert( registry.find( "mod.fly", 7, &handle ) == SIR_REGISTRATION_NOT_FOUND );
	assert( registry.unregisterModule( 88 ) == 0 );
	registry.shutdown();
	assert( !registry.isInitialized() );
	assert( registry.getEntry( oldHandle, &pEntry ) == SIR_REGISTRATION_NOT_INITIALIZED );

	// Rejected schema mutations preserve all registry publication state.
	std::vector<uint8_t> damaged = cookedRegistrations;
	const uint64_t nCVarOffset = sirTestU64( damaged.data() + 56 );
	sirTestWrite32( damaged.data() + nCVarOffset + 20, 0 );
	sirTestRefreshCatalog( damaged );
	init.pBytes = damaged.data();
	assert( registry.initialize( pool, init ) == SIR_REGISTRATION_BAD_SCHEMA );
	assert( !registry.isInitialized() );
	damaged = cookedRegistrations;
	const uint64_t nCommandsOffset = sirTestU64( damaged.data() + 40 );
	sirTestWrite32( damaged.data() + nCommandsOffset + 32, sirTestU32( damaged.data() + nCommandsOffset ) );
	sirTestRefreshCatalog( damaged );
	init.pBytes = damaged.data();
	assert( registry.initialize( pool, init ) == SIR_REGISTRATION_DUPLICATE_NAME );
	assert( !registry.isInitialized() );
	damaged = cookedRegistrations;
	const uint64_t nNamesOffset = sirTestU64( damaged.data() + 24 );
	sirTestWrite32( damaged.data() + nNamesOffset, 777 );
	sirTestRefreshCatalog( damaged );
	init.pBytes = damaged.data();
	assert( registry.initialize( pool, init ) == SIR_REGISTRATION_BAD_SCHEMA );
	assert( !registry.isInitialized() );
	init.pBytes = cookedRegistrations.data();
	init.nBindingCount = 1;
	assert( registry.initialize( pool, init ) == SIR_REGISTRATION_UNBOUND_CALLBACK );
	assert( !registry.isInitialized() );
	init.nBindingCount = 2;
	assert( registry.initialize( pool, init ) == SIR_REGISTRATION_OK );
	registry.shutdown();
	assert( pool.shutdown() );
}

int main( void )
{
	sirTestCookValidation();
	sirTestRuntime();
	return 0;
}
