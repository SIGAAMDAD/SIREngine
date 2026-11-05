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

#include <Core/CVars/SIRCVarRegistry.h>
#include <Tools/RegistrationCooker/SIRRegistrationCooker.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <atomic>
#include <string.h>
#include <thread>
#include <vector>

static uint32_t s_nGroupCalls = 0;
static uint32_t s_nLastGroup = 0;
static uint64_t s_nLastRevision = 0;
static SIRCVarRegistry *s_pRegistry = nullptr;
static sirCVarWrite_t s_ReentrantWrite = {};

static void sirTestGroupSink( void *, uint32_t nGroup, uint64_t nRevision ) noexcept
{
	++s_nGroupCalls;
	s_nLastGroup = nGroup;
	s_nLastRevision = nRevision;
	uint64_t nToken = 0;
	assert( s_pRegistry->prepareBatch( &s_ReentrantWrite, 1, &nToken ) == SIR_REGISTRATION_DENIED );
	s_pRegistry->shutdown();
	assert( s_pRegistry->isInitialized() );
}

static uint64_t sirTestU64( const uint8_t *pBytes ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nValue |= ( uint64_t )pBytes[iByte] << ( iByte * 8 );
	}
	return nValue;
}

static sirRegistrationCookDefinition_t sirTestCVar( const char *pszName, sirCVarType_t eType, uint32_t nSchemaId, uint32_t nFlags )
{
	sirRegistrationCookDefinition_t definition = {};
	definition.sName = pszName;
	definition.eKind = SIR_REGISTRATION_CVAR;
	definition.eType = eType;
	definition.nSchemaId = nSchemaId;
	definition.nFlags = nFlags;
	definition.nDefaultBits = 0;
	return definition;
}

