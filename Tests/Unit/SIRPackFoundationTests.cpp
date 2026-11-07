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

#include <Resources/SIRPackCatalog.h>
#include <Tools/SIRPackCooker/SIRPackCooker.h>

#include <stdio.h>
#include <string.h>

#ifndef SIR_PACK_FIXTURE_DIR
#define SIR_PACK_FIXTURE_DIR "."
#endif

static bool check( bool bTest, const char *pszCase, uint32_t nLine ) noexcept
{
	if ( !bTest ) {
		fprintf( stderr, "SIRPack test FAILED: %s (line %u)\n", pszCase, nLine );
	}
	return bTest;
}

#define CHECK( x )                                                                                                                         \
	do {                                                                                                                                   \
		if ( !check( ( x ), #x, __LINE__ ) ) {                                                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirPackTestFixture_s
{
	uint8_t szCatalog[1024];
	uint8_t szShard0[256];
	uint8_t szShard1[256];
	uint8_t szScratch[256];
	sirPackCookResource_t resources[2];
	sirPackCookChunk_t chunks[3];
	sirPackDependency_t dependencies[1];
	sirPackCookShard_t shards[2];
	sirPackCookDesc_t desc;
} sirPackTestFixture_t;

static void makeFixture( sirPackTestFixture_t *pFixture ) noexcept
{
	static constexpr uint8_t s_szHello[] = { 'h', 'e', 'l', 'l', 'o' };
	static constexpr uint8_t s_szWorld[] = { 'w', 'o', 'r', 'l', 'd' };
	static constexpr uint8_t s_szXYZ[] = { 'x', 'y', 'z' };
	memset( pFixture, 0, sizeof( *pFixture ) );
	pFixture->resources[0].key = { 100, 1, 0 };
	pFixture->resources[0].nRevision = 7;
	pFixture->resources[0].nFormatId = 3;
	pFixture->resources[0].nChunkCount = 2;
	pFixture->resources[0].nDependencyCount = 1;
	pFixture->resources[0].nSchemaVersion = 1;
	pFixture->resources[0].nKind = SIR_RESOURCE_KIND_GEOMETRY;
	pFixture->resources[1].key = { 200, 1, 0 };
	pFixture->resources[1].nRevision = 4;
	pFixture->resources[1].nFormatId = 5;
	pFixture->resources[1].nFirstChunk = 2;
	pFixture->resources[1].nChunkCount = 1;
	pFixture->resources[1].nFirstDependency = 1;
	pFixture->resources[1].nSchemaVersion = 1;
	pFixture->resources[1].nKind = SIR_RESOURCE_KIND_FIELD;
	pFixture->chunks[0] = { 0, 0, s_szHello, sizeof( s_szHello ), 16 };
	pFixture->chunks[1] = { 1, 1, s_szWorld, sizeof( s_szWorld ), 32 };
	pFixture->chunks[2] = { 0, 1, s_szXYZ, sizeof( s_szXYZ ), 16 };
	pFixture->dependencies[0].key = pFixture->resources[1].key;
	pFixture->dependencies[0].eKind = SIR_RESOURCE_EDGE_KIND_REQUIRED;
	pFixture->shards[0] = { pFixture->szShard0, sizeof( pFixture->szShard0 ) };
	pFixture->shards[1] = { pFixture->szShard1, sizeof( pFixture->szShard1 ) };
	pFixture->desc.nPackageId = 0x1234567887654321ULL;
	pFixture->desc.nGeneration = 3;
	pFixture->desc.pResources = pFixture->resources;
	pFixture->desc.nResourceCount = 2;
	pFixture->desc.pChunks = pFixture->chunks;
	pFixture->desc.nChunkCount = 3;
	pFixture->desc.pDependencies = pFixture->dependencies;
	pFixture->desc.nDependencyCount = 1;
	pFixture->desc.pShards = pFixture->shards;
	pFixture->desc.nShardCount = 2;
	pFixture->desc.pScratch = pFixture->szScratch;
	pFixture->desc.nScratchBytes = sizeof( pFixture->szScratch );
}

static bool recalculateDigest( uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	if ( nBytes < SIR_PACK_HEADER_BYTES ) {
		return false;
	}
	sirPackCatalogDigest( pBytes, nBytes, pBytes + 88 );
	return true;
}

static bool readFile( const char *pszName, uint8_t *pBytes, uint32_t nCapacity, uint32_t *pRead ) noexcept
{
	char szPath[512];
	const int nChars = snprintf( szPath, sizeof( szPath ), "%s/%s", SIR_PACK_FIXTURE_DIR, pszName );
	if ( nChars <= 0 || ( size_t )nChars >= sizeof( szPath ) ) {
		return false;
	}
	FILE *pFile = fopen( szPath, "rb" );
	if ( pFile == nullptr ) {
		return false;
	}
	const size_t nRead = fread( pBytes, 1, nCapacity, pFile );
	const int nTail = fgetc( pFile );
	const int nClose = fclose( pFile );
	if ( nClose != 0 || nTail != EOF ) {
		return false;
	}
	*pRead = ( uint32_t )nRead;
	return true;
}

static bool writeFile( const char *pszName, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	char szPath[512];
	const int nChars = snprintf( szPath, sizeof( szPath ), "%s/%s", SIR_PACK_FIXTURE_DIR, pszName );
	if ( nChars <= 0 || ( size_t )nChars >= sizeof( szPath ) ) {
		return false;
	}
	FILE *pFile = fopen( szPath, "wb" );
	if ( pFile == nullptr ) {
		return false;
	}
	const size_t nWrite = fwrite( pBytes, 1, ( size_t )nBytes, pFile );
	return fclose( pFile ) == 0 && nWrite == nBytes;
}

static bool testSha256( void ) noexcept
{
	static constexpr uint8_t s_szABC[] = { 'a', 'b', 'c' };
	static constexpr uint8_t s_szExpected[32] = { 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
	                                              0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
	                                              0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad };
	uint8_t szActual[32] = {};
	sirPackSha256( s_szABC, sizeof( s_szABC ), szActual );
	CHECK( memcmp( s_szExpected, szActual, 32 ) == 0 );
	CHECK( sirPackRead16( ( const uint8_t * )"\x78\x56" ) == 0x5678 );
	uint8_t szWire[8] = {};
	sirPackWrite64( szWire, 0x0123456789abcdefULL );
	CHECK( sirPackRead64( szWire ) == 0x0123456789abcdefULL );
	CHECK( szWire[0] == 0xef && szWire[7] == 0x01 );
	return true;
}

static bool testRoundtrip( bool bWriteGoldens ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	CHECK( SIRPackCooker::requiredCatalogBytes( sample.desc ) == 632 );
	CHECK( SIRPackCooker::requiredScratch( sample.desc ) <= sizeof( sample.szScratch ) );
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_OK );
	const uint64_t nCatalogSize = SIRPackCooker::requiredCatalogBytes( sample.desc );
	SIRPackCatalog catalog;
	CHECK( catalog.open( sample.szCatalog, nCatalogSize, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_OK );
	CHECK( catalog.getResourceCount() == 2 && catalog.getChunkCount() == 3 && catalog.getShardCount() == 2 &&
	       catalog.getGeneration() == 3 && catalog.getPackageId() == sample.desc.nPackageId && catalog.getDependencyCount() == 1 );
	sirPackResource_t resource = {};
	CHECK( catalog.findResource( sample.resources[0].key, &resource ) == SIR_PACK_OK );
	CHECK( resource.nRevision == 7 && resource.nLogicalBytes == 10 && resource.nChunkCount == 2 );
	CHECK( catalog.findResource( sample.resources[1].key, &resource ) == SIR_PACK_OK );
	CHECK( resource.nRevision == 4 && resource.nLogicalBytes == 3 );
	CHECK( catalog.findResource( { 999, 1, 0 }, &resource ) == SIR_PACK_NOT_FOUND );
	CHECK( catalog.resourceAt( 2, &resource ) == SIR_PACK_INVALID_ARGUMENT );
	sirPackDependency_t edge = {};
	CHECK( catalog.dependencyAt( 0, &edge ) == SIR_PACK_OK && edge.key.m_nAssetId == 200 );
	CHECK( catalog.dependencyAt( 1, &edge ) == SIR_PACK_INVALID_ARGUMENT );
	for ( uint32_t i = 0; i < 2; ++i ) {
		sirPackShard_t shard = {};
		CHECK( catalog.shardAt( i, &shard ) == SIR_PACK_OK );
		CHECK( catalog.validateShard( i, sample.shards[i].pBytes, shard.nBytes ) == SIR_PACK_OK );
	}
	for ( uint32_t i = 0; i < 3; ++i ) {
		sirPackChunk_t chunk = {};
		CHECK( catalog.chunkAt( i, &chunk ) == SIR_PACK_OK );
		CHECK( chunk.eCodec == SIR_PACK_CODEC_NONE && chunk.nDecodedBytes == sample.chunks[i].nBytes );
		CHECK( catalog.verifyDecodedChunk( i, sample.chunks[i].pBytes, sample.chunks[i].nBytes ) == SIR_PACK_OK );
		CHECK( catalog.verifyDecodedChunk( i, sample.chunks[i].pBytes, 0 ) == SIR_PACK_INVALID_FORMAT );
		CHECK( catalog.verifyDecodedChunk( i, sample.shards[chunk.nShardIndex].pBytes + chunk.nOffset, chunk.nDecodedBytes ) ==
		       SIR_PACK_OK );
	}
	CHECK( catalog.chunkAt( 3, nullptr ) == SIR_PACK_INVALID_ARGUMENT );
	if ( bWriteGoldens ) {
		CHECK( writeFile( "foundation-v1.sirm", sample.szCatalog, nCatalogSize ) );
		CHECK( writeFile( "foundation-v1-0.sirp", sample.szShard0, sirPackRead64( sample.szShard0 + 32 ) ) );
		CHECK( writeFile( "foundation-v1-1.sirp", sample.szShard1, sirPackRead64( sample.szShard1 + 32 ) ) );
	} else {
		uint8_t szGolden[1024] = {};
		uint32_t nRead = 0;
		CHECK( readFile( "foundation-v1.sirm", szGolden, sizeof( szGolden ), &nRead ) );
		CHECK( nRead == nCatalogSize && memcmp( sample.szCatalog, szGolden, nRead ) == 0 );
		CHECK( readFile( "foundation-v1-0.sirp", szGolden, sizeof( szGolden ), &nRead ) );
		CHECK( nRead == sirPackRead64( sample.szShard0 + 32 ) && memcmp( sample.szShard0, szGolden, nRead ) == 0 );
		CHECK( readFile( "foundation-v1-1.sirp", szGolden, sizeof( szGolden ), &nRead ) );
		CHECK( nRead == sirPackRead64( sample.szShard1 + 32 ) && memcmp( sample.szShard1, szGolden, nRead ) == 0 );
	}
	sirPackTestFixture_t repeat = {};
	makeFixture( &repeat );
	CHECK( SIRPackCooker::build( repeat.desc, repeat.szCatalog, sizeof( repeat.szCatalog ) ) == SIR_PACK_OK );
	CHECK( memcmp( sample.szCatalog, repeat.szCatalog, nCatalogSize ) == 0 );
	CHECK( memcmp( sample.szShard0, repeat.szShard0, sirPackRead64( sample.szShard0 + 32 ) ) == 0 );
	CHECK( memcmp( sample.szShard1, repeat.szShard1, sirPackRead64( sample.szShard1 + 32 ) ) == 0 );
	catalog.reset();
	CHECK( !catalog.isOpen() && catalog.getResourceCount() == 0 );
	return true;
}

static bool testMalformedCatalog( void ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_OK );
	SIRPackCatalog catalog;
	const uint64_t nBytes = SIRPackCooker::requiredCatalogBytes( sample.desc );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_OK );
	uint8_t szOriginal[1024] = {};
	memcpy( szOriginal, sample.szCatalog, nBytes );
	CHECK( catalog.open( nullptr, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( catalog.open( sample.szCatalog, 8, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_TRUNCATED );
	CHECK( catalog.open( sample.szCatalog, nBytes, nullptr, sizeof( sample.szScratch ) ) == SIR_PACK_OUT_OF_CAPACITY );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, 1 ) == SIR_PACK_OUT_OF_CAPACITY );
	for ( uint32_t i = 0; i < 4; ++i ) {
		memcpy( sample.szCatalog, szOriginal, nBytes );
		const uint32_t nOffsets[4] = { 0, 4, 8, 12 };
		sample.szCatalog[nOffsets[i]] ^= 0x20;
		CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) != SIR_PACK_OK );
	}
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sample.szCatalog[4] = 2;
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_UNSUPPORTED_VERSION );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sample.szCatalog[88] ^= 0x01;
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_BAD_DIGEST );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite64( sample.szCatalog + 64, UINT64_MAX );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_INVALID_FORMAT );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite32( sample.szCatalog + 48, UINT32_MAX );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_INVALID_FORMAT );
	const uint64_t nResourceOff = sirPackRead64( szOriginal + 64 );
	const uint64_t nChunkOff = sirPackRead64( szOriginal + 72 );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	memcpy( sample.szCatalog + nResourceOff + SIR_PACK_RESOURCE_RECORD_BYTES, sample.szCatalog + nResourceOff, 16 );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_INVALID_FORMAT );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite64( sample.szCatalog + nChunkOff + 8, UINT64_MAX );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_INVALID_FORMAT );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite16( sample.szCatalog + nChunkOff + 64, SIR_PACK_CODEC_ZSTD );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_UNSUPPORTED_CODEC );
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite64( sample.szCatalog + nResourceOff + 24, UINT64_MAX );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_INVALID_FORMAT );
	// Previous valid view remains in effect on every failing open(); its source
	// backing is restored before dereferencing the retained metadata.
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackResource_t resource = {};
	CHECK( catalog.findResource( { 100, 1, 0 }, &resource ) == SIR_PACK_OK && resource.nRevision == 7 );
	return true;
}

