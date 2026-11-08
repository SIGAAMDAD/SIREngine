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

#include <cmath>
#include <cstring>
#include <limits>

static uint32_t sirMeshPackFloatBits( float fValue ) noexcept
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	return nBits;
}

static float sirMeshPackBitsFloat( uint32_t nBits ) noexcept
{
	float fValue = 0;
	memcpy( &fValue, &nBits, sizeof( fValue ) );
	return fValue;
}

static uint64_t sirMeshPackTableOffset( uint32_t nGroups, uint32_t nClusters, uint32_t nPages, uint32_t iTable ) noexcept
{
	uint64_t nOffset = SIR_MESH_PACK_HEADER_BYTES;
	if ( iTable > 0 )
		nOffset += ( uint64_t )nGroups * SIR_MESH_PACK_GROUP_BYTES;
	if ( iTable > 1 )
		nOffset += ( uint64_t )nClusters * SIR_MESH_PACK_CLUSTER_BYTES;
	if ( iTable > 2 )
		nOffset += ( uint64_t )nPages * SIR_MESH_PACK_PAGE_BYTES;
	return nOffset;
}

static void sirMeshPackDigest( const uint8_t *pBytes, uint64_t nBytes, uint8_t *pDigest ) noexcept
{
	// Digest the canonical byte stream with its digest field treated as zero.
	// The underlying SHA implementation is the same as the SIRPack catalog.
	// Reuse SIRPack's digest algorithm by zeroing a copy on write; open performs
	// the same calculation using the helper below (field offset 88).
	sirPackCatalogDigest( pBytes, nBytes, pDigest );
}

static bool sirMeshPackValidCounts( uint32_t nGroups, uint32_t nClusters, uint32_t nPages, uint32_t nCollisions ) noexcept
{
	return nGroups != 0 && nClusters != 0 && nPages != 0 && nGroups <= SIR_MESH_PACK_MAX_RECORDS &&
	       nClusters <= SIR_MESH_PACK_MAX_RECORDS && nPages <= SIR_MESH_PACK_MAX_RECORDS && nCollisions <= SIR_MESH_PACK_MAX_RECORDS;
}

static void sirMeshPackWriteGroup( uint8_t *p, const sirMeshPackGroup_t& g ) noexcept
{
	sirPackWrite32( p, g.nParent );
	sirPackWrite32( p + 4, g.nFirstCluster );
	sirPackWrite32( p + 8, g.nClusterCount );
	sirPackWrite32( p + 12, g.nFirstChild );
	sirPackWrite32( p + 16, g.nChildCount );
	sirPackWrite32( p + 20, g.nMaterialId );
	sirPackWrite32( p + 24, g.nLevel );
	sirPackWrite32( p + 28, g.nPage );
	sirPackWrite32( p + 32, sirMeshPackFloatBits( g.fError ) );
}
static void sirMeshPackWriteCluster( uint8_t *p, const sirMeshPackCluster_t& c ) noexcept
{
	sirPackWrite32( p, c.nGroup );
	sirPackWrite32( p + 4, c.nPage );
	sirPackWrite32( p + 8, c.nVertexCount );
	sirPackWrite32( p + 12, c.nTriangleCount );
	sirPackWrite64( p + 16, c.nByteOffset );
	sirPackWrite64( p + 24, c.nByteCount );
	sirPackWrite32( p + 32, sirMeshPackFloatBits( c.fError ) );
}
static void sirMeshPackWritePage( uint8_t *p, const sirMeshPackPage_t& page ) noexcept
{
	sirPackWrite32( p, page.nParent );
	sirPackWrite32( p + 4, page.nFirstCluster );
	sirPackWrite32( p + 8, page.nClusterCount );
	sirPackWrite32( p + 12, page.nChunkId );
	sirPackWrite64( p + 16, page.nByteCount );
	memcpy( p + 24, page.szDigest, 16 );
}
static void sirMeshPackWriteCollision( uint8_t *p, const sirMeshPackCollision_t& c ) noexcept
{
	sirPackWrite32( p, c.nProfile );
	sirPackWrite32( p + 4, c.nFlags );
	sirPackWrite32( p + 8, c.nVertexCount );
	sirPackWrite32( p + 12, c.nIndexCount );
	sirPackWrite32( p + 16, c.nChunkId );
	sirPackWrite64( p + 24, c.nRevision );
}