int main( void )
{
	std::vector<sirRegistrationCookDefinition_t> definitions;
	definitions.push_back( sirTestCVar( "r.value", SIR_CVAR_U32, 1, 0 ) );
	definitions.push_back( sirTestCVar( "r.wide", SIR_CVAR_I64, 2, 0 ) );
	definitions.push_back( sirTestCVar( "r.title", SIR_CVAR_UTF8_STRING, 3, SIR_CVAR_FLAG_LATCHED ) );
	definitions.push_back( sirTestCVar( "r.lock", SIR_CVAR_BOOL, 4, SIR_CVAR_FLAG_READ_ONLY ) );
	definitions.push_back( sirTestCVar( "r.net", SIR_CVAR_U32, 5, SIR_CVAR_FLAG_REPLICATED ) );
	definitions.push_back( sirTestCVar( "r.cheat", SIR_CVAR_BOOL, 6, SIR_CVAR_FLAG_CHEAT ) );
	definitions[0].nDefaultBits = 10;
	definitions[2].sDefaultString = "base";
	std::vector<uint8_t> cookedStrings;
	std::vector<uint8_t> cookedRegistration;
	assert( sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistration ) );
	const uint64_t nStringHash = sirTestU64( cookedStrings.data() + 36 );
	sirStringPoolConfig_t poolConfig = SIRStringPool::defaultConfig();
	poolConfig.nHashBuckets = 64;
	poolConfig.nMaxNames = 16;
	poolConfig.nDynamicTextBytes = 512;
	poolConfig.bAdaptiveBudgets = false;
	poolConfig.bTieredCache = false;
	poolConfig.nShortSlots = 2;
	poolConfig.nLongSlots = 1;
	poolConfig.nShortBytes = 128;
	poolConfig.nLongBytes = 1024;
	const size_t nPoolBytes = SIRStringPool::requiredBytes( poolConfig, cookedStrings.data(), cookedStrings.size() );
	assert( nPoolBytes != 0 );
	std::vector<uint8_t> poolMemory( nPoolBytes );
	SIRStringPool pool;
	assert( pool.initialize( { poolMemory.data(), poolMemory.size(), cookedStrings.data(), cookedStrings.size() }, poolConfig ) );
	sirRegistrationEntry_t szRegistrationEntries[10] = {};
	SIRStringAtom szRegistrationNames[10];
	uint32_t szRegistrationNameIds[10] = {};
	uint64_t szRegistrationBindingIds[10] = {};
	sirRegistrationCatalogInit_t catalogInit = {};
	catalogInit.pBytes = cookedRegistration.data();
	catalogInit.nBytes = cookedRegistration.size();
	catalogInit.nStringCatalogHash = nStringHash;
	catalogInit.pEntries = szRegistrationEntries;
	catalogInit.nEntryCapacity = 10;
	catalogInit.pNames = szRegistrationNames;
	catalogInit.pNameIds = szRegistrationNameIds;
	catalogInit.nNameCapacity = 10;
	catalogInit.pBindingIds = szRegistrationBindingIds;
	catalogInit.nBindingCapacity = 10;
	SIRRegistrationCatalog registration;
	assert( registration.initialize( pool, catalogInit ) == SIR_REGISTRATION_OK );
	sirCVarSlot_t szSlots[8] = {};
	uint32_t szBank32[8] = {};
	uint64_t szBank64[8] = {};
	sirCVarOverride_t szOverrides[12] = {};
	sirCVarOverride_t szShadow[12] = {};
	sirCVarEvent_t szEvents[4] = {};
	sirCVarEvent_t szScratchEvents[4] = {};
	sirCVarWrite_t szStage[8] = {};
	char szActive[12 * 64] = {};
	char szPending[12 * 64] = {};
	char szStageText[8 * 64] = {};
	char szEventText[4 * 2 * 64] = {};
	sirCVarStorage_t storage = { szSlots,
	                             8,
	                             szBank32,
	                             8,
	                             szBank64,
	                             8,
	                             szOverrides,
	                             szShadow,
	                             12,
	                             szEvents,
	                             szScratchEvents,
	                             4,
	                             szStage,
	                             8,
	                             szActive,
	                             szPending,
	                             szStageText,
	                             szEventText,
	                             64,
	                             sizeof( szActive ),
	                             sizeof( szPending ),
	                             sizeof( szStageText ),
	                             sizeof( szEventText ) };
	SIRCVarRegistry invalid;
	sirCVarStorage_t shortStorage = storage;
	shortStorage.nEventTextBytes = 1;
	assert( invalid.initialize( registration, shortStorage ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	SIRCVarRegistry registry;
	assert( registry.initialize( registration, storage ) == SIR_REGISTRATION_OK );
	sirCVarHandle_t handles[6] = {};
	const char *szKeys[6] = { "r.value", "r.wide", "r.title", "r.lock", "r.net", "r.cheat" };
	for ( uint32_t iSlot = 0; iSlot < 6; ++iSlot ) {
		sirRegistrationHandle_t registrationHandle = {};
		assert( registration.find( szKeys[iSlot], strlen( szKeys[iSlot] ), &registrationHandle ) == SIR_REGISTRATION_OK );
		sirCVarDefinition_t definition = {};
		sirCVarCookedSource_t cooked = { cookedRegistration.data(), cookedRegistration.size(), iSlot < 2 ? 7u : 9u,
		                                 SIR_CVAR_APPLY_NEXT_SAFE_POINT };
		assert( sirCVarDefinitionFromCooked( registration, registrationHandle, cooked, &definition ) == SIR_REGISTRATION_OK );
		assert( registry.bind( definition, &handles[iSlot] ) == SIR_REGISTRATION_OK );
	}
	assert( registry.getCount() == 6 );
	std::atomic<int> wrongThreadStatus{ 0 };
	std::thread other( [&registry, &handles, &wrongThreadStatus]() {
		sirCVarValue_t out = {};
		wrongThreadStatus.store( ( int )registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &out ) );
		registry.shutdown();
	} );
	other.join();
	assert( wrongThreadStatus.load() == SIR_REGISTRATION_WRONG_THREAD );
	assert( registry.isInitialized() );
	const sirCVarOrigin_t console = {
	    SIR_CVAR_SOURCE_SESSION_CONSOLE, 1, SIR_CONSOLE_CAP_WRITE | SIR_CONSOLE_CAP_READ, false, false, false, false };
	sirCVarValue_t got = {};
	assert( registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 10 );
	assert( registry.getValue( handles[0], 0, &got ) == SIR_REGISTRATION_DENIED );
	sirCVarWrite_t write = { handles[0], console, { 21, nullptr, 0 }, false };
	uint64_t token = 0;
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 10 );
	assert( registry.commitBatch( token + 1 ) == SIR_REGISTRATION_STALE_HANDLE );
	assert( registry.cancelBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getPendingEvents() == 0 );
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 21 );
	assert( registry.getRevision() == 1 && registry.getPendingEvents() == 1 );

	const sirCVarOrigin_t lower = { SIR_CVAR_SOURCE_USER_ARCHIVE, 2, SIR_CONSOLE_CAP_WRITE, false, false, false, false };
	write.origin = lower;
	write.value.nBits = 99;
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getPendingEvents() == 1 );
	assert( registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 21 );
	write.bRemove = true;
	write.origin = console;
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 99 );
	assert( registry.getPendingEvents() == 2 );

	write.bRemove = false;
	write.origin = console;
	write.handle = handles[2];
	char szPendingSource[] = "changed";
	write.value = { 0, szPendingSource, 7 };
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	memcpy( szPendingSource, "mutated", 7 );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( handles[2], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK );
	assert( got.nTextBytes == 4 && memcmp( got.pszText, "base", 4 ) == 0 );
	assert( registry.applyLatched( SIR_CVAR_APPLY_RESTART ) == SIR_REGISTRATION_NO_CHANGE );
	assert( registry.applyLatched( SIR_CVAR_APPLY_NEXT_SAFE_POINT ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( handles[2], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK );
	assert( got.nTextBytes == 7 && memcmp( got.pszText, "changed", 7 ) == 0 );
	assert( registry.getPendingEvents() == 3 );
	while ( registry.getPendingEvents() != 0 ) {
		assert( registry.popEvent() == SIR_REGISTRATION_OK );
	}

	write.handle = handles[3];
	write.value = { 1, nullptr, 0 };
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_DENIED );
	write.origin = { SIR_CVAR_SOURCE_ENGINE_DEFAULT, 0, SIR_CONSOLE_CAP_WRITE | SIR_CONSOLE_CAP_INIT, true, true, false, false };
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	write.origin = console;
	write.handle = handles[4];
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_DENIED );
	write.origin.nCapabilities |= SIR_CONSOLE_CAP_SERVER;
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	write.origin = console;
	write.handle = handles[5];
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_DENIED );
	write.origin.bCheatsAllowed = true;
	write.origin.nCapabilities |= SIR_CONSOLE_CAP_CHEAT;
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );

	assert( registry.getPendingEvents() == 3 );
	write.handle = handles[0];
	write.origin = console;
	write.value = { 80, nullptr, 0 };
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getPendingEvents() == 4 );
	write.value.nBits = 81;
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_QUEUE_FULL );
	sirCVarWrite_t lowPriority = write;
	lowPriority.origin = { SIR_CVAR_SOURCE_DEVICE_PROFILE, 60, SIR_CONSOLE_CAP_WRITE, false, false, false, false };
	lowPriority.value.nBits = 123;
	assert( registry.prepareBatch( &lowPriority, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getPendingEvents() == 4 );
	assert( registry.getValue( handles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 80 );
	const sirCVarEvent_t *pEvent = nullptr;
	assert( registry.peekEvent( &pEvent ) == SIR_REGISTRATION_OK );
	assert( registry.popEvent() == SIR_REGISTRATION_OK );
	assert( registry.getPendingEvents() == 3 );
	assert( registry.prepareBatch( &write, 1, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );

	s_pRegistry = &registry;
	s_ReentrantWrite = write;
	assert( registry.pumpGroups( sirTestGroupSink, nullptr ) == 4 );
	assert( s_nGroupCalls == 4 );
	assert( s_nLastGroup != 0 && s_nLastRevision != 0 );
	assert( registry.getPendingEvents() == 0 );
	s_nGroupCalls = 0;
	sirCVarWrite_t two[2] = { write, write };
	two[0].value.nBits = 83;
	two[1].handle = handles[1];
	two[1].value.nBits = 333;
	assert( registry.prepareBatch( two, 2, &token ) == SIR_REGISTRATION_OK );
	assert( registry.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( registry.getPendingEvents() == 2 );
	assert( registry.pumpGroups( sirTestGroupSink, nullptr ) == 2 );
	assert( s_nGroupCalls == 1 );
	sirRegistrationDynamicDesc_t dynamic = {};
	dynamic.name = { "mod.speed", 9, 88 };
	dynamic.eKind = SIR_REGISTRATION_CVAR;
	dynamic.eType = SIR_CVAR_U32;
	dynamic.nSchemaId = 88;
	sirRegistrationHandle_t dynamicRegistration = {};
	assert( registration.registerDynamic( dynamic, &dynamicRegistration ) == SIR_REGISTRATION_OK );
	sirCVarDefinition_t dynamicDefinition = {};
	dynamicDefinition.registration = dynamicRegistration;
	dynamicDefinition.defaultValue.nBits = 5;
	sirCVarHandle_t dynamicHandle = {};
	assert( registry.bind( dynamicDefinition, &dynamicHandle ) == SIR_REGISTRATION_OK );
	assert( registration.unregisterDynamic( dynamicRegistration ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( dynamicHandle, SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_STALE_HANDLE );
	assert( registry.unbind( dynamicHandle ) == SIR_REGISTRATION_OK );
	assert( registry.getValue( dynamicHandle, SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_STALE_HANDLE );
	registry.shutdown();
	assert( !registry.isInitialized() );
	// Text-only instances do not reserve a scalar bank.
	sirCVarStorage_t stringsOnly = storage;
	stringsOnly.pBank32 = nullptr;
	stringsOnly.nBank32 = 0;
	stringsOnly.pBank64 = nullptr;
	stringsOnly.nBank64 = 0;
	SIRCVarRegistry textRegistry;
	assert( textRegistry.initialize( registration, stringsOnly ) == SIR_REGISTRATION_OK );
	sirRegistrationHandle_t textRegistration = {};
	assert( registration.find( "r.title", 7, &textRegistration ) == SIR_REGISTRATION_OK );
	sirCVarDefinition_t textDefinition = {};
	sirCVarCookedSource_t cookedSource = { cookedRegistration.data(), cookedRegistration.size(), 9, SIR_CVAR_APPLY_NEXT_SAFE_POINT };
	assert( sirCVarDefinitionFromCooked( registration, textRegistration, cookedSource, &textDefinition ) == SIR_REGISTRATION_OK );
	sirCVarHandle_t textHandle = {};
	assert( textRegistry.bind( textDefinition, &textHandle ) == SIR_REGISTRATION_OK );
	assert( textRegistry.getValue( textHandle, SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK );
	assert( got.nTextBytes == 4 && memcmp( got.pszText, "base", 4 ) == 0 );
	textRegistry.shutdown();
	// A full override arena must reject an entire batch before publishing any write.
	sirCVarStorage_t small = storage;
	small.nOverrides = 1;
	SIRCVarRegistry limited;
	assert( limited.initialize( registration, small ) == SIR_REGISTRATION_OK );
	sirCVarHandle_t limitedHandles[2] = {};
	const char *szLimitedNames[2] = { "r.value", "r.wide" };
	for ( uint32_t iSlot = 0; iSlot < 2; ++iSlot ) {
		sirRegistrationHandle_t sourceHandle = {};
		assert( registration.find( szLimitedNames[iSlot], strlen( szLimitedNames[iSlot] ), &sourceHandle ) == SIR_REGISTRATION_OK );
		sirCVarDefinition_t definition = {};
		assert( sirCVarDefinitionFromCooked( registration, sourceHandle, cookedSource, &definition ) == SIR_REGISTRATION_OK );
		assert( limited.bind( definition, &limitedHandles[iSlot] ) == SIR_REGISTRATION_OK );
	}
	sirCVarWrite_t failWrites[2] = {};
	failWrites[0] = { limitedHandles[0], console, { 70, nullptr, 0 }, false };
	failWrites[1] = { limitedHandles[1], console, { 700, nullptr, 0 }, false };
	assert( limited.prepareBatch( failWrites, 2, &token ) == SIR_REGISTRATION_CAPACITY_EXCEEDED );
	assert( limited.getPendingEvents() == 0 );
	assert( limited.getValue( limitedHandles[0], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 10 );
	assert( limited.getValue( limitedHandles[1], SIR_CONSOLE_CAP_READ, &got ) == SIR_REGISTRATION_OK && got.nBits == 0 );
	assert( limited.prepareBatch( failWrites, 1, &token ) == SIR_REGISTRATION_OK );
	assert( limited.commitBatch( token ) == SIR_REGISTRATION_OK );
	assert( limited.prepareBatch( failWrites + 1, 1, &token ) == SIR_REGISTRATION_CAPACITY_EXCEEDED );
	limited.shutdown();
	registration.shutdown();
	assert( pool.shutdown() );
	return 0;
}
