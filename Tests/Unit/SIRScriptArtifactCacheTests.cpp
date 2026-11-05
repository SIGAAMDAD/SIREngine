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

#include <Scripting/Core/SIRScriptArtifactCache.h>
#include <Scripting/Core/SIRScriptSHA256.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <stdio.h>
#include <string.h>
#include <thread>

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool sameDigest( const uint8_t *pData, const uint8_t *pHexText ) noexcept
{
	static constexpr char szHex[] = "0123456789abcdef";
	for ( uint32_t iByte = 0; iByte < 32; ++iByte ) {
		if ( szHex[pData[iByte] >> 4] != pHexText[iByte * 2] || szHex[pData[iByte] & 15] != pHexText[iByte * 2 + 1] ) {
			return false;
		}
	}
	return true;
}

static bool testHashVectors( void )
{
	uint8_t szDigest[32] = {};
	SIRScriptSHA256::digest( nullptr, 0, szDigest );
	SIR_TEST_CHECK( sameDigest( szDigest, ( const uint8_t * )"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ) );
	SIRScriptSHA256::digest( "abc", 3, szDigest );
	SIR_TEST_CHECK( sameDigest( szDigest, ( const uint8_t * )"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ) );
	uint8_t szSequence[1000] = {};
	memset( szSequence, 'a', sizeof( szSequence ) );
	SIRScriptSHA256::digest( szSequence, sizeof( szSequence ), szDigest );
	SIR_TEST_CHECK( sameDigest( szDigest, ( const uint8_t * )"41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3" ) );
	return true;
}

static bool testCache( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-script-cache-test-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	const std::string szRoot = root.string();

	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( cache.configure( "", 8192 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( cache.configure( szRoot.c_str(), 8192 ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.configure( szRoot.c_str(), 8192 ) == SIR_SCRIPT_WRONG_STATE );
	const char szRecipe[] = "frontend=any;schema=0x1234;target=x86_64;compiler=23;settings=baseline";
	sirScriptCacheKey_t key = {};
	SIR_TEST_CHECK( SIRScriptArtifactCache::makeKey( nullptr, 10, &key ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( SIRScriptArtifactCache::makeKey( szRecipe, sizeof( szRecipe ) - 1, &key ) == SIR_SCRIPT_OK );
	const sirScriptCodeArtifact_t artifact = { 7, 91, 3, 77, 0x1234, 87, 33 };
	alignas( 64 ) uint8_t szBytes[640] = {};
	for ( uint32_t iByte = 0; iByte < sizeof( szBytes ); ++iByte ) {
		szBytes[iByte] = ( uint8_t )( iByte * 17 + 3 );
	}
	sirScriptCachedArtifact_t record = {};
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( record.nBytes == 640 && record.nAlignment == 64 );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_DUPLICATE );
	alignas( 64 ) uint8_t szOutput[640] = {};
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( szOutput, szBytes, sizeof( szBytes ) ) == 0 );
	SIR_TEST_CHECK( cache.fetch( record, szOutput + 1, sizeof( szOutput ) - 1 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, 639 ) == SIR_SCRIPT_INVALID_ARGUMENT );

	SIRScriptArtifactCache second;
	SIR_TEST_CHECK( second.configure( szRoot.c_str(), 8192 ) == SIR_SCRIPT_OK );
	memset( szOutput, 0, sizeof( szOutput ) );
	SIR_TEST_CHECK( second.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( szOutput, szBytes, sizeof( szBytes ) ) == 0 );
	const sirScriptCodeArtifact_t incompatible = { 7, 91, 4, 77, 0x1234, 87, 33 };
	SIR_TEST_CHECK( cache.store( incompatible, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_TYPE_MISMATCH );
	uint8_t szChanged[640] = {};
	memcpy( szChanged, szBytes, sizeof( szChanged ) );
	szChanged[5] ^= 1;
	SIR_TEST_CHECK( cache.store( artifact, key, szChanged, sizeof( szChanged ), 64, &record ) == SIR_SCRIPT_TYPE_MISMATCH );

	std::filesystem::path file;
	for ( const auto& entry : std::filesystem::directory_iterator( root ) ) {
		if ( entry.path().extension() == ".sirbc" ) {
			file = entry.path();
		}
	}
	SIR_TEST_CHECK( !file.empty() );
	FILE *pCorrupt = fopen( file.string().c_str(), "r+b" );
	SIR_TEST_CHECK( pCorrupt != nullptr );
	SIR_TEST_CHECK( fseek( pCorrupt, 152 + 11, SEEK_SET ) == 0 );
	SIR_TEST_CHECK( fputc( 0xaa, pCorrupt ) != EOF );
	SIR_TEST_CHECK( fclose( pCorrupt ) == 0 );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( cache.erase( key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( cache.erase( key ) == SIR_SCRIPT_NOT_FOUND );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_OK );

	// An extra byte after the declared payload is also a malformed entry.
	FILE *pAppend = fopen( file.string().c_str(), "ab" );
	SIR_TEST_CHECK( pAppend != nullptr );
	SIR_TEST_CHECK( fputc( 0, pAppend ) != EOF );
	SIR_TEST_CHECK( fclose( pAppend ) == 0 );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( cache.erase( key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_OK );

	// A short header must never be accepted; staging replaces incomplete files.
	FILE *pTruncate = fopen( file.string().c_str(), "wb" );
	SIR_TEST_CHECK( pTruncate != nullptr );
	SIR_TEST_CHECK( fputc( 0, pTruncate ) != EOF );
	SIR_TEST_CHECK( fclose( pTruncate ) == 0 );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 64, &record ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.fetch( record, szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_OK );

	sirScriptResult_t eOtherThread = SIR_SCRIPT_OK;
	std::thread worker( [&]() { eOtherThread = second.fetch( record, szOutput, sizeof( szOutput ) ); } );
	worker.join();
	SIR_TEST_CHECK( eOtherThread == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( cache.erase( key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) >= 1 );
	return true;
}

static bool testInvalidRecords( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-cache-invalid-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	const std::string szRoot = root.string();
	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( cache.configure( szRoot.c_str(), 16 ) == SIR_SCRIPT_OK );
	alignas( 64 ) uint8_t szBytes[32] = {};
	const sirScriptCodeArtifact_t artifact = { 1, 2, 3, 4, 5, 6, 7 };
	sirScriptCacheKey_t key = {};
	sirScriptCachedArtifact_t record = {};
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, 16, 1, &record ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( cache.makeKey( szBytes, 8, &key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, sizeof( szBytes ), 1, &record ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( cache.store( artifact, key, szBytes, 16, 32, &record ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( cache.fetch( record, szBytes + 1, 16 ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( cache.erase( key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) >= 1 );
	return true;
}

// Competing owners in separate threads use distinct OS file handles. A shared
// cache key can publish one payload only, even when both start simultaneously.
static bool testCompetingWriters( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-script-cache-race-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	const std::string szRoot = root.string();
	const sirScriptCodeArtifact_t artifact = { 7, 91, 3, 77, 0x1234, 87, 33 };
	const uint8_t szFirst[64] = { 1 };
	const uint8_t szSecond[64] = { 2 };
	const char szRecipe[] = "racing-two-writers-same-key";
	sirScriptCacheKey_t key = {};
	SIR_TEST_CHECK( SIRScriptArtifactCache::makeKey( szRecipe, sizeof( szRecipe ) - 1, &key ) == SIR_SCRIPT_OK );

	std::atomic<uint32_t> nReady{ 0 };
	std::atomic<bool> bStart{ false };
	sirScriptResult_t szResults[2] = { SIR_SCRIPT_BACKEND_FAILURE, SIR_SCRIPT_BACKEND_FAILURE };
	sirScriptCachedArtifact_t szRecords[2] = {};
	std::thread szWorkers[2];
	for ( uint32_t iWorker = 0; iWorker < 2; ++iWorker ) {
		szWorkers[iWorker] = std::thread( [&, iWorker] {
			SIRScriptArtifactCache writer;
			const sirScriptResult_t eConfigured = writer.configure( szRoot.c_str(), 1024 );
			nReady.fetch_add( 1, std::memory_order_release );
			while ( !bStart.load( std::memory_order_acquire ) ) {
				std::this_thread::yield();
			}
			if ( eConfigured != SIR_SCRIPT_OK ) {
				szResults[iWorker] = eConfigured;
				return;
			}
			const uint8_t *pPayload = iWorker == 0 ? szFirst : szSecond;
			szResults[iWorker] = writer.store( artifact, key, pPayload, sizeof( szFirst ), 1, &szRecords[iWorker] );
		} );
	}
	while ( nReady.load( std::memory_order_acquire ) != 2 ) {
		std::this_thread::yield();
	}
	bStart.store( true, std::memory_order_release );
	for ( std::thread& worker : szWorkers ) {
		worker.join();
	}

	const uint32_t nWinners = ( szResults[0] == SIR_SCRIPT_OK ? 1U : 0U ) + ( szResults[1] == SIR_SCRIPT_OK ? 1U : 0U );
	SIR_TEST_CHECK( nWinners == 1 );
	const uint32_t iWinner = szResults[0] == SIR_SCRIPT_OK ? 0U : 1U;
	SIR_TEST_CHECK( szResults[1 - iWinner] == SIR_SCRIPT_TYPE_MISMATCH );
	SIRScriptArtifactCache verifier;
	SIR_TEST_CHECK( verifier.configure( szRoot.c_str(), 1024 ) == SIR_SCRIPT_OK );
	uint8_t szOutput[64] = {};
	SIR_TEST_CHECK( verifier.fetch( szRecords[iWinner], szOutput, sizeof( szOutput ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( szOutput, iWinner == 0 ? szFirst : szSecond, sizeof( szOutput ) ) == 0 );
	SIR_TEST_CHECK( verifier.erase( key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) >= 1 );
	return true;
}

int main( void )
{
	if ( !testHashVectors() || !testCache() || !testInvalidRecords() || !testCompetingWriters() ) {
		return 1;
	}
	puts( "scripting artifact cache: PASS" );
	return 0;
}