static sirMeshPackGroup_t sirMeshPackReadGroup( const uint8_t *p ) noexcept
{
	sirMeshPackGroup_t g = {};
	g.nParent = sirPackRead32( p );
	g.nFirstCluster = sirPackRead32( p + 4 );
	g.nClusterCount = sirPackRead32( p + 8 );
	g.nFirstChild = sirPackRead32( p + 12 );
	g.nChildCount = sirPackRead32( p + 16 );
	g.nMaterialId = sirPackRead32( p + 20 );
	g.nLevel = sirPackRead32( p + 24 );
	g.nPage = sirPackRead32( p + 28 );
	g.fError = sirMeshPackBitsFloat( sirPackRead32( p + 32 ) );
	return g;
}
static sirMeshPackCluster_t sirMeshPackReadCluster( const uint8_t *p ) noexcept
{
	sirMeshPackCluster_t c = {};
	c.nGroup = sirPackRead32( p );
	c.nPage = sirPackRead32( p + 4 );
	c.nVertexCount = sirPackRead32( p + 8 );
	c.nTriangleCount = sirPackRead32( p + 12 );
	c.nByteOffset = sirPackRead64( p + 16 );
	c.nByteCount = sirPackRead64( p + 24 );
	c.fError = sirMeshPackBitsFloat( sirPackRead32( p + 32 ) );
	return c;
}
static sirMeshPackPage_t sirMeshPackReadPage( const uint8_t *p ) noexcept
{
	sirMeshPackPage_t page = {};
	page.nParent = sirPackRead32( p );
	page.nFirstCluster = sirPackRead32( p + 4 );
	page.nClusterCount = sirPackRead32( p + 8 );
	page.nChunkId = sirPackRead32( p + 12 );
	page.nByteCount = sirPackRead64( p + 16 );
	memcpy( page.szDigest, p + 24, 16 );
	return page;
}
static sirMeshPackCollision_t sirMeshPackReadCollision( const uint8_t *p ) noexcept
{
	sirMeshPackCollision_t c = {};
	c.nProfile = sirPackRead32( p );
	c.nFlags = sirPackRead32( p + 4 );
	c.nVertexCount = sirPackRead32( p + 8 );
	c.nIndexCount = sirPackRead32( p + 12 );
	c.nChunkId = sirPackRead32( p + 16 );
	c.nRevision = sirPackRead64( p + 24 );
	return c;
}

SIRMeshPackSchema::SIRMeshPackSchema( void ) noexcept
    : m_pBytes( nullptr ), m_nBytes( 0 ), m_Key{}, m_nGroups( 0 ), m_nClusters( 0 ), m_nPages( 0 ), m_nCollisions( 0 )
{
}

bool SIRMeshPackSchema::isOpen( void ) const noexcept
{
	return m_pBytes != nullptr;
}
sirMeshKey_t SIRMeshPackSchema::getKey( void ) const noexcept
{
	return m_Key;
}
uint32_t SIRMeshPackSchema::getGroupCount( void ) const noexcept
{
	return m_nGroups;
}
uint32_t SIRMeshPackSchema::getClusterCount( void ) const noexcept
{
	return m_nClusters;
}
uint32_t SIRMeshPackSchema::getPageCount( void ) const noexcept
{
	return m_nPages;
}

uint64_t SIRMeshPackSchema::requiredBytes( const sirMeshPackDesc_t& d ) noexcept
{
	if ( !sirMeshPackValidCounts( d.nGroups, d.nClusters, d.nPages, d.nCollisions ) )
		return 0;
	return sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 3 ) + ( uint64_t )d.nCollisions * SIR_MESH_PACK_COLLISION_BYTES;
}

