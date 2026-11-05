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

#include <Core/CVars/SIRCVarConfigAdapter.h>
#include <Core/CVars/SIRCVarValueCodec.h>
#include <Core/Console/SIRConsoleConfigAdapter.h>
#include <Core/Console/SIRConsoleSystem.h>
#include <Core/SIRApplication.h>
#include <Core/Config/SIRConfigAtomicFile.h>
#include <Tools/RegistrationCooker/SIRRegistrationCooker.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <vector>

static uint64_t sirP5Read64( const uint8_t *pBytes ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t i = 0; i < 8; ++i ) {
		nValue |= ( uint64_t )pBytes[i] << ( i * 8 );
	}
	return nValue;
}

static sirRegistrationCookDefinition_t sirP5Definition( const char *pszName, sirCVarType_t eType, uint32_t nSchemaId, uint32_t nFlags )
{
	sirRegistrationCookDefinition_t entry = {};
	entry.sName = pszName;
	entry.eKind = SIR_REGISTRATION_CVAR;
	entry.eType = eType;
	entry.nSchemaId = nSchemaId;
	entry.nFlags = nFlags;
	return entry;
}

typedef struct sirP5BufferSink_s
{
	char szData[4096];
	uint32_t nBytes;
} sirP5BufferSink_t;

static sirConfigResult_t sirP5Sink( void *pContext, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	sirP5BufferSink_t *pSink = ( sirP5BufferSink_t * )pContext;
	if ( nBytes > sizeof( pSink->szData ) - pSink->nBytes - 1 ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}
	memcpy( pSink->szData + pSink->nBytes, pBytes, ( size_t )nBytes );
	pSink->nBytes += ( uint32_t )nBytes;
	pSink->szData[pSink->nBytes] = 0;
	return SIR_CONFIG_OK;
}

typedef struct sirP5FileFixture_s
{
	const char *pszContents;
	uint32_t nReads;
	uint32_t nCorruptMode;
} sirP5FileFixture_t;

static sirConfigResult_t sirP5ReadFixture( void *pContext, sirConsoleConfigReadRequest_t *pRequest ) noexcept
{
	sirP5FileFixture_t *pFixture = ( sirP5FileFixture_t * )pContext;
	++pFixture->nReads;
	if ( pRequest->nNameBytes != 8 || memcmp( pRequest->pszName, "user.ini", 8 ) != 0 ) {
		return SIR_CONFIG_NOT_FOUND;
	}
	const size_t nLength = strlen( pFixture->pszContents );
	if ( nLength > pRequest->nCapacity ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}
	memcpy( pRequest->pBytes, pFixture->pszContents, nLength );
	pRequest->nRead = ( uint32_t )nLength;
	if ( pFixture->nCorruptMode == 1 ) {
		pRequest->pBytes = nullptr;
	} else if ( pFixture->nCorruptMode == 2 ) {
		pRequest->nCapacity = UINT32_MAX;
	} else if ( pFixture->nCorruptMode == 3 ) {
		pRequest->pszName = "other.ini";
	} else if ( pFixture->nCorruptMode == 4 ) {
		pRequest->nNameBytes = 1;
	}
	return SIR_CONFIG_OK;
}

static void sirP5Drain( SIRCVarRegistry& cvars ) noexcept
{
	while ( cvars.getPendingEvents() != 0 ) {
		assert( cvars.popEvent() == SIR_REGISTRATION_OK );
	}
}

