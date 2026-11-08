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

#include <Mesh/SIRMeshPackSchema.h>
#include <cstdio>
#include <cstring>

#define TEST( x )                                                                                                                          \
	do {                                                                                                                                   \
		if ( !( x ) ) {                                                                                                                    \
			fprintf( stderr, "failed %s:%d %s\n", __FILE__, __LINE__, #x );                                                                \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static void makePage( uint8_t *p )
{
	memset( p, 0, 111 );
	sirPackWrite32( p, 1 );
	sirPackWrite16( p + 4, 3 );
	sirPackWrite16( p + 6, 1 );
	// Three distinct local vertices; axis-aligned bounds may be zero.
	p[50] = 1;
	p[74] = 1;
	p[98] = 1;
	p[108] = 0;
	p[109] = 1;
	p[110] = 2;
}

int main()
{
	uint8_t rootPayload[111] = {};
	uint8_t leafPayload[111] = {};
	makePage( rootPayload );
	makePage( leafPayload );
	uint8_t digest[32] = {};
	sirMeshPackPage_t pages[2] = {};
	pages[0].nParent = SIR_MESH_PACK_NO_PARENT;
	pages[0].nFirstCluster = 0;
	pages[0].nClusterCount = 1;
	pages[0].nChunkId = 1;
	pages[0].nByteCount = sizeof( rootPayload );
	sirPackSha256( rootPayload, sizeof( rootPayload ), digest );
	memcpy( pages[0].szDigest, digest, 16 );
	pages[1].nParent = 0;
	pages[1].nFirstCluster = 1;
	pages[1].nClusterCount = 1;
	pages[1].nChunkId = 2;
	pages[1].nByteCount = sizeof( leafPayload );
	sirPackSha256( leafPayload, sizeof( leafPayload ), digest );
	memcpy( pages[1].szDigest, digest, 16 );
	sirMeshPackGroup_t groups[2] = {};
	groups[0] = { SIR_MESH_PACK_NO_PARENT, 0, 1, 1, 1, 0, 1, 0, 0.5f };
	groups[1] = { 0, 1, 1, 0, 0, 0, 0, 1, 0.0f };
	sirMeshPackCluster_t clusters[2] = {};
	clusters[0] = { 0, 0, 3, 1, 4, sizeof( rootPayload ) - 4, 0.5f };
	clusters[1] = { 1, 1, 3, 1, 4, sizeof( leafPayload ) - 4, 0.0f };
	sirMeshPackCollision_t collision = { 1, SIR_MESH_PACK_COLLISION_SOURCE_FLAGS, 3, 3, 3, 9 };
	sirMeshPackDesc_t desc = { { 123, 9, 1, 0 }, groups, clusters, pages, &collision, 2, 2, 2, 1, 3, 1 };
	uint8_t buffer[1024] = {};
	const uint64_t bytes = SIRMeshPackSchema::requiredBytes( desc );
	TEST( bytes == 128 + 2 * 48 + 2 * 48 + 2 * 48 + 48 );
	TEST( SIRMeshPackSchema::write( desc, buffer, bytes ) == SIR_MESH_PACK_OK );
	SIRMeshPackSchema reader;
	TEST( reader.open( buffer, bytes ) == SIR_MESH_PACK_OK );
	TEST( reader.getKey().nRevision == 9 );
	sirPackResource_t resource = {};
	resource.key = { 123, 103, 1 };
	resource.nRevision = 9;
	resource.nFormatId = SIR_MESH_PACK_FORMAT_ID;
	resource.nSchemaVersion = SIR_MESH_PACK_VERSION;
	resource.nKind = SIR_RESOURCE_KIND_GEOMETRY;
	resource.nChunkCount = 4;
	TEST( reader.validateResource( resource ) == SIR_MESH_PACK_OK );
	++resource.nRevision;
	TEST( reader.validateResource( resource ) == SIR_MESH_PACK_INVALID_FORMAT );
	--resource.nRevision;
	resource.key.m_nAssetId = 124;
	TEST( reader.validateResource( resource ) == SIR_MESH_PACK_INVALID_FORMAT );
	resource.key.m_nAssetId = 123;
	resource.nFormatId = 0;
	TEST( reader.validateResource( resource ) == SIR_MESH_PACK_INVALID_FORMAT );
	resource.nFormatId = SIR_MESH_PACK_FORMAT_ID;
	resource.nChunkCount = 3;
	TEST( reader.validateResource( resource ) == SIR_MESH_PACK_INVALID_FORMAT );
	resource.nChunkCount = 4;
	sirPackChunk_t packedChunk = {};
	packedChunk.nChunkId = 0;
	packedChunk.nDecodedBytes = bytes;
	sirPackSha256( buffer, bytes, packedChunk.szDigest );
	TEST( reader.validateChunk( 0, packedChunk ) == SIR_MESH_PACK_OK );
	packedChunk.nDecodedBytes = bytes + 1;
	TEST( reader.validateChunk( 0, packedChunk ) == SIR_MESH_PACK_BAD_DIGEST );
	packedChunk.nDecodedBytes = bytes;
	packedChunk.nChunkId = 1;
	packedChunk.nDecodedBytes = sizeof( rootPayload );
	sirPackSha256( rootPayload, sizeof( rootPayload ), packedChunk.szDigest );
	TEST( reader.validateChunk( 1, packedChunk ) == SIR_MESH_PACK_OK );
	packedChunk.szDigest[0] ^= 0x40;
	TEST( reader.validateChunk( 1, packedChunk ) == SIR_MESH_PACK_BAD_DIGEST );
	packedChunk.szDigest[0] ^= 0x40;
	packedChunk.nChunkId = 2;
	TEST( reader.validateChunk( 2, packedChunk ) == SIR_MESH_PACK_OK );
	packedChunk.nChunkId = 3;
	packedChunk.nDecodedBytes = 68;
	TEST( reader.validateChunk( 3, packedChunk ) == SIR_MESH_PACK_OK );
	packedChunk.nDecodedBytes = 67;
	TEST( reader.validateChunk( 3, packedChunk ) == SIR_MESH_PACK_INVALID_FORMAT );
	TEST( reader.validateChunk( 4, packedChunk ) == SIR_MESH_PACK_INVALID_ARGUMENT );
	sirMeshPackGroup_t recovered = {};
	TEST( reader.groupAt( 1, &recovered ) == SIR_MESH_PACK_OK && recovered.nParent == 0 );
	TEST( reader.verifyPage( 0, rootPayload, sizeof( rootPayload ) ) == SIR_MESH_PACK_OK );
	TEST( reader.verifyPage( 1, leafPayload, sizeof( leafPayload ) ) == SIR_MESH_PACK_OK );
	leafPayload[110] = 3;
	TEST( reader.verifyPage( 1, leafPayload, sizeof( leafPayload ) ) == SIR_MESH_PACK_BAD_DIGEST );
	leafPayload[110] = 2;
	TEST( reader.verifyPage( 1, rootPayload, sizeof( rootPayload ) ) == SIR_MESH_PACK_OK );
	TEST( reader.open( buffer, bytes - 1 ) == SIR_MESH_PACK_TRUNCATED );
	TEST( !reader.isOpen() );
	TEST( reader.open( buffer, bytes ) == SIR_MESH_PACK_OK );
	buffer[7] ^= 1;
	TEST( reader.open( buffer, bytes ) == SIR_MESH_PACK_INVALID_FORMAT );
	buffer[7] ^= 1;
	buffer[4] += 1;
	TEST( reader.open( buffer, bytes ) == SIR_MESH_PACK_UNSUPPORTED_VERSION );
	buffer[4] -= 1;
	buffer[128 + 8] ^= 1;
	TEST( reader.open( buffer, bytes ) == SIR_MESH_PACK_BAD_DIGEST );
	buffer[128 + 8] ^= 1;
	TEST( reader.open( buffer, bytes ) == SIR_MESH_PACK_OK );
	sirMeshPackVertex_t decoded = {};
	sirMeshPackVertexQuery_t query = { rootPayload, sizeof( rootPayload ), 0, 0, 1 };
	TEST( reader.decodeVertex( query, &decoded ) == SIR_MESH_PACK_OK );
	TEST( decoded.szNormal[2] > 0.99f && decoded.szTangent[3] == 1.0f );
	query.nVertex = 3;
	TEST( reader.decodeVertex( query, &decoded ) == SIR_MESH_PACK_INVALID_ARGUMENT );
	uint8_t collisionBytes[68] = {};
	sirPackWrite32( collisionBytes, 0x43524953U );
	sirPackWrite32( collisionBytes + 4, 3 );
	sirPackWrite32( collisionBytes + 8, 3 );
	sirPackWrite32( collisionBytes + 12, 1 );
	sirPackWrite32( collisionBytes + 52, 0 );
	sirPackWrite32( collisionBytes + 56, 1 );
	sirPackWrite32( collisionBytes + 60, 2 );
	sirPackWrite32( collisionBytes + 64, 18 );
	TEST( reader.verifyCollision( 0, collisionBytes, sizeof( collisionBytes ) ) == SIR_MESH_PACK_OK );
	collisionBytes[56] = 5;
	TEST( reader.verifyCollision( 0, collisionBytes, sizeof( collisionBytes ) ) == SIR_MESH_PACK_INVALID_FORMAT );
	collisionBytes[56] = 1;
	TEST( reader.verifyCollision( 0, collisionBytes, sizeof( collisionBytes ) - 1 ) == SIR_MESH_PACK_TRUNCATED );
	pages[1].nParent = 1;
	TEST( SIRMeshPackSchema::write( desc, buffer, bytes ) == SIR_MESH_PACK_INVALID_FORMAT );
	pages[1].nParent = 0;
	groups[1].nParent = 1;
	TEST( SIRMeshPackSchema::write( desc, buffer, bytes ) == SIR_MESH_PACK_INVALID_FORMAT );
	groups[1].nParent = 0;
	clusters[1].nByteOffset = 1000;
	TEST( SIRMeshPackSchema::write( desc, buffer, bytes ) == SIR_MESH_PACK_INVALID_FORMAT );
	clusters[1].nByteOffset = 4;
	collision.nRevision = 10;
	TEST( SIRMeshPackSchema::write( desc, buffer, bytes ) == SIR_MESH_PACK_INVALID_FORMAT );
	collision.nRevision = 9;
	TEST( SIRMeshPackSchema::write( desc, buffer, bytes ) == SIR_MESH_PACK_OK );
	desc.nGroups = 0;
	TEST( SIRMeshPackSchema::requiredBytes( desc ) == 0 );
	printf( "SIRG schema tests passed\n" );
	return 0;
}
