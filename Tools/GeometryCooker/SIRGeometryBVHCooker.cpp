/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Tools/GeometryCooker/SIRGeometryBVHCooker.h>
#include <Tools/GeometryCooker/SIRGeometryCooker.h>

#include <Mesh/SIRMeshCollisionBVH.h>
#include <Resources/SIRPackFormat.h>

#include <climits>
#include <cstring>

static void sirGeometryCookReleaseTemporaryPin( void * ) noexcept
{
}

// One local owner orchestrates the same native mesh BVH builder used for
// CPU collision tests, without copying source vertices or indexes.
class SIRGeometryBVHCookScope
{
public:
	SIRGeometryBVHCookScope( void ) noexcept : m_bCache( false ), m_bSource( false ), m_bBVH( false ), m_bHandle( false ), m_Handle{}
	{
	}
	~SIRGeometryBVHCookScope( void ) noexcept
	{
		if ( m_bBVH )
			( void )bvh.shutdown();
		if ( m_bSource )
			( void )source.shutdown();
		if ( m_bHandle )
			( void )cache.retire( m_Handle );
		if ( m_bCache )
			( void )cache.shutdown();
	}

	bool initialize( const sirGeometryCookInput_t& input ) noexcept
	{
		const uint64_t nCacheBytes = SIRMeshCache::requiredBytes( 1 );
		const uint32_t nAlignment = SIRMeshCache::requiredAlignment();
		try {
			cacheStorage.resize( ( size_t )nCacheBytes + nAlignment );
			const uintptr_t nAligned = ( ( uintptr_t )cacheStorage.data() + nAlignment - 1 ) & ~( ( uintptr_t )nAlignment - 1 );
			if ( cache.initialize( ( void * )nAligned, cacheStorage.size() - ( nAligned - ( uintptr_t )cacheStorage.data() ), 1 ) !=
			     SIR_MESH_OK )
				return false;
			m_bCache = true;
			uint32_t nPinToken = 0;
			const sirMeshGeometrySource_t geometry = { input.key,
			                                           input.pPositionsXYZ,
			                                           input.pIndices,
			                                           input.nVertexCount,
			                                           input.nIndexCount,
			                                           &m_nPinToken,
			                                           sirGeometryCookReleaseTemporaryPin };
			if ( cache.insert( &geometry, &m_Handle ) != SIR_MESH_OK )
				return false;
			m_bHandle = true;
			const sirMeshCollisionProfile_t profile = { 1, input.pMaterials[0].nMaterialId, 0, 0 };
			if ( source.initialize( cache, m_Handle, profile ) != SIR_MESH_OK )
				return false;
			m_bSource = true;
			const uint64_t nBytes = SIRMeshCollisionBVH::requiredBytes( input.nIndexCount / 3 );
			if ( nBytes == 0 || nBytes > SIZE_MAX - 7 )
				return false;
			bvhStorage.resize( ( size_t )( ( nBytes + 7 ) / 8 ) );
			if ( bvh.initialize( source, bvhStorage.data(), bvhStorage.size() * sizeof( uint64_t ) ) != SIR_MESH_OK )
				return false;
			m_bBVH = true;
		} catch ( ... ) {
			return false;
		}
		return true;
	}

	const sirMeshBVHNode_t *getNodes( void ) const noexcept
	{
		return ( const sirMeshBVHNode_t * )bvhStorage.data();
	}
	const uint32_t *getTriangleOrder( uint32_t nTriangles ) const noexcept
	{
		return ( const uint32_t * )( getNodes() + 2ULL * nTriangles );
	}
	uint32_t getNodeCount( void ) const noexcept
	{
		return bvh.getNodeCount();
	}

protected:
private:
	SIRMeshCache cache;
	SIRMeshCollisionSource source;
	SIRMeshCollisionBVH bvh;
	std::vector<uint8_t> cacheStorage;
	std::vector<uint64_t> bvhStorage;
	uint32_t m_nPinToken = 0;
	bool m_bCache;
	bool m_bSource;
	bool m_bBVH;
	bool m_bHandle;
	sirMeshHandle_t m_Handle;
};

static void sirGeometryWriteBVH32( std::vector<uint8_t>& bytes, uint32_t nValue )
{
	const size_t at = bytes.size();
	bytes.resize( at + 4 );
	sirPackWrite32( bytes.data() + at, nValue );
}

static void sirGeometryWriteBVHFloat( std::vector<uint8_t>& bytes, float fValue )
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	sirGeometryWriteBVH32( bytes, nBits );
}

bool SIRGeometryBVHCooker::cook( const sirGeometryCookInput_t& input, std::vector<uint8_t> *pBytes, uint32_t *pNodeCount ) noexcept
{
	if ( pBytes == nullptr || pNodeCount == nullptr || input.pIndices == nullptr || input.pPositionsXYZ == nullptr ||
	     input.pMaterials == nullptr || input.nIndexCount == 0 || input.nIndexCount % 3 != 0 )
		return false;
	try {
		SIRGeometryBVHCookScope scope;
		if ( !scope.initialize( input ) )
			return false;
		const uint32_t nNodes = scope.getNodeCount();
		const uint32_t nTriangles = input.nIndexCount / 3;
		const uint64_t nTotalBytes = SIR_MESH_PACK_BVH_HEADER_BYTES + ( uint64_t )nNodes * SIR_MESH_PACK_BVH_NODE_BYTES +
		                             ( uint64_t )nTriangles * sizeof( uint32_t );
		if ( nNodes == 0 || nTotalBytes > SIZE_MAX || nTotalBytes > SIR_PACK_MAX_DECODED_CHUNK_BYTES )
			return false;
		std::vector<uint8_t> serialized;
		serialized.reserve( ( size_t )nTotalBytes );
		sirGeometryWriteBVH32( serialized, SIR_MESH_PACK_BVH_FORMAT_ID );
		sirGeometryWriteBVH32( serialized, SIR_MESH_PACK_BVH_VERSION | ( SIR_MESH_PACK_BVH_HEADER_BYTES << 16 ) );
		sirGeometryWriteBVH32( serialized, SIR_PACK_ENDIAN_MARKER );
		sirGeometryWriteBVH32( serialized, nNodes );
		sirGeometryWriteBVH32( serialized, nTriangles );
		sirGeometryWriteBVH32( serialized, 0 );
		sirGeometryWriteBVH32( serialized, 0 );
		sirGeometryWriteBVH32( serialized, 0 );
		for ( uint32_t i = 0; i < nNodes; ++i ) {
			const sirMeshBVHNode_t& node = scope.getNodes()[i];
			for ( uint32_t axis = 0; axis < 3; ++axis )
				sirGeometryWriteBVHFloat( serialized, node.boundsMin[axis] );
			for ( uint32_t axis = 0; axis < 3; ++axis )
				sirGeometryWriteBVHFloat( serialized, node.boundsMax[axis] );
			sirGeometryWriteBVH32( serialized, node.nLeft );
			sirGeometryWriteBVH32( serialized, node.nRight );
			sirGeometryWriteBVH32( serialized, node.nFirst );
			sirGeometryWriteBVH32( serialized, node.nCount );
		}
		const uint32_t *pOrder = scope.getTriangleOrder( nTriangles );
		for ( uint32_t i = 0; i < nTriangles; ++i )
			sirGeometryWriteBVH32( serialized, pOrder[i] );
		if ( serialized.size() != nTotalBytes )
			return false;
		pBytes->swap( serialized );
		*pNodeCount = nNodes;
	} catch ( ... ) {
		return false;
	}
	return true;
}