static bool testShardAndChunkIdentity( void ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_OK );
	SIRPackCatalog catalog;
	const uint64_t nBytes = SIRPackCooker::requiredCatalogBytes( sample.desc );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_OK );
	const uint64_t nShardBytes = sirPackRead64( sample.szShard0 + 32 );
	CHECK( catalog.validateShard( 0, sample.szShard0, nShardBytes - 1 ) == SIR_PACK_TRUNCATED );
	CHECK( catalog.validateShard( 2, sample.szShard0, nShardBytes ) == SIR_PACK_INVALID_ARGUMENT );
	sample.szShard0[24] ^= 1;
	CHECK( catalog.validateShard( 0, sample.szShard0, nShardBytes ) == SIR_PACK_STALE_SOURCE );
	sample.szShard0[24] ^= 1;
	sample.szShard0[64] ^= 1;
	CHECK( catalog.validateShard( 0, sample.szShard0, nShardBytes ) == SIR_PACK_BAD_DIGEST );
	CHECK( catalog.verifyDecodedChunk( 0, sample.szShard0 + 64, 5 ) == SIR_PACK_BAD_DIGEST );
	return true;
}

static bool testFailedBuildIsAtomic( void ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	memset( sample.szCatalog, 0xa5, sizeof( sample.szCatalog ) );
	memset( sample.szShard0, 0x5a, sizeof( sample.szShard0 ) );
	const sirPackCookResource_t original = sample.resources[1];
	sample.resources[1].key = sample.resources[0].key;
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_INVALID_FORMAT );
	CHECK( sample.szCatalog[0] == 0xa5 && sample.szShard0[0] == 0x5a );
	sample.resources[1] = original;
	sample.chunks[0].nBytes = UINT64_MAX;
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( sample.szCatalog[0] == 0xa5 && sample.szShard0[0] == 0x5a );
	makeFixture( &sample );
	memset( sample.szCatalog, 0xa5, sizeof( sample.szCatalog ) );
	memset( sample.szShard0, 0x5a, sizeof( sample.szShard0 ) );
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, 1 ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( sample.szCatalog[0] == 0xa5 && sample.szShard0[0] == 0x5a );
	const sirPackCookDesc_t originalDesc = sample.desc;
	sample.desc.pScratch = sample.szCatalog;
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( sample.szCatalog[0] == 0xa5 );
	sample.desc = originalDesc;
	sample.chunks[0].pBytes = sample.szShard0;
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_INVALID_ARGUMENT );
	CHECK( sample.szShard0[0] == 0x5a && sample.szCatalog[0] == 0xa5 );
	return true;
}