sirMeshPackResult_t SIRMeshPackSchema::write( const sirMeshPackDesc_t& d, uint8_t *pBytes, uint64_t nCapacity ) noexcept
{
	const uint64_t nBytes = requiredBytes( d );
	if ( pBytes == nullptr || d.pGroups == nullptr || d.pClusters == nullptr || d.pPages == nullptr ||
	     ( d.nCollisions && d.pCollisions == nullptr ) || d.key.nAssetId == 0 || d.key.nRevision == 0 || d.key.nReserved != 0 ||
	     d.nVertexCount == 0 || d.nTriangleCount == 0 || nBytes == 0 )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	if ( nCapacity < nBytes )
		return SIR_MESH_PACK_OUT_OF_CAPACITY;
	memset( pBytes, 0, ( size_t )nBytes );
	sirPackWrite32( pBytes, SIR_MESH_PACK_FORMAT_ID );
	sirPackWrite16( pBytes + 4, SIR_MESH_PACK_VERSION );
	sirPackWrite16( pBytes + 6, ( uint16_t )SIR_MESH_PACK_HEADER_BYTES );
	sirPackWrite32( pBytes + 8, SIR_PACK_ENDIAN_MARKER );
	sirPackWrite64( pBytes + 16, d.key.nAssetId );
	sirPackWrite64( pBytes + 24, d.key.nRevision );
	sirPackWrite32( pBytes + 32, d.key.nVariant );
	sirPackWrite32( pBytes + 36, 0 );
	sirPackWrite32( pBytes + 40, d.nGroups );
	sirPackWrite32( pBytes + 44, d.nClusters );
	sirPackWrite32( pBytes + 48, d.nPages );
	sirPackWrite32( pBytes + 52, d.nCollisions );
	sirPackWrite32( pBytes + 56, d.nVertexCount );
	sirPackWrite32( pBytes + 60, d.nTriangleCount );
	for ( uint32_t t = 0; t < 3; ++t )
		sirPackWrite64( pBytes + 64 + 8 * t, sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, t ) );
	for ( uint32_t i = 0; i < d.nGroups; ++i )
		sirMeshPackWriteGroup( pBytes + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) +
		                           ( uint64_t )i * SIR_MESH_PACK_GROUP_BYTES,
		                       d.pGroups[i] );
	for ( uint32_t i = 0; i < d.nClusters; ++i )
		sirMeshPackWriteCluster( pBytes + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 1 ) +
		                             ( uint64_t )i * SIR_MESH_PACK_CLUSTER_BYTES,
		                         d.pClusters[i] );
	for ( uint32_t i = 0; i < d.nPages; ++i )
		sirMeshPackWritePage( pBytes + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 2 ) +
		                          ( uint64_t )i * SIR_MESH_PACK_PAGE_BYTES,
		                      d.pPages[i] );
	for ( uint32_t i = 0; i < d.nCollisions; ++i )
		sirMeshPackWriteCollision( pBytes + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 3 ) +
		                               ( uint64_t )i * SIR_MESH_PACK_COLLISION_BYTES,
		                           d.pCollisions[i] );
	sirMeshPackDigest( pBytes, nBytes, pBytes + 88 );
	SIRMeshPackSchema view;
	return view.open( pBytes, nBytes );
}

void SIRMeshPackSchema::reset( void ) noexcept
{
	m_pBytes = nullptr;
	m_nBytes = 0;
	m_Key = {};
	m_nGroups = 0;
	m_nClusters = 0;
	m_nPages = 0;
	m_nCollisions = 0;
}

