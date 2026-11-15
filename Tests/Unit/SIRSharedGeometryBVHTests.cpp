/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Mesh/SIRSharedGeometryBVH.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE( x )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( x ) ) {                                                                                                                    \
			fprintf( stderr, "failed line %u: %s\n", __LINE__, #x );                                                                       \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static void resign( uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	uint8_t szDigest[32] = {};
	sirPackSha256( pBytes + SIR_SHARED_BVH_HEADER_BYTES, nBytes - SIR_SHARED_BVH_HEADER_BYTES, szDigest );
	memcpy( pBytes + 96, szDigest, 32 );
}

static void *sirBvhTestAlloc( void *pContext, uint64_t nBytes, uint32_t nAlign, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nDomain;
#if defined( _WIN32 )
	return _aligned_malloc( ( size_t )nBytes, nAlign );
#else
	void *pData = nullptr;
	const size_t nActualAlign = nAlign < sizeof( void * ) ? sizeof( void * ) : nAlign;
	return posix_memalign( &pData, nActualAlign, ( size_t )nBytes ) == 0 ? pData : nullptr;
#endif
}

static void sirBvhTestFree( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlign, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nBytes;
	( void )nAlign;
	( void )nDomain;
#if defined( _WIN32 )
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static bool testSharedGRDGBacking( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, sirBvhTestAlloc, sirBvhTestFree };
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 4;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 4 * 1024 * 1024;
	SIRResourceGraph graph( config );
	REQUIRE( graph.isReady() );
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	REQUIRE( pWriter != nullptr );
	SIRResourceStagingHandle hStaging = {};
	SIRResourceMutableBytes staging = {};
	REQUIRE( pWriter->allocateStaging( nBytes, 16, 1, &hStaging, &staging ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( staging.m_pData, pBytes, ( size_t )nBytes );
	const SIRResourceChunkPublication chunk = { { 0 }, { 9, 11, SIR_SHARED_BVH_FORMAT, 0 }, hStaging, false };
	const SIRResourcePublication publication = {
	    { 55, 7, 0 }, SIR_RESOURCE_KIND_GEOMETRY, SIR_SHARED_BVH_VERSION, SIR_SHARED_BVH_FORMAT, &chunk, 1, nullptr, 0 };
	SIRResourceHandle hResource = {};
	REQUIRE( pWriter->publishExpected( publication, 0, &hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
	REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
	{
		SIRResourceReadScope scope( graph );
		SIRResourceView resourceViews[4];
		SIRSharedGeometryBVH domainViews[4];
		uint32_t szValidation[4][9] = {};
		const uint8_t *pSourceBacking = nullptr;
		for ( uint32_t iDomain = 0; iDomain < 4; ++iDomain ) {
			REQUIRE( graph.resolve( scope, hResource, &resourceViews[iDomain] ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( domainViews[iDomain].openPublished( resourceViews[iDomain], 0, szValidation[iDomain], 9 ) == SIR_SHARED_BVH_OK );
			SIRResourceConstBytes bytes = {};
			REQUIRE( resourceViews[iDomain].getChunk( { 0 }, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
			if ( iDomain == 0 ) {
				pSourceBacking = bytes.m_pData;
			}
			REQUIRE( bytes.m_pData == pSourceBacking );
			sirSharedBvhTriangle_t triangle = {};
			REQUIRE( domainViews[iDomain].triangleAt( 1, &triangle ) == SIR_SHARED_BVH_OK );
			REQUIRE( triangle.nStableTriangleId == 11 && triangle.nSurfaceMaterialId == 9 );
			sirSharedSurfaceMaterial_t material = {};
			REQUIRE( domainViews[iDomain].materialAt( 1, &material ) == SIR_SHARED_BVH_OK );
			REQUIRE( material.nMaterialId == 9 );
		}
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
	}
	return true;
}

static bool testSharedBvh( void ) noexcept
{
	sirSharedBvhNode_t nodes[3] = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		nodes[i].szMinimum[0] = 0.0f;
		nodes[i].szMaximum[0] = 10.0f;
		nodes[i].szMinimum[1] = 0.0f;
		nodes[i].szMaximum[1] = 10.0f;
		nodes[i].szMinimum[2] = 0.0f;
		nodes[i].szMaximum[2] = 10.0f;
	}
	nodes[0].nFirstChild = 1;
	nodes[0].nSecondChild = 2;
	nodes[0].fGeometricError = 2.0f;
	nodes[1].nFlags = SIR_SHARED_BVH_LEAF;
	nodes[1].nFirstChild = SIR_SHARED_BVH_NO_CHILD;
	nodes[1].nSecondChild = SIR_SHARED_BVH_NO_CHILD;
	nodes[1].nTriangleCount = 1;
	nodes[2].nFlags = SIR_SHARED_BVH_LEAF;
	nodes[2].nFirstChild = SIR_SHARED_BVH_NO_CHILD;
	nodes[2].nSecondChild = SIR_SHARED_BVH_NO_CHILD;
	nodes[2].nFirstTriangle = 1;
	nodes[2].nTriangleCount = 2;

	sirSharedSurfaceMaterial_t materials[2] = {};
	materials[0].nMaterialId = 7;
	materials[0].fDensity = 1000.0f;
	materials[0].fFriction = 0.5f;
	materials[0].fNavigationCost = 1.0f;
	materials[0].szAcousticAbsorption[0] = 0.25f;
	materials[0].szAcousticTransmission[0] = 0.25f;
	materials[1] = materials[0];
	materials[1].nMaterialId = 9;
	materials[1].fDensity = 2000.0f;
	sirSharedBvhTriangle_t triangles[3] = { { 10, 7, 23, 0 }, { 11, 9, 24, 1 }, { 12, 7, 23, 2 } };
	sirSharedBvhProxy_t proxies[2] = { { 2 | 4, 1, 0, 3, 1.0f, 0 }, { 1 | 8, 2, 0, 3, 2.0f, 0 } };
	uint32_t szParents[9] = {};
	const sirSharedBvhDescriptor_t desc = { 55, 3, nodes, triangles, proxies, materials, 3, 3, 2, 2, szParents, 9 };
	const uint64_t nSize = SIRSharedGeometryBVH::requiredBytes( 3, 3, 2, 2 );
	uint8_t szBytes[768] = {};
	uint8_t szAgain[768] = {};
	REQUIRE( nSize <= sizeof( szBytes ) );
	REQUIRE( SIRSharedGeometryBVH::requiredValidationWords( 3, 3 ) == 9 );
	REQUIRE( SIRSharedGeometryBVH::write( desc, szBytes, sizeof( szBytes ) ) == SIR_SHARED_BVH_OK );
	REQUIRE( SIRSharedGeometryBVH::write( desc, szAgain, sizeof( szAgain ) ) == SIR_SHARED_BVH_OK );
	REQUIRE( memcmp( szBytes, szAgain, ( size_t )nSize ) == 0 );
	REQUIRE( testSharedGRDGBacking( szBytes, nSize ) );
	SIRSharedGeometryBVH view;
	REQUIRE( view.open( szBytes, nSize, szParents, 9 ) == SIR_SHARED_BVH_OK );
	REQUIRE( view.getAssetId() == 55 && view.getRevision() == 3 );
	sirSharedBvhTriangle_t triangle = {};
	REQUIRE( view.triangleAt( 1, &triangle ) == SIR_SHARED_BVH_OK );
	REQUIRE( triangle.nStableTriangleId == 11 && triangle.nSurfaceMaterialId == 9 );
	sirSharedSurfaceMaterial_t material = {};
	REQUIRE( view.materialAt( 0, &material ) == SIR_SHARED_BVH_OK );
	REQUIRE( material.nMaterialId == 7 && material.szAcousticAbsorption[0] == 0.25f );
	sirSharedBvhProxy_t proxy = {};
	REQUIRE( view.proxyAt( 0, &proxy ) == SIR_SHARED_BVH_OK && proxy.nDomainMask == 6 );
	REQUIRE( view.open( szBytes, nSize - 1, szParents, 9 ) == SIR_SHARED_BVH_INVALID_FORMAT );
	REQUIRE( !view.isOpen() );

	// BVH leaves can reorder source triangles for spatial locality while
	// preserving globally stable triangle identity and a true permutation.
	triangles[0].nStableTriangleId = 12;
	triangles[0].nSourceTriangleIndex = 2;
	triangles[2].nStableTriangleId = 10;
	triangles[2].nSourceTriangleIndex = 0;
	REQUIRE( SIRSharedGeometryBVH::write( desc, szAgain, sizeof( szAgain ) ) == SIR_SHARED_BVH_OK );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_OK );
	REQUIRE( view.triangleAt( 0, &triangle ) == SIR_SHARED_BVH_OK );
	REQUIRE( triangle.nStableTriangleId == 12 && triangle.nSourceTriangleIndex == 2 );
	triangles[0].nStableTriangleId = 10;
	triangles[0].nSourceTriangleIndex = 0;
	triangles[2].nStableTriangleId = 12;
	triangles[2].nSourceTriangleIndex = 2;
	memcpy( szAgain, szBytes, ( size_t )nSize );
	sirPackWrite32( szAgain + 128 + 3 * 48 + 16, 10 );
	resign( szAgain, nSize );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_INVALID_FORMAT );
	memcpy( szAgain, szBytes, ( size_t )nSize );
	sirPackWrite32( szAgain + 128 + 3 * 48 + 16 + 12, 0 );
	resign( szAgain, nSize );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_INVALID_FORMAT );

	// Reject a signed cycle, a duplicate parent, bad material reference, and
	// invalid acoustic conservation independent of the outer SIRP digest.
	memcpy( szAgain, szBytes, ( size_t )nSize );
	sirPackWrite32( szAgain + 128 + 24, 0 );
	resign( szAgain, nSize );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_INVALID_FORMAT );
	memcpy( szAgain, szBytes, ( size_t )nSize );
	const uint64_t nTrianglesOffset = 128 + 3 * 48;
	sirPackWrite32( szAgain + nTrianglesOffset + 4, 999 );
	resign( szAgain, nSize );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_INVALID_FORMAT );
	memcpy( szAgain, szBytes, ( size_t )nSize );
	const uint64_t nMaterialsOffset = nTrianglesOffset + 3 * 16 + 2 * 24;
	const uint32_t nOne = 0x3f800000;
	sirPackWrite32( szAgain + nMaterialsOffset + 40, nOne );
	resign( szAgain, nSize );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_INVALID_FORMAT );
	memcpy( szAgain, szBytes, ( size_t )nSize );
	szAgain[128] ^= 1;
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_BAD_DIGEST );
	memcpy( szAgain, szBytes, ( size_t )nSize );
	sirPackWrite16( szAgain + 4, 2 );
	REQUIRE( view.open( szAgain, nSize, szParents, 9 ) == SIR_SHARED_BVH_UNSUPPORTED_VERSION );

	// Invalid source data is rejected before touching the old cooked bytes.
	memcpy( szAgain, szBytes, ( size_t )nSize );
	nodes[0].nSecondChild = 1;
	REQUIRE( SIRSharedGeometryBVH::write( desc, szAgain, sizeof( szAgain ) ) == SIR_SHARED_BVH_INVALID_FORMAT );
	REQUIRE( memcmp( szAgain, szBytes, ( size_t )nSize ) == 0 );
	return true;
}

int main( void )
{
	return testSharedBvh() ? 0 : 1;
}