static bool testCycleRejection( void ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	sirPackDependency_t szDeps[2] = {};
	szDeps[0].key = sample.resources[1].key;
	szDeps[1].key = sample.resources[0].key;
	szDeps[0].eKind = SIR_RESOURCE_EDGE_KIND_REQUIRED;
	szDeps[1].eKind = SIR_RESOURCE_EDGE_KIND_REQUIRED;
	sample.resources[1].nDependencyCount = 1;
	sample.desc.pDependencies = szDeps;
	sample.desc.nDependencyCount = 2;
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_CYCLIC_DEPENDENCY );
	return true;
}

static bool testCatalogSemanticValidation( void ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_OK );
	const uint64_t nBytes = SIRPackCooker::requiredCatalogBytes( sample.desc );
	uint8_t szOriginal[1024] = {};
	memcpy( szOriginal, sample.szCatalog, nBytes );
	const uint64_t nShard = sirPackRead64( szOriginal + 56 );
	const uint64_t nResource = sirPackRead64( szOriginal + 64 );
	const uint64_t nChunk = sirPackRead64( szOriginal + 72 );
	const uint64_t nDep = sirPackRead64( szOriginal + 80 );
	// Malformed but re-signed catalog metadata must still fail semantic validation.
	typedef struct sirPackPoison_s
	{
		uint64_t nOffset;
		uint64_t nValue;
		uint8_t nWidth;
	} sirPackPoison_t;
	const sirPackPoison_t szPoison[] = { { 6, 0, 2 },
	                                     { 8, 0, 4 },
	                                     { 12, 1, 4 },
	                                     { 16, 0, 8 },
	                                     { 24, 0, 8 },
	                                     { 32, UINT64_MAX, 8 },
	                                     { 40, 4097, 4 },
	                                     { 44, 1048577, 4 },
	                                     { 48, 4194305, 4 },
	                                     { 52, 4194305, 4 },
	                                     { 56, 0, 8 },
	                                     { 64, 0, 8 },
	                                     { 72, 0, 8 },
	                                     { 80, 0, 8 },
	                                     { 120, 1, 1 },
	                                     { nShard, 9, 4 },
	                                     { nShard + 4, 1, 4 },
	                                     { nShard + 8, 0, 8 },
	                                     { nShard + 48, 1, 1 },
	                                     { nResource, 0, 8 },
	                                     { nResource + 16, 0, 8 },
	                                     { nResource + 24, 1, 8 },
	                                     { nResource + 36, 1, 4 },
	                                     { nResource + 40, 0, 4 },
	                                     { nResource + 44, 1, 4 },
	                                     { nResource + 48, 100, 4 },
	                                     { nResource + 52, 0, 2 },
	                                     { nResource + 54, 255, 1 },
	                                     { nResource + 55, 1, 1 },
	                                     { nChunk + 4, 99, 4 },
	                                     { nChunk + 8, 0, 8 },
	                                     { nChunk + 16, 0, 8 },
	                                     { nChunk + 24, 0, 8 },
	                                     { nChunk + 64, SIR_PACK_CODEC_LZ4, 2 },
	                                     { nChunk + 66, 2, 2 },
	                                     { nChunk + 68, 3, 4 },
	                                     { nChunk + 72, 1, 4 },
	                                     { nChunk + 76, 1, 4 },
	                                     { nChunk + SIR_PACK_CHUNK_RECORD_BYTES, 0, 4 },
	                                     { nDep, 0, 8 },
	                                     { nDep + 16, 255, 1 },
	                                     { nDep + 17, 1, 1 } };
	for ( uint32_t i = 0; i < sizeof( szPoison ) / sizeof( szPoison[0] ); ++i ) {
		memcpy( sample.szCatalog, szOriginal, nBytes );
		const sirPackPoison_t& poison = szPoison[i];
		if ( poison.nWidth == 8 ) {
			sirPackWrite64( sample.szCatalog + poison.nOffset, poison.nValue );
		} else if ( poison.nWidth == 4 ) {
			sirPackWrite32( sample.szCatalog + poison.nOffset, ( uint32_t )poison.nValue );
		} else if ( poison.nWidth == 2 ) {
			sirPackWrite16( sample.szCatalog + poison.nOffset, ( uint16_t )poison.nValue );
		} else {
			sample.szCatalog[poison.nOffset] = ( uint8_t )poison.nValue;
		}
		CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
		SIRPackCatalog catalog;
		const sirPackResult_t eResult = catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) );
		if ( eResult == SIR_PACK_OK ) {
			fprintf( stderr, "SIRPack semantic poison accepted at index %u offset %llu\n", i, ( unsigned long long )poison.nOffset );
			return false;
		}
	}
	// Full-body checksum is not enough: graph cycles are checked after hashing.
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite64( sample.szCatalog + nDep, sample.resources[0].key.m_nAssetId );
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	SIRPackCatalog catalog;
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_CYCLIC_DEPENDENCY );
	// Unresolved optional/streamed dependencies are legal metadata references.
	memcpy( sample.szCatalog, szOriginal, nBytes );
	sirPackWrite64( sample.szCatalog + nDep, 999 );
	sample.szCatalog[nDep + 16] = SIR_RESOURCE_EDGE_KIND_OPTIONAL;
	CHECK( recalculateDigest( sample.szCatalog, nBytes ) );
	CHECK( catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) ) == SIR_PACK_OK );
	return true;
}