sirMeshPackResult_t SIRMeshPackSchema::open( const void *pData, uint64_t nBytes ) noexcept
{
	reset();
	if ( pData == nullptr )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	if ( nBytes < SIR_MESH_PACK_HEADER_BYTES )
		return SIR_MESH_PACK_TRUNCATED;
	const uint8_t *p = static_cast<const uint8_t *>( pData );
	if ( sirPackRead32( p ) != SIR_MESH_PACK_FORMAT_ID || sirPackRead32( p + 8 ) != SIR_PACK_ENDIAN_MARKER ||
	     sirPackRead16( p + 6 ) != SIR_MESH_PACK_HEADER_BYTES || sirPackRead32( p + 12 ) != 0 || sirPackRead32( p + 36 ) != 0 )
		return SIR_MESH_PACK_INVALID_FORMAT;
	if ( sirPackRead16( p + 4 ) != SIR_MESH_PACK_VERSION )
		return SIR_MESH_PACK_UNSUPPORTED_VERSION;
	sirMeshPackDesc_t d = {};
	d.key = { sirPackRead64( p + 16 ), sirPackRead64( p + 24 ), sirPackRead32( p + 32 ), 0 };
	d.nGroups = sirPackRead32( p + 40 );
	d.nClusters = sirPackRead32( p + 44 );
	d.nPages = sirPackRead32( p + 48 );
	d.nCollisions = sirPackRead32( p + 52 );
	d.nVertexCount = sirPackRead32( p + 56 );
	d.nTriangleCount = sirPackRead32( p + 60 );
	const uint64_t nRequired = requiredBytes( d );
	if ( nRequired == 0 || d.key.nAssetId == 0 || d.key.nRevision == 0 || d.nVertexCount == 0 || d.nTriangleCount == 0 )
		return SIR_MESH_PACK_INVALID_FORMAT;
	if ( nBytes < nRequired )
		return SIR_MESH_PACK_TRUNCATED;
	if ( nBytes != nRequired )
		return SIR_MESH_PACK_INVALID_FORMAT;
	for ( uint32_t j = 120; j < 128; ++j )
		if ( p[j] != 0 )
			return SIR_MESH_PACK_INVALID_FORMAT;
	for ( uint32_t t = 0; t < 3; ++t )
		if ( sirPackRead64( p + 64 + 8 * t ) != sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, t ) )
			return SIR_MESH_PACK_INVALID_FORMAT;
	uint8_t szDigest[32] = {};
	sirMeshPackDigest( p, nBytes, szDigest );
	if ( memcmp( p + 88, szDigest, 32 ) != 0 )
		return SIR_MESH_PACK_BAD_DIGEST;
	uint32_t nExpectedGroupCluster = 0;
	for ( uint32_t i = 0; i < d.nGroups; ++i ) {
		const sirMeshPackGroup_t g = sirMeshPackReadGroup( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) +
		                                                   ( uint64_t )i * SIR_MESH_PACK_GROUP_BYTES );
		const uint8_t *pGroupWire =
		    p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) + ( uint64_t )i * SIR_MESH_PACK_GROUP_BYTES;
		for ( uint32_t j = 36; j < SIR_MESH_PACK_GROUP_BYTES; ++j )
			if ( pGroupWire[j] != 0 )
				return SIR_MESH_PACK_INVALID_FORMAT;
		if ( ( i == 0 ? g.nParent != SIR_MESH_PACK_NO_PARENT : g.nParent >= i ) || g.nClusterCount == 0 ||
		     g.nFirstCluster != nExpectedGroupCluster || g.nClusterCount > d.nClusters - nExpectedGroupCluster || g.nPage >= d.nPages ||
		     !std::isfinite( g.fError ) || g.fError < 0.0f ||
		     ( g.nChildCount && ( g.nFirstChild <= i || g.nFirstChild >= d.nGroups || g.nChildCount > d.nGroups - g.nFirstChild ) ) ||
		     ( !g.nChildCount && g.nFirstChild != 0 ) )
			return SIR_MESH_PACK_INVALID_FORMAT;
		if ( i != 0 ) {
			const sirMeshPackGroup_t parent = sirMeshPackReadGroup( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) +
			                                                        ( uint64_t )g.nParent * SIR_MESH_PACK_GROUP_BYTES );
			if ( g.nLevel >= parent.nLevel || g.fError > parent.fError || i < parent.nFirstChild ||
			     i - parent.nFirstChild >= parent.nChildCount )
				return SIR_MESH_PACK_INVALID_FORMAT;
		} else if ( g.nPage != 0 )
			return SIR_MESH_PACK_INVALID_FORMAT;
		for ( uint32_t j = 0; j < g.nChildCount; ++j ) {
			const uint32_t nChildIndex = g.nFirstChild + j;
			const uint8_t *pChild =
			    p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) + ( uint64_t )nChildIndex * SIR_MESH_PACK_GROUP_BYTES;
			if ( sirPackRead32( pChild ) != i )
				return SIR_MESH_PACK_INVALID_FORMAT;
		}
		nExpectedGroupCluster += g.nClusterCount;
	}
	if ( nExpectedGroupCluster != d.nClusters )
		return SIR_MESH_PACK_INVALID_FORMAT;
	for ( uint32_t i = 0; i < d.nClusters; ++i ) {
		const sirMeshPackCluster_t c = sirMeshPackReadCluster( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 1 ) +
		                                                       ( uint64_t )i * SIR_MESH_PACK_CLUSTER_BYTES );
		const uint8_t *pClusterWire =
		    p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 1 ) + ( uint64_t )i * SIR_MESH_PACK_CLUSTER_BYTES;
		for ( uint32_t j = 36; j < SIR_MESH_PACK_CLUSTER_BYTES; ++j )
			if ( pClusterWire[j] != 0 )
				return SIR_MESH_PACK_INVALID_FORMAT;
		if ( c.nGroup >= d.nGroups || c.nPage >= d.nPages || c.nVertexCount == 0 || c.nVertexCount > 256 || c.nTriangleCount == 0 ||
		     c.nTriangleCount > 512 || c.nByteCount == 0 || !std::isfinite( c.fError ) || c.fError < 0.0f )
			return SIR_MESH_PACK_INVALID_FORMAT;
		const sirMeshPackGroup_t g = sirMeshPackReadGroup( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) +
		                                                   ( uint64_t )c.nGroup * SIR_MESH_PACK_GROUP_BYTES );
		if ( c.nPage != g.nPage || i < g.nFirstCluster || i - g.nFirstCluster >= g.nClusterCount )
			return SIR_MESH_PACK_INVALID_FORMAT;
	}
	uint32_t nExpectedCluster = 0;
	for ( uint32_t i = 0; i < d.nPages; ++i ) {
		const sirMeshPackPage_t page = sirMeshPackReadPage( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 2 ) +
		                                                    ( uint64_t )i * SIR_MESH_PACK_PAGE_BYTES );
		const uint8_t *pPageWire =
		    p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 2 ) + ( uint64_t )i * SIR_MESH_PACK_PAGE_BYTES;
		for ( uint32_t j = 40; j < SIR_MESH_PACK_PAGE_BYTES; ++j )
			if ( pPageWire[j] != 0 )
				return SIR_MESH_PACK_INVALID_FORMAT;
		if ( ( i == 0 ? page.nParent != SIR_MESH_PACK_NO_PARENT : page.nParent >= i ) || page.nChunkId != i + 1 ||
		     page.nClusterCount == 0 || page.nFirstCluster != nExpectedCluster || page.nClusterCount > d.nClusters - nExpectedCluster ||
		     page.nByteCount == 0 || page.nByteCount > SIR_PACK_MAX_DECODED_CHUNK_BYTES )
			return SIR_MESH_PACK_INVALID_FORMAT;
		for ( uint32_t j = 0; j < page.nClusterCount; ++j ) {
			const sirMeshPackCluster_t c = sirMeshPackReadCluster( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 1 ) +
			                                                       ( uint64_t )( page.nFirstCluster + j ) * SIR_MESH_PACK_CLUSTER_BYTES );
			if ( c.nPage != i || c.nByteOffset > page.nByteCount || c.nByteCount > page.nByteCount - c.nByteOffset )
				return SIR_MESH_PACK_INVALID_FORMAT;
		}
		nExpectedCluster += page.nClusterCount;
	}
	if ( nExpectedCluster != d.nClusters )
		return SIR_MESH_PACK_INVALID_FORMAT;
	for ( uint32_t i = 1; i < d.nGroups; ++i ) {
		const sirMeshPackGroup_t g = sirMeshPackReadGroup( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) +
		                                                   ( uint64_t )i * SIR_MESH_PACK_GROUP_BYTES );
		const sirMeshPackGroup_t parent = sirMeshPackReadGroup( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 0 ) +
		                                                        ( uint64_t )g.nParent * SIR_MESH_PACK_GROUP_BYTES );
		const sirMeshPackPage_t page = sirMeshPackReadPage( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 2 ) +
		                                                    ( uint64_t )g.nPage * SIR_MESH_PACK_PAGE_BYTES );
		if ( page.nParent != parent.nPage )
			return SIR_MESH_PACK_INVALID_FORMAT;
	}
	for ( uint32_t i = 0; i < d.nCollisions; ++i ) {
		const sirMeshPackCollision_t c = sirMeshPackReadCollision( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 3 ) +
		                                                           ( uint64_t )i * SIR_MESH_PACK_COLLISION_BYTES );
		const uint8_t *pCollisionWire =
		    p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 3 ) + ( uint64_t )i * SIR_MESH_PACK_COLLISION_BYTES;
		for ( uint32_t j = 20; j < 24; ++j )
			if ( pCollisionWire[j] != 0 )
				return SIR_MESH_PACK_INVALID_FORMAT;
		for ( uint32_t j = 32; j < SIR_MESH_PACK_COLLISION_BYTES; ++j )
			if ( pCollisionWire[j] != 0 )
				return SIR_MESH_PACK_INVALID_FORMAT;
		if ( c.nRevision != d.key.nRevision || c.nFlags != SIR_MESH_PACK_COLLISION_SOURCE_FLAGS || c.nChunkId != d.nPages + i + 1 ||
		     c.nVertexCount == 0 || c.nIndexCount == 0 || c.nIndexCount % 3 ||
		     ( i && c.nProfile <= sirMeshPackReadCollision( p + sirMeshPackTableOffset( d.nGroups, d.nClusters, d.nPages, 3 ) +
		                                                    ( uint64_t )( i - 1 ) * SIR_MESH_PACK_COLLISION_BYTES )
		                              .nProfile ) )
			return SIR_MESH_PACK_INVALID_FORMAT;
	}
	m_pBytes = p;
	m_nBytes = nBytes;
	m_Key = d.key;
	m_nGroups = d.nGroups;
	m_nClusters = d.nClusters;
	m_nPages = d.nPages;
	m_nCollisions = d.nCollisions;
	return SIR_MESH_PACK_OK;
}

