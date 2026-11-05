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

#include <Core/Console/SIRConsoleService.h>
#include <Tools/RegistrationCooker/SIRRegistrationCooker.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <atomic>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

// A headless consumer copies output synchronously; console never stores
// sink-owned memory.
typedef struct sirConsoleTestRecord_s
{
	uint64_t nRequestId;
	sirRegistrationResult_t eResult;
	std::string sText;
} sirConsoleTestRecord_t;

static std::vector<sirConsoleTestRecord_t> s_Results;
static uint32_t s_nCallbackCount = 0;
static uint32_t s_nLeaseAcquire = 0;
static uint32_t s_nLeaseRelease = 0;
static bool s_bModuleAlive = true;
static bool s_bSubmittedRecursive = false;

static void sirTestOutput( void *, const sirConsoleOutput_t& output ) noexcept
{
	s_Results.push_back( { output.nRequestId, output.eResult, std::string( output.pszText, output.nBytes ) } );
}

static bool sirTestAcquire( void *, uint32_t nOwnerId ) noexcept
{
	assert( nOwnerId == 88 );
	if ( !s_bModuleAlive ) {
		return false;
	}
	++s_nLeaseAcquire;
	return true;
}

static void sirTestRelease( void *, uint32_t nOwnerId ) noexcept
{
	assert( nOwnerId == 88 );
	++s_nLeaseRelease;
}

static sirCVarOrigin_t sirTestOrigin( uint32_t nCapabilities ) noexcept
{
	sirCVarOrigin_t origin = {};
	origin.eSource = SIR_CVAR_SOURCE_SESSION_CONSOLE;
	origin.nIssuerId = 7;
	origin.nCapabilities = nCapabilities;
	return origin;
}

static void sirTestCallback( void *, const void *pInvocation, const void *pArguments ) noexcept
{
	const sirConsoleInvocation_t& invocation = *( const sirConsoleInvocation_t * )pInvocation;
	const sirConsoleArguments_t& args = *( const sirConsoleArguments_t * )pArguments;
	assert( args.nCount >= 1 );
	++s_nCallbackCount;
	if ( args.nCount > 1 && strcmp( args.szArgs[1].pszText, "spam" ) == 0 ) {
		for ( uint32_t iMessage = 0; iMessage < 32; ++iMessage ) {
			const sirConsoleOutput_t output = { invocation.submission.nRequestId, SIR_REGISTRATION_OK, "spam", 4 };
			const sirRegistrationResult_t eWrite = invocation.pConsole->writeOutput( output );
			assert( iMessage < 16 ? eWrite == SIR_REGISTRATION_OK : eWrite == SIR_REGISTRATION_QUEUE_FULL );
		}
		return;
	}
	if ( args.nCount > 1 ) {
		assert( strcmp( args.szArgs[1].pszText, "hello world" ) == 0 );
	}
	const sirConsoleOutput_t output = { invocation.submission.nRequestId, SIR_REGISTRATION_OK, "pong", 4 };
	assert( invocation.pConsole->writeOutput( output ) == SIR_REGISTRATION_OK );
	sirConsolePumpReport_t nested = {};
	assert( invocation.pConsole->pump( 1, &nested ) == SIR_REGISTRATION_DENIED );
	uint32_t nRemoved = 0;
	assert( invocation.pConsole->unloadModule( 88, &nRemoved ) == SIR_REGISTRATION_DENIED );
	if ( !s_bSubmittedRecursive ) {
		s_bSubmittedRecursive = true;
		const sirConsoleSubmission_t next = { invocation.submission.origin, invocation.submission.nRequestId + 1000 };
		assert( invocation.pConsole->submit( "help", 4, next ) == SIR_REGISTRATION_OK );
	}
}

static uint64_t sirTestRead64( const uint8_t *pBytes ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nValue |= ( uint64_t )pBytes[iByte] << ( iByte * 8 );
	}
	return nValue;
}