int main( void )
{
	std::vector<sirRegistrationCookDefinition_t> definitions;
	definitions.push_back( sirP5Definition( "r.alpha", SIR_CVAR_U32, 1, SIR_CVAR_FLAG_ARCHIVE ) );
	definitions.push_back( sirP5Definition( "r.title", SIR_CVAR_UTF8_STRING, 2, SIR_CVAR_FLAG_ARCHIVE ) );
	definitions.push_back( sirP5Definition( "r.temp", SIR_CVAR_BOOL, 3, 0 ) );
	definitions.push_back( sirP5Definition( "r.lock", SIR_CVAR_BOOL, 4, SIR_CVAR_FLAG_READ_ONLY | SIR_CVAR_FLAG_ARCHIVE ) );
	definitions[0].nDefaultBits = 10;
	definitions[1].sDefaultString = "base";
	std::vector<uint8_t> strings;
	std::vector<uint8_t> catalog;
	assert( sirCookRegistrationCatalog( definitions, strings, catalog ) );
	const uint64_t nStringHash = sirP5Read64( strings.data() + 36 );
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
	const size_t nPoolBytes = SIRStringPool::requiredBytes( poolConfig, strings.data(), strings.size() );
	assert( nPoolBytes > 0 );
	std::vector<uint8_t> poolMemory( nPoolBytes );
	SIRStringPool pool;
	assert( pool.initialize( { poolMemory.data(), poolMemory.size(), strings.data(), strings.size() }, poolConfig ) );
	sirRegistrationEntry_t szRegistrationEntries[8] = {};
	SIRStringAtom szRegistrationNames[8];
	uint32_t szRegistrationNameIds[8] = {};
	uint64_t szRegistrationBindingIds[8] = {};
	sirRegistrationCatalogInit_t catInit = {};
	catInit.pBytes = catalog.data();
	catInit.nBytes = catalog.size();
	catInit.nStringCatalogHash = nStringHash;
	catInit.pEntries = szRegistrationEntries;
	catInit.nEntryCapacity = 8;
	catInit.pNames = szRegistrationNames;
	catInit.pNameIds = szRegistrationNameIds;
	catInit.nNameCapacity = 8;
	catInit.pBindingIds = szRegistrationBindingIds;
	catInit.nBindingCapacity = 8;
	SIRRegistrationCatalog registration;
	assert( registration.initialize( pool, catInit ) == SIR_REGISTRATION_OK );
	sirCVarSlot_t szSlots[8] = {};
	uint32_t szBank32[8] = {};
	uint64_t szBank64[8] = {};
	sirCVarOverride_t szOverrides[12] = {};
	sirCVarOverride_t szShadow[12] = {};
	sirCVarEvent_t szEvents[12] = {};
	sirCVarEvent_t szScratchEvents[12] = {};
	sirCVarWrite_t szStage[8] = {};
	char szActive[12 * 64] = {};
	char szPending[12 * 64] = {};
	char szStageText[8 * 64] = {};
	char szEventText[12 * 2 * 64] = {};
	sirCVarStorage_t varStorage = { szSlots,
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
	                                12,
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
	SIRCVarRegistry cvars;
	assert( cvars.initialize( registration, varStorage ) == SIR_REGISTRATION_OK );
	sirCVarConfigBinding_t bindings[4] = {};
	const char *szNames[4] = { "r.alpha", "r.title", "r.temp", "r.lock" };
	for ( uint32_t i = 0; i < 4; ++i ) {
		assert( registration.find( szNames[i], strlen( szNames[i] ), &bindings[i].registration ) == SIR_REGISTRATION_OK );
		sirCVarDefinition_t definition = {};
		const sirCVarCookedSource_t source = { catalog.data(), catalog.size(), 1, SIR_CVAR_APPLY_NEXT_SAFE_POINT };
		assert( sirCVarDefinitionFromCooked( registration, bindings[i].registration, source, &definition ) == SIR_REGISTRATION_OK );
		assert( cvars.bind( definition, &bindings[i].cvar ) == SIR_REGISTRATION_OK );
	}
	sirIniEntry_t szIniEntries[8] = {};
	char szIniValues[8 * 64] = {};
	sirCVarWrite_t szIniWrites[8] = {};
	sirCVarConfigStorage_t configStorage = {};
	configStorage.ini = { szIniEntries, 8, szIniValues, 64, sizeof( szIniValues ) };
	configStorage.pWrites = szIniWrites;
	configStorage.nWriteCapacity = 8;
	configStorage.pBindings = bindings;
	configStorage.nBindingCount = 4;
	configStorage.limits.nMaxInputBytes = 2048;
	configStorage.limits.nMaxScalarBytes = 63;
	configStorage.limits.nMaxRowBytes = 512;
	configStorage.limits.nMaxNodes = 8;
	SIRCVarConfigAdapter adapter;
	assert( adapter.initialize( registration, cvars, pool, configStorage ) == SIR_CONFIG_OK );
	const sirCVarOrigin_t user = {
	    SIR_CVAR_SOURCE_USER_ARCHIVE, 5, SIR_CONSOLE_CAP_READ | SIR_CONSOLE_CAP_WRITE, false, false, false, false };
	sirConfigDiagnostic_t diagnostic = {};
	const char *pszGood = "[CVars]\nr.alpha = 21\nr.title = \"hello\\nworld\"\n";
	assert( adapter.applyIni( { ( const uint8_t * )pszGood, strlen( pszGood ), {} }, user, &diagnostic ) == SIR_CONFIG_OK );
	assert( cvars.getRevision() == 1 && cvars.getPendingEvents() == 2 );
	sirCVarValue_t value = {};
	assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 21 );
	assert( cvars.getValue( bindings[1].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK );
	assert( value.nTextBytes == 11 && memcmp( value.pszText, "hello\nworld", 11 ) == 0 );
	sirP5Drain( cvars );
	const uint64_t nOriginalRevision = cvars.getRevision();
	const char *pszBadType = "[CVars]\nr.alpha = 99\nr.temp = invalid_boolean\n";
	assert( adapter.applyIni( { ( const uint8_t * )pszBadType, strlen( pszBadType ), {} }, user, &diagnostic ) ==
	        SIR_CONFIG_TYPE_MISMATCH );
	assert( cvars.getRevision() == nOriginalRevision && cvars.getPendingEvents() == 0 );
	assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 21 );
	const char *pszBadSyntax = "[CVars]\nr.alpha = 45\nr.title = \"broken\\q\"\n";
	assert( adapter.applyIni( { ( const uint8_t * )pszBadSyntax, strlen( pszBadSyntax ), {} }, user, &diagnostic ) ==
	        SIR_CONFIG_SYNTAX_ERROR );
	assert( cvars.getRevision() == nOriginalRevision );
	const char *pszDenied = "[CVars]\nr.alpha = 46\nr.lock = true\n";
	assert( adapter.applyIni( { ( const uint8_t * )pszDenied, strlen( pszDenied ), {} }, user, &diagnostic ) != SIR_CONFIG_OK );
	assert( cvars.getRevision() == nOriginalRevision );
	const char *pszUnknown = "[Other]\nr.alpha = 45\n";
	assert( adapter.applyIni( { ( const uint8_t * )pszUnknown, strlen( pszUnknown ), {} }, user, &diagnostic ) ==
	        SIR_CONFIG_UNSUPPORTED_FEATURE );
	assert( cvars.getRevision() == nOriginalRevision );

	sirP5BufferSink_t archive = {};
	const sirConfigSink_t archiveSink = { &archive, sirP5Sink };
	uint32_t nWritten = 0;
	assert( adapter.writeArchive( archiveSink, SIR_CONSOLE_CAP_READ, sizeof( archive.szData ) - 1, &nWritten ) == SIR_CONFIG_OK );
	assert( nWritten == 2 );
	assert( strcmp( archive.szData, "[CVars]\nr.alpha = \"21\"\nr.title = \"hello\\nworld\"\n" ) == 0 );
	const char *pszArchivePath = "sir-p5-cvar-user.ini";
	const char *pszArchiveTemp = "sir-p5-cvar-user.tmp";
	SIRPlatformFile::remove( pszArchiveTemp );
	SIRPlatformFile::remove( pszArchivePath );
	SIRConfigAtomicFile archiveFile;
	assert( archiveFile.open( pszArchivePath, pszArchiveTemp, 4096 ) == SIR_CONFIG_OK );
	assert( adapter.writeArchive( archiveFile.getSink(), SIR_CONSOLE_CAP_READ, 4096, &nWritten ) == SIR_CONFIG_OK );
	assert( archiveFile.commit() == SIR_CONFIG_OK );
	uint8_t szLoadedArchive[4096] = {};
	uint32_t nReadArchive = 0;
	assert( SIRConfigAtomicFile::read( pszArchivePath, szLoadedArchive, sizeof( szLoadedArchive ), &nReadArchive ) == SIR_CONFIG_OK );
	assert( nReadArchive == archive.nBytes && memcmp( szLoadedArchive, archive.szData, nReadArchive ) == 0 );
	assert( archiveFile.open( pszArchivePath, pszArchiveTemp, 6 ) == SIR_CONFIG_OK );
	assert( adapter.writeArchive( archiveFile.getSink(), SIR_CONSOLE_CAP_READ, 6, &nWritten ) == SIR_CONFIG_LIMIT_EXCEEDED );
	archiveFile.cancel();
	memset( szLoadedArchive, 0, sizeof( szLoadedArchive ) );
	assert( SIRConfigAtomicFile::read( pszArchivePath, szLoadedArchive, sizeof( szLoadedArchive ), &nReadArchive ) == SIR_CONFIG_OK );
	assert( nReadArchive == archive.nBytes && memcmp( szLoadedArchive, archive.szData, nReadArchive ) == 0 );
	assert( SIRPlatformFile::remove( pszArchivePath ) == SIR_FILE_OK );
	const uint64_t nPreRoundTrip = cvars.getRevision();
	assert( adapter.applyIni( { ( const uint8_t * )archive.szData, archive.nBytes, {} }, user, &diagnostic ) == SIR_CONFIG_OK );
	assert( cvars.getRevision() == nPreRoundTrip + 1 );
	assert( adapter.writeArchive( archiveSink, SIR_CONSOLE_CAP_READ, 5, &nWritten ) == SIR_CONFIG_LIMIT_EXCEEDED );

	const char *pszExecInput = "[CVars]\nr.alpha = 77\n";
	sirP5FileFixture_t fixture = { pszExecInput, 0, 0 };
	uint8_t szFile[1024] = {};
	sirConsoleConfigStorage_t execStorage = { szFile, sizeof( szFile ), sirP5ReadFixture, &fixture };
	SIRConsoleConfigAdapter exec;
	assert( exec.initialize( adapter, execStorage ) == SIR_CONFIG_OK );
	sirConsoleSubmission_t submission = {};
	submission.origin = user;
	submission.origin.nCapabilities |= SIR_CONSOLE_CAP_EXECUTE;
	submission.nRequestId = 1;
	assert( SIRConsoleConfigAdapter::execute( &exec, "../user.ini", 11, submission ) == SIR_REGISTRATION_INVALID_NAME );
	assert( SIRConsoleConfigAdapter::execute( &exec, "user.cfg", 8, submission ) == SIR_REGISTRATION_INVALID_NAME );
	assert( fixture.nReads == 0 );
	assert( SIRConsoleConfigAdapter::execute( &exec, "user.ini", 8, submission ) == SIR_REGISTRATION_OK );
	assert( fixture.nReads == 1 );
	assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 77 );
	sirP5Drain( cvars );
	const uint64_t nBeforeFailedExec = cvars.getRevision();
	fixture.pszContents = "[CVars]\nr.alpha = 13\nr.temp = maybe\n";
	assert( SIRConsoleConfigAdapter::execute( &exec, "user.ini", 8, submission ) == SIR_REGISTRATION_TYPE_MISMATCH );
	assert( cvars.getRevision() == nBeforeFailedExec );
	assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 77 );
	fixture.pszContents = "[CVars]\nr.alpha = 99\n";
	for ( uint32_t nMode = 1; nMode <= 4; ++nMode ) {
		fixture.nCorruptMode = nMode;
		assert( SIRConsoleConfigAdapter::execute( &exec, "user.ini", 8, submission ) == SIR_REGISTRATION_BAD_SCHEMA );
		assert( cvars.getRevision() == nBeforeFailedExec );
		assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 77 );
	}
	fixture.nCorruptMode = 0;
	submission.origin.nCapabilities &= ~SIR_CONSOLE_CAP_WRITE;
	assert( SIRConsoleConfigAdapter::execute( &exec, "user.ini", 8, submission ) == SIR_REGISTRATION_DENIED );

	// Full P4 console pump dispatches exec into the P5 INI transaction adapter.
	sirConsoleRequestSlot_t szConsoleRequests[2];
	char szConsoleText[2 * 128] = {};
	sirConsoleStorage_t consoleStorage = {};
	consoleStorage.pRequests = szConsoleRequests;
	consoleStorage.nRequestCapacity = 2;
	consoleStorage.pszRequestText = szConsoleText;
	consoleStorage.nRequestTextBytes = sizeof( szConsoleText );
	consoleStorage.nInputStride = 128;
	consoleStorage.nMaxCommandsPerPump = 4;
	consoleStorage.nMaxOutputsPerCommand = 4;
	consoleStorage.pfnExec = SIRConsoleConfigAdapter::execute;
	consoleStorage.pExecContext = &exec;
	SIRConsoleService console;
	assert( console.initialize( registration, cvars, pool, consoleStorage ) == SIR_REGISTRATION_OK );
	fixture.pszContents = "[CVars]\nr.alpha = 88\n";
	submission.origin.nCapabilities |= SIR_CONSOLE_CAP_WRITE;
	submission.nRequestId = 100;
	assert( console.submit( "exec user.ini", 13, submission ) == SIR_REGISTRATION_OK );
	sirConsolePumpReport_t report = {};
	assert( console.pump( 4, &report ) == SIR_REGISTRATION_OK );
	assert( report.nProcessed == 1 && report.nSucceeded == 1 && report.nRejected == 0 );
	assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 88 );
	sirP5Drain( cvars );
	const uint64_t nBeforeConsoleError = cvars.getRevision();
	fixture.pszContents = "[CVars]\nr.alpha = 91\nr.temp = bad\n";
	submission.nRequestId = 101;
	assert( console.submit( "exec user.ini", 13, submission ) == SIR_REGISTRATION_OK );
	assert( console.pump( 4, &report ) == SIR_REGISTRATION_OK );
	assert( report.nProcessed == 1 && report.nRejected == 1 );
	assert( cvars.getRevision() == nBeforeConsoleError );
	assert( cvars.getValue( bindings[0].cvar, SIR_CONSOLE_CAP_READ, &value ) == SIR_REGISTRATION_OK && value.nBits == 88 );
	console.shutdown();

	// SIRApplication owns the lifecycle transition and bounded owner-thread pump.
	sirConsoleSystemConfig_t runtimeConfig = { &registration, &cvars, &pool, consoleStorage, 2 };
	SIRConsoleSystem runtime( runtimeConfig );
	SIRApplication runtimeApplication;
	assert( runtimeApplication.registerSystem( &runtime ) );
	assert( runtimeApplication.initialize() && runtime.isInitialized() );
	assert( runtime.getConsole().submit( "help", 4, { submission.origin, 303 } ) == SIR_REGISTRATION_OK );
	assert( runtimeApplication.tick() );
	assert( runtime.getLastPumpResult() == SIR_REGISTRATION_OK );
	assert( runtime.getLastPumpReport().nProcessed == 1 && runtime.getLastPumpReport().nSucceeded == 1 );
	assert( runtime.getConsole().submit( "help", 4, { submission.origin, 304 } ) == SIR_REGISTRATION_OK );
	assert( runtimeApplication.shutdown() == SIR_SYSTEM_OK );
	assert( runtimeApplication.getState() == SIR_APP_STOPPED && !runtime.isInitialized() );
	assert( runtime.getConsole().submit( "help", 4, { submission.origin, 305 } ) == SIR_REGISTRATION_NOT_INITIALIZED );

	// Failed console initialization must roll the application back to STOPPED.
	sirConsoleSystemConfig_t invalidRuntime = runtimeConfig;
	invalidRuntime.storage.nRequestCapacity = 0;
	SIRConsoleSystem invalidSystem( invalidRuntime );
	SIRApplication invalidApplication;
	assert( invalidApplication.registerSystem( &invalidSystem ) );
	assert( !invalidApplication.initialize() );
	assert( invalidApplication.getState() == SIR_APP_STOPPED && !invalidSystem.isInitialized() );

	exec.shutdown();
	adapter.shutdown();
	cvars.shutdown();
	registration.shutdown();
	assert( pool.shutdown() );
	return 0;
}