sirMeshPackResult_t SIRMeshPackSchema::groupAt( uint32_t i, sirMeshPackGroup_t *p ) const noexcept
{
	if ( p == nullptr || !isOpen() || i >= m_nGroups )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	*p = sirMeshPackReadGroup( m_pBytes + sirMeshPackTableOffset( m_nGroups, m_nClusters, m_nPages, 0 ) +
	                           ( uint64_t )i * SIR_MESH_PACK_GROUP_BYTES );
	return SIR_MESH_PACK_OK;
}
sirMeshPackResult_t SIRMeshPackSchema::clusterAt( uint32_t i, sirMeshPackCluster_t *p ) const noexcept
{
	if ( p == nullptr || !isOpen() || i >= m_nClusters )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	*p = sirMeshPackReadCluster( m_pBytes + sirMeshPackTableOffset( m_nGroups, m_nClusters, m_nPages, 1 ) +
	                             ( uint64_t )i * SIR_MESH_PACK_CLUSTER_BYTES );
	return SIR_MESH_PACK_OK;
}
sirMeshPackResult_t SIRMeshPackSchema::pageAt( uint32_t i, sirMeshPackPage_t *p ) const noexcept
{
	if ( p == nullptr || !isOpen() || i >= m_nPages )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	*p = sirMeshPackReadPage( m_pBytes + sirMeshPackTableOffset( m_nGroups, m_nClusters, m_nPages, 2 ) +
	                          ( uint64_t )i * SIR_MESH_PACK_PAGE_BYTES );
	return SIR_MESH_PACK_OK;
}
sirMeshPackResult_t SIRMeshPackSchema::collisionAt( uint32_t i, sirMeshPackCollision_t *p ) const noexcept
{
	if ( p == nullptr || !isOpen() || i >= m_nCollisions )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	*p = sirMeshPackReadCollision( m_pBytes + sirMeshPackTableOffset( m_nGroups, m_nClusters, m_nPages, 3 ) +
	                               ( uint64_t )i * SIR_MESH_PACK_COLLISION_BYTES );
	return SIR_MESH_PACK_OK;
}
sirMeshPackResult_t SIRMeshPackSchema::verifyPage( uint32_t i, const void *pBytes, uint64_t nBytes ) const noexcept
{
	sirMeshPackPage_t page = {};
	if ( pageAt( i, &page ) != SIR_MESH_PACK_OK || pBytes == nullptr )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	if ( page.nByteCount != nBytes )
		return SIR_MESH_PACK_TRUNCATED;
	uint8_t szDigest[32] = {};
	sirPackSha256( static_cast<const uint8_t *>( pBytes ), nBytes, szDigest );
	if ( memcmp( page.szDigest, szDigest, 16 ) != 0 )
		return SIR_MESH_PACK_BAD_DIGEST;
	const uint8_t *p = static_cast<const uint8_t *>( pBytes );
	if ( nBytes < 4 || sirPackRead32( p ) != page.nClusterCount )
		return SIR_MESH_PACK_INVALID_FORMAT;
	uint64_t nCursor = 4;
	for ( uint32_t j = 0; j < page.nClusterCount; ++j ) {
		sirMeshPackCluster_t c = {};
		if ( clusterAt( page.nFirstCluster + j, &c ) != SIR_MESH_PACK_OK || c.nByteOffset != nCursor || c.nByteCount < 32 ||
		     c.nByteCount > nBytes - nCursor )
			return SIR_MESH_PACK_INVALID_FORMAT;
		const uint8_t *pCluster = p + nCursor;
		if ( sirPackRead16( pCluster ) != c.nVertexCount || sirPackRead16( pCluster + 2 ) != c.nTriangleCount ||
		     c.nByteCount != 32ULL + 24ULL * c.nVertexCount + 3ULL * c.nTriangleCount )
			return SIR_MESH_PACK_INVALID_FORMAT;
		sirMeshPackGroup_t g = {};
		if ( groupAt( c.nGroup, &g ) != SIR_MESH_PACK_OK ||
		     ( g.nMaterialId != 0xffffffffU && sirPackRead32( pCluster + 4 ) != g.nMaterialId ) )
			return SIR_MESH_PACK_INVALID_FORMAT;
		for ( uint32_t k = 0; k < 3; ++k ) {
			const float fMin = sirMeshPackBitsFloat( sirPackRead32( pCluster + 8 + 4 * k ) );
			const float fMax = sirMeshPackBitsFloat( sirPackRead32( pCluster + 20 + 4 * k ) );
			if ( !std::isfinite( fMin ) || !std::isfinite( fMax ) || fMin > fMax )
				return SIR_MESH_PACK_INVALID_FORMAT;
		}
		for ( uint32_t v = 0; v < c.nVertexCount; ++v ) {
			const uint8_t *pVertex = pCluster + 32 + 24ULL * v;
			if ( ( pVertex[14] != 1 && pVertex[14] != 0xff ) || pVertex[15] != 0 ||
			     !std::isfinite( sirMeshPackBitsFloat( sirPackRead32( pVertex + 16 ) ) ) ||
			     !std::isfinite( sirMeshPackBitsFloat( sirPackRead32( pVertex + 20 ) ) ) )
				return SIR_MESH_PACK_INVALID_FORMAT;
		}
		const uint64_t nTrianglesStart = nCursor + 32ULL + 24ULL * c.nVertexCount;
		for ( uint32_t k = 0; k < c.nTriangleCount * 3; ++k ) {
			if ( p[nTrianglesStart + k] >= c.nVertexCount )
				return SIR_MESH_PACK_INVALID_FORMAT;
		}
		nCursor += c.nByteCount;
	}
	return nCursor == nBytes ? SIR_MESH_PACK_OK : SIR_MESH_PACK_INVALID_FORMAT;
}