static sirRegistrationCookDefinition_t sirTestCommand( const char *pszName, uint64_t nBinding, uint32_t nOwnerId )
{
	sirRegistrationCookDefinition_t result = {};
	result.sName = pszName;
	result.eKind = SIR_REGISTRATION_COMMAND;
	result.nBindingId = nBinding;
	result.nOwnerId = nOwnerId;
	result.nCapabilities = SIR_CONSOLE_CAP_EXECUTE;
	return result;
}

static sirRegistrationCookDefinition_t sirTestCVar( const char *pszName, uint32_t nSchemaId, sirCVarType_t eType, uint32_t nFlags )
{
	sirRegistrationCookDefinition_t result = {};
	result.sName = pszName;
	result.eKind = SIR_REGISTRATION_CVAR;
	result.nSchemaId = nSchemaId;
	result.eType = eType;
	result.nFlags = nFlags;
	result.nDefaultBits = 0;
	return result;
}

static bool sirTestHasOutput( uint64_t nRequestId, sirRegistrationResult_t eResult, const char *pszText )
{
	for ( const sirConsoleTestRecord_t& record : s_Results ) {
		if ( record.nRequestId == nRequestId && record.eResult == eResult && record.sText == pszText ) {
			return true;
		}
	}
	return false;
}

int main( void )
{
	std::vector<sirRegistrationCookDefinition_t> definitions;
	definitions.push_back( sirTestCommand( "engine.ping", 101, 0 ) );
	definitions.push_back( sirTestCommand( "mod.ping", 102, 88 ) );
	definitions.push_back( sirTestCVar( "r.count", 201, SIR_CVAR_U32, 0 ) );
	definitions.push_back( sirTestCVar( "r.bool", 202, SIR_CVAR_BOOL, 0 ) );
	definitions.push_back( sirTestCVar( "r.title", 203, SIR_CVAR_UTF8_STRING, 0 ) );
	definitions.push_back( sirTestCVar( "r.lock", 204, SIR_CVAR_BOOL, SIR_CVAR_FLAG_READ_ONLY ) );
	definitions.push_back( sirTestCVar( "r.secret", 205, SIR_CVAR_U32, SIR_CVAR_FLAG_SENSITIVE | SIR_CVAR_FLAG_HIDDEN ) );
	definitions[2].nDefaultBits = 10;
	definitions[4].sDefaultString = "base";
	definitions[6].nDefaultBits = 123;
	std::vector<uint8_t> cookedStrings;
	std::vector<uint8_t> cookedRegistration;
	assert( sirCookRegistrationCatalog( definitions, cookedStrings, cookedRegistration ) );
	const uint64_t nStringHash = sirTestRead64( cookedStrings.data() + 36 );
	sirStringPoolConfig_t config = SIRStringPool::defaultConfig();
	config.nHashBuckets = 128;
	config.nMaxNames = 32;
	config.nDynamicTextBytes = 512;
	config.bAdaptiveBudgets = false;
	config.bTieredCache = false;
	config.nShortSlots = 2;
	config.nLongSlots = 1;
	config.nShortBytes = 128;
	config.nLongBytes = 1024;
	const size_t nRequired = SIRStringPool::requiredBytes( config, cookedStrings.data(), cookedStrings.size() );
	assert( nRequired > 0 );
	std::vector<uint8_t> poolMemory( nRequired );
	SIRStringPool pool;
	assert( pool.initialize( { poolMemory.data(), poolMemory.size(), cookedStrings.data(), cookedStrings.size() }, config ) );

	sirRegistrationEntry_t szEntries[16] = {};
	SIRStringAtom szAtoms[16];
	uint32_t szNameIds[16] = {};
	uint64_t szBindingIds[16] = {};
	sirRegistrationBinding_t szBindings[2] = { { 101, 0, sirTestCallback, nullptr }, { 102, 88, sirTestCallback, nullptr } };
	sirRegistrationCatalogInit_t catalog = {};
	catalog.pBytes = cookedRegistration.data();
	catalog.nBytes = cookedRegistration.size();
	catalog.nStringCatalogHash = nStringHash;
	catalog.pEntries = szEntries;
	catalog.nEntryCapacity = 16;
	catalog.pNames = szAtoms;
	catalog.pNameIds = szNameIds;
	catalog.nNameCapacity = 16;
	catalog.pBindingIds = szBindingIds;
	catalog.nBindingCapacity = 16;
	catalog.pBindings = szBindings;
	catalog.nBindingCount = 2;
	SIRRegistrationCatalog registration;
	assert( registration.initialize( pool, catalog ) == SIR_REGISTRATION_OK );

	sirCVarSlot_t szSlots[8] = {};
	uint32_t szBank32[8] = {};
	uint64_t szBank64[8] = {};
	sirCVarOverride_t szOverrides[16] = {};
	sirCVarOverride_t szShadow[16] = {};
	sirCVarEvent_t szEvents[16] = {};
	sirCVarEvent_t szEventScratch[16] = {};
	sirCVarWrite_t szWrites[8] = {};
	char szActive[16 * 64] = {};
	char szPending[16 * 64] = {};
	char szStaged[8 * 64] = {};
	char szEventText[16 * 2 * 64] = {};
	sirCVarStorage_t valueStorage = { szSlots,
	                                  8,
	                                  szBank32,
	                                  8,
	                                  szBank64,
	                                  8,
	                                  szOverrides,
	                                  szShadow,
	                                  16,
	                                  szEvents,
	                                  szEventScratch,
	                                  16,
	                                  szWrites,
	                                  8,
	                                  szActive,
	                                  szPending,
	                                  szStaged,
	                                  szEventText,
	                                  64,
	                                  sizeof( szActive ),
	                                  sizeof( szPending ),
	                                  sizeof( szStaged ),
	                                  sizeof( szEventText ) };
	SIRCVarRegistry cvars;
	assert( cvars.initialize( registration, valueStorage ) == SIR_REGISTRATION_OK );
	const char *szKeys[5] = { "r.count", "r.bool", "r.title", "r.lock", "r.secret" };
	sirConsoleCVarBinding_t szCVarBindings[5] = {};
	for ( uint32_t iBinding = 0; iBinding < 5; ++iBinding ) {
		sirRegistrationHandle_t handle = {};
		assert( registration.find( szKeys[iBinding], strlen( szKeys[iBinding] ), &handle ) == SIR_REGISTRATION_OK );
		sirCVarDefinition_t def = {};
		const sirCVarCookedSource_t src = { cookedRegistration.data(), cookedRegistration.size(), 0, SIR_CVAR_APPLY_NEXT_SAFE_POINT };
		assert( sirCVarDefinitionFromCooked( registration, handle, src, &def ) == SIR_REGISTRATION_OK );
		szCVarBindings[iBinding].registration = handle;
		assert( cvars.bind( def, &szCVarBindings[iBinding].cvar ) == SIR_REGISTRATION_OK );
	}

	sirConsoleRequestSlot_t szRequests[8];
	char szRequestText[8 * 128] = {};
	sirConsoleHistoryEntry_t szHistory[8] = {};
	char szHistoryText[8 * 128] = {};
	sirConsoleCompletionCandidate_t szCompletions[16] = {};
	sirConsoleStorage_t storage = {};
	storage.pRequests = szRequests;
	storage.nRequestCapacity = 8;
	storage.pszRequestText = szRequestText;
	storage.nRequestTextBytes = sizeof( szRequestText );
	storage.nInputStride = 128;
	storage.pHistory = szHistory;
	storage.nHistoryCapacity = 8;
	storage.pszHistoryText = szHistoryText;
	storage.nHistoryTextBytes = sizeof( szHistoryText );
	storage.pCompletionScratch = szCompletions;
	storage.nCompletionCapacity = 16;
	storage.pCVarBindings = szCVarBindings;
	storage.nCVarBindings = 5;
	storage.nMaxCommandsPerPump = 16;
	storage.nMaxOutputsPerCommand = 16;
	storage.pfnOutput = sirTestOutput;
	storage.pfnAcquireModule = sirTestAcquire;
	storage.pfnReleaseModule = sirTestRelease;
	SIRConsoleService invalid;
	sirConsoleStorage_t wrong = storage;
	wrong.nRequestTextBytes = 1;
	assert( invalid.initialize( registration, cvars, pool, wrong ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	SIRConsoleService console;
	assert( console.initialize( registration, cvars, pool, storage ) == SIR_REGISTRATION_OK );

	const uint32_t nAll = SIR_CONSOLE_CAP_READ | SIR_CONSOLE_CAP_WRITE | SIR_CONSOLE_CAP_EXECUTE;
	const sirConsoleSubmission_t user = { sirTestOrigin( nAll ), 1 };
	sirConsolePumpReport_t report = {};
	assert( console.submit( "GET R.COUNT", 11, user ) == SIR_REGISTRATION_OK );
	assert( console.pump( 1, &report ) == SIR_REGISTRATION_OK );
	assert( report.nProcessed == 1 && report.nSucceeded == 1 );
	assert( sirTestHasOutput( 1, SIR_REGISTRATION_OK, "10" ) );
	assert( console.submit( "set r.count 25", 14, { user.origin, 2 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( console.submit( "get r.count", 11, { user.origin, 3 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK );
	assert( sirTestHasOutput( 3, SIR_REGISTRATION_OK, "25" ) );
	assert( console.submit( "set r.count \"oops", 17, { user.origin, 4 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 4, SIR_REGISTRATION_BAD_SCHEMA, "command rejected" ) );
	assert( console.submit( "set r.count 26; junk", 20, { user.origin, 5 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( console.submit( "get r.count", 11, { user.origin, 6 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK );
	assert( sirTestHasOutput( 6, SIR_REGISTRATION_OK, "25" ) );
	assert( console.submit( "set r.title \"hello world\"", 25, { user.origin, 7 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( console.submit( "get r.title", 11, { user.origin, 8 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK );
	assert( sirTestHasOutput( 8, SIR_REGISTRATION_OK, "hello world" ) );
	assert( console.submit( "set r.lock 1", 12, { user.origin, 9 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 9, SIR_REGISTRATION_DENIED, "command rejected" ) );
	assert( console.submit( "get r.secret", 12, { user.origin, 10 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( console.submit( "toggle r.bool", 13, { user.origin, 11 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( console.submit( "get r.bool", 10, { user.origin, 12 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK );
	assert( sirTestHasOutput( 12, SIR_REGISTRATION_OK, "true" ) );
	assert( console.submit( "reset r.bool", 12, { user.origin, 13 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK );
	assert( console.submit( "get r.bool", 10, { user.origin, 14 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK );
	assert( sirTestHasOutput( 14, SIR_REGISTRATION_OK, "false" ) );
	assert( console.submit( "exec profile.cfg", 16, { user.origin, 15 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 15, SIR_REGISTRATION_BAD_SCHEMA, "command rejected" ) );

	char szCompletionOutput[512] = {};
	sirConsoleCompletionResult_t completion = { szCompletionOutput, sizeof( szCompletionOutput ), nAll, 0, 0 };
	assert( console.complete( "R.", 2, &completion ) == SIR_REGISTRATION_OK );
	assert( strstr( szCompletionOutput, "r.bool\n" ) != nullptr );
	assert( strstr( szCompletionOutput, "r.count\n" ) != nullptr );
	assert( strstr( szCompletionOutput, "r.secret\n" ) == nullptr );
	assert( strstr( szCompletionOutput, "r.title\n" ) != nullptr );
	assert( console.complete( "invalid?", 8, &completion ) == SIR_REGISTRATION_INVALID_NAME );
	completion.nCapabilities = nAll | SIR_CONSOLE_CAP_ADMIN;
	assert( console.complete( "r.", 2, &completion ) == SIR_REGISTRATION_OK );
	assert( strstr( szCompletionOutput, "r.secret\n" ) == nullptr );
	completion.nCapabilities |= SIR_CONSOLE_CAP_SENSITIVE;
	assert( console.complete( "r.", 2, &completion ) == SIR_REGISTRATION_OK );
	assert( strstr( szCompletionOutput, "r.secret\n" ) != nullptr );
	assert( console.submit( "list", 4, { user.origin, 16 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( !sirTestHasOutput( 16, SIR_REGISTRATION_OK, "r.secret" ) );
	assert( console.submit( "get r.count", 11, { sirTestOrigin( SIR_CONSOLE_CAP_EXECUTE ), 17 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	char szHistoryBuffer[128] = {};
	sirConsoleHistoryView_t history = { szHistoryBuffer, sizeof( szHistoryBuffer ), 0, 0 };
	assert( console.readHistory( 0, &history ) == SIR_REGISTRATION_OK );
	assert( strcmp( szHistoryBuffer, "get r.count" ) == 0 && history.nRequestId == 17 );
	assert( console.readHistory( 2, &history ) == SIR_REGISTRATION_OK );
	assert( strcmp( szHistoryBuffer, "exec profile.cfg" ) == 0 && history.nRequestId == 15 );

	// Caller input is copied before the producer returns; mutations cannot change execution.
	char szBorrowed[] = "help";
	assert( console.submit( szBorrowed, 4, { user.origin, 400 } ) == SIR_REGISTRATION_OK );
	memcpy( szBorrowed, "nope", 4 );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( sirTestHasOutput( 400, SIR_REGISTRATION_OK, "help list get set reset toggle (exec awaits P5 Config adapter)" ) );
	assert( console.submit( nullptr, 4, { user.origin, 401 } ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	assert( console.submit( "", 0, { user.origin, 401 } ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	char szOversized[130] = {};
	memset( szOversized, 'x', sizeof( szOversized ) );
	assert( console.submit( szOversized, 128, { user.origin, 401 } ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	const char szEmbeddedNul[] = { 'h', 'e', 'l', 'p', '\0', 'x' };
	assert( console.submit( szEmbeddedNul, sizeof( szEmbeddedNul ), { user.origin, 401 } ) == SIR_REGISTRATION_INVALID_ARGUMENT );
	const char szBadEscape[] = "help \\q";
	assert( console.submit( szBadEscape, strlen( szBadEscape ), { user.origin, 402 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 402, SIR_REGISTRATION_BAD_SCHEMA, "command rejected" ) );
	assert( console.submit( "bad?name", 8, { user.origin, 403 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 403, SIR_REGISTRATION_INVALID_NAME, "command rejected" ) );
	char szTooManyArgs[64] = {};
	uint32_t nTooManyBytes = 0;
	for ( uint32_t iArg = 0; iArg < SIR_CONSOLE_MAX_ARGS + 1; ++iArg ) {
		if ( iArg != 0 ) {
			szTooManyArgs[nTooManyBytes++] = ' ';
		}
		szTooManyArgs[nTooManyBytes++] = 'a';
	}
	assert( console.submit( szTooManyArgs, nTooManyBytes, { user.origin, 404 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 404, SIR_REGISTRATION_CAPACITY_EXCEEDED, "command rejected" ) );
	const char szEscapedValue[] = "set r.title \"a\\ b\"";
	assert( console.submit( szEscapedValue, strlen( szEscapedValue ), { user.origin, 405 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( console.submit( "get r.title", 11, { user.origin, 406 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nSucceeded == 1 );
	assert( sirTestHasOutput( 406, SIR_REGISTRATION_OK, "a b" ) );
	const char szOverflow[] = "set r.count 4294967296";
	assert( console.submit( szOverflow, strlen( szOverflow ), { user.origin, 407 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 407, SIR_REGISTRATION_TYPE_MISMATCH, "command rejected" ) );
	assert( console.submit( "mod.ping \"hello world\"", 22, { user.origin, 100 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 16, &report ) == SIR_REGISTRATION_OK && report.nProcessed == 1 );
	assert( s_nCallbackCount == 1 && s_nLeaseAcquire == 1 && s_nLeaseRelease == 1 );
	assert( sirTestHasOutput( 100, SIR_REGISTRATION_OK, "pong" ) );
	assert( console.pump( 16, &report ) == SIR_REGISTRATION_OK && report.nProcessed == 1 );
	assert( sirTestHasOutput( 1100, SIR_REGISTRATION_OK, "help list get set reset toggle (exec awaits P5 Config adapter)" ) );
	// A failed module-code pin prevents native callback entry.
	s_bModuleAlive = false;
	assert( console.submit( "mod.ping", 8, { user.origin, 111 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( sirTestHasOutput( 111, SIR_REGISTRATION_DENIED, "command rejected" ) );
	assert( s_nCallbackCount == 1 && s_nLeaseRelease == 1 );
	s_bModuleAlive = true;
	assert( console.submit( "engine.ping spam", 16, { user.origin, 101 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 16, &report ) == SIR_REGISTRATION_OK );
	assert( report.nOutputOverflow == 16 && report.nOutputs == 16 );

	for ( uint32_t iRequest = 0; iRequest < 8; ++iRequest ) {
		assert( console.submit( "help", 4, { user.origin, 200 + iRequest } ) == SIR_REGISTRATION_OK );
	}
	assert( console.submit( "help", 4, { user.origin, 208 } ) == SIR_REGISTRATION_QUEUE_FULL );
	assert( console.pump( 2, &report ) == SIR_REGISTRATION_OK && report.nProcessed == 2 );
	assert( console.submit( "help", 4, { user.origin, 208 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 0, &report ) == SIR_REGISTRATION_OK && report.nProcessed == 7 );

	const sirRegistrationDynamicDesc_t dynamic = {
	    { "mod.dynamic", 11, 88 }, SIR_REGISTRATION_COMMAND, 303, 0, 0, SIR_CONSOLE_CAP_EXECUTE, SIR_CVAR_BOOL, sirTestCallback, nullptr };
	sirRegistrationHandle_t dynamicHandle = {};
	assert( console.registerDynamicCommand( dynamic, &dynamicHandle ) == SIR_REGISTRATION_OK );
	assert( console.submit( "mod.dynamic", 11, { user.origin, 301 } ) == SIR_REGISTRATION_OK );
	assert( console.unregisterDynamicCommand( dynamicHandle ) == SIR_REGISTRATION_OK );
	assert( console.pump( 16, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );
	assert( s_nCallbackCount == 2 );
	assert( console.unregisterDynamicCommand( dynamicHandle ) == SIR_REGISTRATION_STALE_HANDLE );
	assert( console.registerDynamicCommand( dynamic, &dynamicHandle ) == SIR_REGISTRATION_OK );
	uint32_t nUnloaded = 0;
	assert( console.unloadModule( 88, &nUnloaded ) == SIR_REGISTRATION_OK && nUnloaded == 2 );
	assert( console.submit( "mod.ping", 8, { user.origin, 302 } ) == SIR_REGISTRATION_OK );
	assert( console.pump( 16, &report ) == SIR_REGISTRATION_OK && report.nRejected == 1 );

	std::atomic<uint32_t> nProcessed{ 0 };
	std::thread workers[3];
	for ( uint32_t iWorker = 0; iWorker < 3; ++iWorker ) {
		workers[iWorker] = std::thread( [&console, &user, iWorker]() {
			for ( uint32_t iSubmit = 0; iSubmit < 20; ++iSubmit ) {
				const sirConsoleSubmission_t request = { user.origin, 5000 + iWorker * 100 + iSubmit };
				while ( console.submit( "help", 4, request ) == SIR_REGISTRATION_QUEUE_FULL ) {
					std::this_thread::yield();
				}
			}
		} );
	}
	while ( nProcessed.load() < 60 ) {
		assert( console.pump( 8, &report ) == SIR_REGISTRATION_OK );
		nProcessed.fetch_add( report.nProcessed );
		if ( report.nProcessed == 0 ) {
			std::this_thread::yield();
		}
	}
	for ( std::thread& worker : workers ) {
		worker.join();
	}
	assert( nProcessed.load() == 60 );
	std::atomic<uint32_t> eWrongThread{ 0 };
	std::thread other( [&console, &eWrongThread]() {
		sirConsolePumpReport_t wrongReport = {};
		eWrongThread.store( console.pump( 1, &wrongReport ) );
	} );
	other.join();
	assert( eWrongThread.load() == SIR_REGISTRATION_WRONG_THREAD );
	assert( console.getHistoryCount() == 8 );
	console.shutdown();
	assert( console.submit( "help", 4, user ) == SIR_REGISTRATION_NOT_INITIALIZED );
	cvars.shutdown();
	registration.shutdown();
	assert( pool.shutdown() );
	return 0;
}