static bool testCookerArgumentBoundaries( void ) noexcept
{
	// Each case has an independent fixture and checks that no output is written
	// during failed admission. The production producer is entirely caller-owned.
#define REJECT_COOK( mutation )                                                                                                            \
	do {                                                                                                                                   \
		sirPackTestFixture_t sample = {};                                                                                                  \
		makeFixture( &sample );                                                                                                            \
		memset( sample.szCatalog, 0xa5, sizeof( sample.szCatalog ) );                                                                      \
		memset( sample.szShard0, 0x5a, sizeof( sample.szShard0 ) );                                                                        \
		mutation;                                                                                                                          \
		CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) != SIR_PACK_OK );                         \
		CHECK( sample.szCatalog[0] == 0xa5 && sample.szShard0[0] == 0x5a );                                                                \
	} while ( 0 )
	REJECT_COOK( sample.desc.nPackageId = 0 );
	REJECT_COOK( sample.desc.nGeneration = 0 );
	REJECT_COOK( sample.desc.nShardCount = 0 );
	REJECT_COOK( sample.desc.nResourceCount = 0 );
	REJECT_COOK( sample.desc.nChunkCount = 0 );
	REJECT_COOK( sample.desc.pResources = nullptr );
	REJECT_COOK( sample.desc.pChunks = nullptr );
	REJECT_COOK( sample.desc.pShards = nullptr );
	REJECT_COOK( sample.desc.pDependencies = nullptr );
	REJECT_COOK( sample.desc.pScratch = nullptr );
	REJECT_COOK( sample.desc.nScratchBytes = 0 );
	REJECT_COOK( sample.desc.nShardCount = SIR_PACK_MAX_SHARDS + 1 );
	REJECT_COOK( sample.desc.nResourceCount = SIR_PACK_MAX_RESOURCES + 1 );
	REJECT_COOK( sample.desc.nChunkCount = SIR_PACK_MAX_CHUNKS + 1 );
	REJECT_COOK( sample.desc.nDependencyCount = SIR_PACK_MAX_CHUNKS + 1 );
	REJECT_COOK( sample.shards[0].pBytes = nullptr );
	REJECT_COOK( sample.shards[0].nCapacity = 63 );
	REJECT_COOK( sample.shards[0].nCapacity = 64 );
	REJECT_COOK( sample.resources[0].key.m_nAssetId = 0 );
	REJECT_COOK( sample.resources[0].nRevision = 0 );
	REJECT_COOK( sample.resources[0].nSchemaVersion = 0 );
	REJECT_COOK( sample.resources[0].nKind = 255 );
	REJECT_COOK( sample.resources[1].key.m_nAssetId = 50 );
	REJECT_COOK( sample.resources[0].nChunkCount = 0 );
	REJECT_COOK( sample.resources[1].nFirstChunk = 1 );
	REJECT_COOK( sample.resources[1].nFirstDependency = 0 );
	REJECT_COOK( sample.resources[0].nChunkCount = 4 );
	REJECT_COOK( sample.resources[0].nDependencyCount = 2 );
	REJECT_COOK( sample.chunks[0].nShardIndex = 2 );
	REJECT_COOK( sample.chunks[0].pBytes = nullptr );
	REJECT_COOK( sample.chunks[0].nBytes = 0 );
	REJECT_COOK( sample.chunks[0].nBytes = SIR_PACK_MAX_DECODED_CHUNK_BYTES + 1 );
	REJECT_COOK( sample.chunks[0].nAlignment = 0 );
	REJECT_COOK( sample.chunks[0].nAlignment = 8192 );
	REJECT_COOK( sample.chunks[0].nAlignment = 3 );
	REJECT_COOK( sample.chunks[1].nChunkId = 0 );
	REJECT_COOK( sample.dependencies[0].key.m_nAssetId = 0 );
	REJECT_COOK( sample.dependencies[0].eKind = ( sirResourceEdgeKind_t )255 );
	REJECT_COOK( sample.dependencies[0].key.m_nAssetId = 999 );
	REJECT_COOK( sample.shards[1].pBytes = sample.szShard0 );
	REJECT_COOK( sample.desc.pScratch = sample.szCatalog );
	REJECT_COOK( sample.desc.pScratch = sample.szShard0 );
	REJECT_COOK( sample.desc.pScratch = sample.resources );
	REJECT_COOK( sample.chunks[0].pBytes = sample.szCatalog );
	REJECT_COOK( sample.chunks[0].pBytes = sample.szShard0 );
