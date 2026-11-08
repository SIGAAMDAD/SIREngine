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
#include <Tools/SIRPackCooker/SIRPackShippingGuard.h>

#include <stdio.h>
#include <string.h>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Shipping guard failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                                    \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

int main( void )
{
	const uint8_t cleanMaterial[] = "albedo normal roughness";
	const uint8_t poisoned[] = "color:/authoring/rock-height.pgm:sampler;payload";
	const uint8_t sourceImage[] = "P2\n2 2\n255\n0 255\n255 0\n";
	const char sourcePath[] = "/authoring/rock-height.pgm";
	const char recipeToken[] = "sampler";
	sirPackCookResource_t resources[3] = {};
	resources[0].key = { 101, 11, 0 };
	resources[0].nRevision = 1;
	resources[0].nFormatId = 3;
	resources[0].nChunkCount = 1;
	resources[0].nDependencyCount = 1;
	resources[0].nSchemaVersion = 1;
	resources[0].nKind = SIR_RESOURCE_KIND_GEOMETRY;
	resources[1].key = { 102, 12, 0 };
	resources[1].nRevision = 1;
	resources[1].nFormatId = 4;
	resources[1].nFirstChunk = 1;
	resources[1].nChunkCount = 1;
	resources[1].nFirstDependency = 1;
	resources[1].nDependencyCount = 1;
	resources[1].nSchemaVersion = 1;
	resources[1].nKind = SIR_RESOURCE_KIND_FIELD;
	resources[2].key = { 103, 13, 0 };
	resources[2].nRevision = 1;
	resources[2].nFormatId = 5;
	resources[2].nFirstChunk = 2;
	resources[2].nChunkCount = 1;
	resources[2].nFirstDependency = 2;
	resources[2].nSchemaVersion = 1;
	resources[2].nKind = SIR_RESOURCE_KIND_FIELD;
	sirPackDependency_t edges[2] = { { { 102, 12, 0 }, SIR_RESOURCE_EDGE_KIND_REQUIRED },
	                                 { { 103, 13, 0 }, SIR_RESOURCE_EDGE_KIND_REQUIRED } };
	sirPackCookChunk_t chunks[3] = { { 1, 0, cleanMaterial, sizeof( cleanMaterial ), 16 },
	                                 { 2, 0, cleanMaterial, sizeof( cleanMaterial ), 16 },
	                                 { 3, 0, cleanMaterial, sizeof( cleanMaterial ), 16 } };
	uint8_t shardBytes[1024] = {};
	uint8_t catalog[1024] = {};
	uint8_t scratch[256] = {};
	sirPackCookShard_t shard = { shardBytes, sizeof( shardBytes ) };
	sirPackCookDesc_t desc = {};
	desc.nPackageId = 11;
	desc.nGeneration = 1;
	desc.pResources = resources;
	desc.nResourceCount = 3;
	desc.pChunks = chunks;
	desc.nChunkCount = 3;
	desc.pDependencies = edges;
	desc.nDependencyCount = 2;
	desc.pShards = &shard;
	desc.nShardCount = 1;
	desc.pScratch = scratch;
	desc.nScratchBytes = sizeof( scratch );
	const SIRResourceKey sourceKey = { 9001, 71, 0 };
	const uint32_t sourceType = 71;
	const sirPackShippingPattern_t patterns[] = { { ( const uint8_t * )sourcePath, sizeof( sourcePath ) - 1 },
	                                              { sourceImage, sizeof( sourceImage ) - 1 },
	                                              { ( const uint8_t * )recipeToken, sizeof( recipeToken ) - 1 } };
	sirPackShippingPolicy_t policy = { &sourceKey, 1, &sourceType, 1, patterns, 3 };
	CHECK( SIRPackShippingGuard::build( desc, policy, catalog, sizeof( catalog ) ) == SIR_PACK_OK );
	// Normal material dependencies remain legal, even with material textures.
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_OK );
	const sirPackShippingPattern_t invalid = { nullptr, 1 };
	policy.pForbiddenPatterns = &invalid;
	policy.nForbiddenPatterns = 1;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_ARGUMENT );
	policy.pForbiddenPatterns = patterns;
	policy.nForbiddenPatterns = 3;
	policy.pForbiddenKeys = nullptr;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_ARGUMENT );
	policy.pForbiddenKeys = &sourceKey;
	// The entire transitive dependency closure of a shipped geometry resource
	// is checked. A source reference behind a material still fails.
	edges[1].key = sourceKey;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	edges[1].key = resources[2].key;
	resources[2].key = sourceKey;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	resources[2].key = { 103, 13, 0 };
	resources[2].key.m_nTypeId = sourceType;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	resources[2].key.m_nTypeId = 13;
	edges[1].eKind = ( sirResourceEdgeKind_t )3;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	edges[1].eKind = SIR_RESOURCE_EDGE_KIND_REQUIRED;
	resources[1].nFirstDependency = 999;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	resources[1].nFirstDependency = 1;
	chunks[1].pBytes = poisoned;
	chunks[1].nBytes = sizeof( poisoned );
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	chunks[1].pBytes = sourceImage;
	chunks[1].nBytes = sizeof( sourceImage );
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_FORMAT );
	chunks[1].pBytes = cleanMaterial;
	chunks[1].nBytes = sizeof( cleanMaterial );
	chunks[1].pBytes = nullptr;
	CHECK( SIRPackShippingGuard::validate( desc, policy ) == SIR_PACK_INVALID_ARGUMENT );
	chunks[1].pBytes = cleanMaterial;
	// Invalid packages never touch an existing valid output or its shard.
	uint8_t baselineCatalog[1024];
	uint8_t baselineShard[1024];
	memcpy( baselineCatalog, catalog, sizeof( catalog ) );
	memcpy( baselineShard, shardBytes, sizeof( shardBytes ) );
	edges[1].key = sourceKey;
	CHECK( SIRPackShippingGuard::build( desc, policy, catalog, sizeof( catalog ) ) == SIR_PACK_INVALID_FORMAT );
	CHECK( memcmp( catalog, baselineCatalog, sizeof( catalog ) ) == 0 );
	CHECK( memcmp( shardBytes, baselineShard, sizeof( shardBytes ) ) == 0 );
	puts( "SIRPack shipping-source exclusion checks passed" );
	return 0;
}