sirMeshPackResult_t SIRMeshPackSchema::verifyCollision( uint32_t iCollision, const void *pBytes, uint64_t nBytes ) const noexcept
{
	sirMeshPackCollision_t c = {};
	if ( collisionAt( iCollision, &c ) != SIR_MESH_PACK_OK || pBytes == nullptr )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	const uint64_t nRequired = 16ULL + 12ULL * c.nVertexCount + 4ULL * c.nIndexCount + 4ULL * ( c.nIndexCount / 3 );
	if ( nBytes < nRequired )
		return SIR_MESH_PACK_TRUNCATED;
	if ( nBytes != nRequired )
		return SIR_MESH_PACK_INVALID_FORMAT;
	const uint8_t *p = static_cast<const uint8_t *>( pBytes );
	if ( sirPackRead32( p ) != 0x43524953U || sirPackRead32( p + 4 ) != c.nVertexCount || sirPackRead32( p + 8 ) != c.nIndexCount ||
	     sirPackRead32( p + 12 ) != c.nProfile )
		return SIR_MESH_PACK_INVALID_FORMAT;
	for ( uint64_t i = 0; i < ( uint64_t )c.nVertexCount * 3; ++i ) {
		if ( !std::isfinite( sirMeshPackBitsFloat( sirPackRead32( p + 16 + 4 * i ) ) ) )
			return SIR_MESH_PACK_INVALID_FORMAT;
	}
	const uint64_t nIndexOffset = 16ULL + 12ULL * c.nVertexCount;
	for ( uint32_t i = 0; i < c.nIndexCount; ++i ) {
		if ( sirPackRead32( p + nIndexOffset + 4ULL * i ) >= c.nVertexCount )
			return SIR_MESH_PACK_INVALID_FORMAT;
	}
	return SIR_MESH_PACK_OK;
}