#undef REJECT_COOK
	return true;
}

static bool testCorruptionMutations( void ) noexcept
{
	sirPackTestFixture_t sample = {};
	makeFixture( &sample );
	CHECK( SIRPackCooker::build( sample.desc, sample.szCatalog, sizeof( sample.szCatalog ) ) == SIR_PACK_OK );
	uint8_t szOriginal[1024] = {};
	const uint64_t nBytes = SIRPackCooker::requiredCatalogBytes( sample.desc );
	memcpy( szOriginal, sample.szCatalog, nBytes );
	SIRPackCatalog catalog;
	uint32_t nSeed = 0x1337abcfU;
	for ( uint32_t i = 0; i < 256; ++i ) {
		memcpy( sample.szCatalog, szOriginal, nBytes );
		nSeed = nSeed * 1664525U + 1013904223U;
		const uint32_t iByte = ( uint32_t )( nSeed % nBytes );
		sample.szCatalog[iByte] ^= ( uint8_t )( 1U << ( ( i / 7 ) % 8 ) );
		const sirPackResult_t eResult = catalog.open( sample.szCatalog, nBytes, sample.szScratch, sizeof( sample.szScratch ) );
		CHECK( eResult != SIR_PACK_OK );
	}
	return true;
}

int main( int nArgs, char **ppszArgs )
{
	const bool bWrite = nArgs == 2 && strcmp( ppszArgs[1], "--write-golden" ) == 0;
	if ( !testSha256() || !testRoundtrip( bWrite ) || !testMalformedCatalog() || !testShardAndChunkIdentity() ||
	     !testFailedBuildIsAtomic() || !testCycleRejection() || !testCookerArgumentBoundaries() || !testCatalogSemanticValidation() ||
	     !testCorruptionMutations() ) {
		return 1;
	}
	return 0;
}
