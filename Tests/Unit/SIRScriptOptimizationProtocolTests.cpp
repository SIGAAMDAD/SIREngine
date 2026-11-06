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

#include <Scripting/Core/SIRScriptOptimizationProtocol.h>
#include <Scripting/Core/SIRScriptSHA256.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <stdio.h>
#include <string.h>

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testRoundtrip( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-optimizer-protocol-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	const std::string szRoot = root.string();
	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( cache.configure( szRoot.c_str(), 4096 ) == SIR_SCRIPT_OK );
	const sirScriptCodeArtifact_t artifact = { 41, 200, 12, 700, 0xbeef, 0x1234, 0xabcd };
	const char szInputs[] = "deterministic-source-tree+canonical-build-options+cpu+abi+frontend";
	sirScriptCacheKey_t key = {};
	SIR_TEST_CHECK( cache.makeKey( szInputs, sizeof( szInputs ) - 1, &key ) == SIR_SCRIPT_OK );
	sirScriptOptimizationRequest_t request = { 991, artifact, key, 501, SIR_SCRIPT_OPT_TARGET_CPU, 0 };
	uint8_t szRequestWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szRequestWire ) == SIR_SCRIPT_OK );
	sirScriptOptimizationRequest_t restored = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeRequest( szRequestWire, sizeof( szRequestWire ), &restored ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( restored.nJobId == request.nJobId && restored.nInputBytes == 501 &&
	                restored.artifact.nSchemaFingerprint == artifact.nSchemaFingerprint && restored.eTier == request.eTier );

	alignas( 64 ) uint8_t szOptimized[512] = {};
	for ( uint32_t iByte = 0; iByte < sizeof( szOptimized ); ++iByte ) {
		szOptimized[iByte] = ( uint8_t )( iByte * 11 + 7 );
	}
	sirScriptCachedArtifact_t cached = {};
	SIR_TEST_CHECK( cache.store( artifact, restored.key, szOptimized, sizeof( szOptimized ), 64, &cached ) == SIR_SCRIPT_OK );
	sirScriptOptimizationReply_t reply = { request.nJobId, SIR_SCRIPT_OK, cached };
	uint8_t szReplyWire[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeReply( reply, szReplyWire ) == SIR_SCRIPT_OK );
	sirScriptOptimizationReply_t result = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeReply( szReplyWire, sizeof( szReplyWire ), &result ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( restored, result ) == SIR_SCRIPT_OK );
	alignas( 64 ) uint8_t szReceived[512] = {};
	SIR_TEST_CHECK( cache.fetch( result.cached, szReceived, sizeof( szReceived ) ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memcmp( szReceived, szOptimized, sizeof( szReceived ) ) == 0 );

	result.nJobId = 992;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( restored, result ) == SIR_SCRIPT_TYPE_MISMATCH );
	result = reply;
	result.cached.artifact.nTargetId ^= 1;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( restored, result ) == SIR_SCRIPT_TYPE_MISMATCH );
	result = reply;
	result.cached.key.szDigest[0] ^= 1;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( restored, result ) == SIR_SCRIPT_TYPE_MISMATCH );

	SIR_TEST_CHECK( cache.erase( key ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) > 0 );
	return true;
}

static bool testProtocolRejections( void )
{
	const sirScriptCodeArtifact_t artifact = { 1, 2, 3, 4, 5, 6, 7 };
	sirScriptCacheKey_t key = {};
	SIR_TEST_CHECK( SIRScriptArtifactCache::makeKey( "recipe", 6, &key ) == SIR_SCRIPT_OK );
	sirScriptOptimizationRequest_t request = { 1, artifact, key, 12, SIR_SCRIPT_OPT_BASELINE, 0 };
	uint8_t szRequest[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	uint8_t szReply[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szRequest ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeRequest( szRequest, sizeof( szRequest ) - 1, &request ) ==
	                SIR_SCRIPT_INVALID_ARGUMENT );
	const uint64_t nOldJob = request.nJobId;
	szRequest[24] ^= 1;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeRequest( szRequest, sizeof( szRequest ), &request ) ==
	                SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( request.nJobId == nOldJob );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szRequest ) == SIR_SCRIPT_OK );
	request.nFlags = 1;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szRequest ) == SIR_SCRIPT_INVALID_ARGUMENT );
	request.nFlags = 0;
	request.eTier = ( sirScriptOptimizationTier_t )99;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szRequest ) == SIR_SCRIPT_INVALID_ARGUMENT );
	request.eTier = SIR_SCRIPT_OPT_BASELINE;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeRequest( request, szRequest ) == SIR_SCRIPT_OK );

	sirScriptOptimizationReply_t failed = { 1, SIR_SCRIPT_BACKEND_FAILURE, {} };
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeReply( failed, szReply ) == SIR_SCRIPT_OK );
	sirScriptOptimizationReply_t result = {};
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeReply( szReply, sizeof( szReply ), &result ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::validateCompletion( request, result ) == SIR_SCRIPT_BACKEND_FAILURE );
	szReply[15] ^= 1;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeReply( szReply, sizeof( szReply ), &result ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeReply( failed, szReply ) == SIR_SCRIPT_OK );
	// Even a correctly re-hashed wire record cannot introduce unknown status codes.
	szReply[24] = 0;
	szReply[25] = 1;
	SIRScriptSHA256::digest( szReply, sizeof( szReply ) - 32, szReply + sizeof( szReply ) - 32 );
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::decodeReply( szReply, sizeof( szReply ), &result ) == SIR_SCRIPT_TYPE_MISMATCH );
	return true;
}

int main( void )
{
	if ( !testRoundtrip() || !testProtocolRejections() ) {
		return 1;
	}
	puts( "scripting optimization protocol: PASS" );
	return 0;
}