static void sirMeshPackDecodeOct( const uint8_t *pBytes, float *pOut ) noexcept
{
	float x = ( float )( int16_t )sirPackRead16( pBytes ) / 32767.0f;
	float y = ( float )( int16_t )sirPackRead16( pBytes + 2 ) / 32767.0f;
	float z = 1.0f - std::abs( x ) - std::abs( y );
	if ( z < 0.0f ) {
		const float oldX = x;
		x = ( 1.0f - std::abs( y ) ) * ( oldX >= 0.0f ? 1.0f : -1.0f );
		y = ( 1.0f - std::abs( oldX ) ) * ( y >= 0.0f ? 1.0f : -1.0f );
	}
	const float len = std::sqrt( x * x + y * y + z * z );
	pOut[0] = x / len;
	pOut[1] = y / len;
	pOut[2] = z / len;
}

sirMeshPackResult_t SIRMeshPackSchema::decodeVertex( const sirMeshPackVertexQuery_t& query, sirMeshPackVertex_t *pVertex ) const noexcept
{
	if ( pVertex == nullptr || verifyPage( query.nPage, query.pPageBytes, query.nPageBytes ) != SIR_MESH_PACK_OK ) {
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	}
	sirMeshPackPage_t page = {};
	if ( pageAt( query.nPage, &page ) != SIR_MESH_PACK_OK || query.nCluster < page.nFirstCluster ||
	     query.nCluster - page.nFirstCluster >= page.nClusterCount )
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	sirMeshPackCluster_t cluster = {};
	if ( clusterAt( query.nCluster, &cluster ) != SIR_MESH_PACK_OK || query.nVertex >= cluster.nVertexCount ) {
		return SIR_MESH_PACK_INVALID_ARGUMENT;
	}
	const uint8_t *p = static_cast<const uint8_t *>( query.pPageBytes ) + cluster.nByteOffset;
	const uint8_t *pPacked = p + 32 + 24ULL * query.nVertex;
	sirMeshPackVertex_t decoded = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		const float fMin = sirMeshPackBitsFloat( sirPackRead32( p + 8 + 4 * i ) );
		const float fMax = sirMeshPackBitsFloat( sirPackRead32( p + 20 + 4 * i ) );
		decoded.szPosition[i] = fMin + ( fMax - fMin ) * ( ( float )sirPackRead16( pPacked + 2 * i ) / 65535.0f );
	}
	sirMeshPackDecodeOct( pPacked + 6, decoded.szNormal );
	sirMeshPackDecodeOct( pPacked + 10, decoded.szTangent );
	decoded.szTangent[3] = pPacked[14] == 0xff ? -1.0f : 1.0f;
	decoded.szUV[0] = sirMeshPackBitsFloat( sirPackRead32( pPacked + 16 ) );
	decoded.szUV[1] = sirMeshPackBitsFloat( sirPackRead32( pPacked + 20 ) );
	*pVertex = decoded;
	return SIR_MESH_PACK_OK;
}
