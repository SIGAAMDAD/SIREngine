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
#include <string.h>
#include <stdlib.h>

static uint32_t sirBvhFloatBits( float fValue ) noexcept
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	return nBits;
}

static float sirBvhFloatFromBits( uint32_t nBits ) noexcept
{
	float fValue = 0;
	memcpy( &fValue, &nBits, sizeof( fValue ) );
	return fValue;
}

static bool sirBvhBoundsValid( const sirSharedBvhNode_t& node ) noexcept
{
	if ( !isfinite( node.fGeometricError ) || node.fGeometricError < 0.0f ) {
		return false;
	}
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !isfinite( node.szMinimum[i] ) || !isfinite( node.szMaximum[i] ) || node.szMinimum[i] > node.szMaximum[i] ) {
			return false;
		}
	}
	return true;
}

static bool sirBvhMaterialValid( const sirSharedSurfaceMaterial_t& material ) noexcept
{
	if ( !isfinite( material.fFriction ) || material.fFriction < 0 || !isfinite( material.fRestitution ) || material.fRestitution < 0 ||
	     material.fRestitution > 1 ) {
		return false;
	}
	if ( !isfinite( material.fDensity ) || material.fDensity <= 0 || !isfinite( material.fNavigationCost ) ||
	     material.fNavigationCost < 0 ) {
		return false;
	}
	if ( !isfinite( material.fAcousticScattering ) || material.fAcousticScattering < 0 || material.fAcousticScattering > 1 ) {
		return false;
	}
	for ( uint32_t iBand = 0; iBand < 4; ++iBand ) {
		const float fAbsorption = material.szAcousticAbsorption[iBand];
		const float fTransmission = material.szAcousticTransmission[iBand];
		if ( !isfinite( fAbsorption ) || !isfinite( fTransmission ) || fAbsorption < 0 || fTransmission < 0 ||
		     fAbsorption + fTransmission > 1 ) {
			return false;
		}
	}
	return true;
}

static bool sirBvhMaterialExists( const sirSharedSurfaceMaterial_t *pMaterials, uint32_t nCount, uint32_t nId ) noexcept
{
	uint32_t iFirst = 0;
	uint32_t iLast = nCount;
	while ( iFirst < iLast ) {
		const uint32_t iMiddle = iFirst + ( iLast - iFirst ) / 2;
		if ( pMaterials[iMiddle].nMaterialId < nId ) {
			iFirst = iMiddle + 1;
		} else {
			iLast = iMiddle;
		}
	}
	return iFirst != nCount && pMaterials[iFirst].nMaterialId == nId;
}

static bool sirBvhChildContained( const sirSharedBvhNode_t& parent, const sirSharedBvhNode_t& child ) noexcept
{
	if ( parent.fGeometricError < child.fGeometricError ) {
		return false;
	}
	for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
		if ( child.szMinimum[iAxis] < parent.szMinimum[iAxis] || child.szMaximum[iAxis] > parent.szMaximum[iAxis] ) {
			return false;
		}
	}
	return true;
}

static int sirBvhCompareStableIds( const void *pFirst, const void *pSecond ) noexcept
{
	const uint32_t nFirst = *( const uint32_t * )pFirst;
	const uint32_t nSecond = *( const uint32_t * )pSecond;
	return ( nFirst > nSecond ) - ( nFirst < nSecond );
}

static bool sirBvhValidateTriangleRemap( uint32_t *pScratch, uint32_t nTriangles ) noexcept
{
	for ( uint32_t iTriangle = 0; iTriangle < nTriangles; ++iTriangle ) {
		if ( pScratch[iTriangle] != 1 ) {
			return false;
		}
	}
	uint32_t *pStableIds = pScratch + nTriangles;
	qsort( pStableIds, nTriangles, sizeof( uint32_t ), sirBvhCompareStableIds );
	for ( uint32_t iTriangle = 1; iTriangle < nTriangles; ++iTriangle ) {
		if ( pStableIds[iTriangle] == pStableIds[iTriangle - 1] ) {
			return false;
		}
	}
	return true;
}

static bool sirBvhValidateNodes( const sirSharedBvhDescriptor_t& desc ) noexcept
{
	memset( desc.pParentScratch, 0, ( size_t )desc.nNodeCount * sizeof( uint32_t ) );
	uint32_t nNextTriangle = 0;
	for ( uint32_t iNode = 0; iNode < desc.nNodeCount; ++iNode ) {
		const sirSharedBvhNode_t& node = desc.pNodes[iNode];
		if ( !sirBvhBoundsValid( node ) || ( node.nFlags & ~SIR_SHARED_BVH_LEAF ) != 0 ) {
			return false;
		}
		const bool bLeaf = ( node.nFlags & SIR_SHARED_BVH_LEAF ) != 0;
		if ( bLeaf ) {
			if ( node.nFirstChild != SIR_SHARED_BVH_NO_CHILD || node.nSecondChild != SIR_SHARED_BVH_NO_CHILD || node.nTriangleCount == 0 ||
			     node.nFirstTriangle >= desc.nTriangleCount || node.nTriangleCount > desc.nTriangleCount - node.nFirstTriangle ||
			     node.nFirstTriangle != nNextTriangle ) {
				return false;
			}
			nNextTriangle += node.nTriangleCount;
			continue;
		}
		if ( node.nTriangleCount != 0 || node.nFirstChild <= iNode || node.nSecondChild <= iNode || node.nFirstChild >= desc.nNodeCount ||
		     node.nSecondChild >= desc.nNodeCount || node.nFirstChild == node.nSecondChild ) {
			return false;
		}
		if ( !sirBvhChildContained( node, desc.pNodes[node.nFirstChild] ) ||
		     !sirBvhChildContained( node, desc.pNodes[node.nSecondChild] ) ) {
			return false;
		}
		if ( ++desc.pParentScratch[node.nFirstChild] > 1 || ++desc.pParentScratch[node.nSecondChild] > 1 ) {
			return false;
		}
	}
	if ( desc.pParentScratch[0] != 0 || nNextTriangle != desc.nTriangleCount ) {
		return false;
	}
	for ( uint32_t iNode = 1; iNode < desc.nNodeCount; ++iNode ) {
		if ( desc.pParentScratch[iNode] != 1 ) {
			return false;
		}
	}
	return true;
}

static bool sirBvhValidateValues( const sirSharedBvhDescriptor_t& desc ) noexcept
{
	if ( !sirBvhValidateNodes( desc ) ) {
		return false;
	}
	uint32_t nLastId = 0;
	for ( uint32_t iMaterial = 0; iMaterial < desc.nMaterialCount; ++iMaterial ) {
		const sirSharedSurfaceMaterial_t& material = desc.pMaterials[iMaterial];
		if ( !sirBvhMaterialValid( material ) || ( iMaterial != 0 && material.nMaterialId <= nLastId ) ) {
			return false;
		}
		nLastId = material.nMaterialId;
	}
	uint32_t *pRemapScratch = desc.pParentScratch + desc.nNodeCount;
	memset( pRemapScratch, 0, ( size_t )desc.nTriangleCount * sizeof( uint32_t ) );
	for ( uint32_t iTriangle = 0; iTriangle < desc.nTriangleCount; ++iTriangle ) {
		const sirSharedBvhTriangle_t& triangle = desc.pTriangles[iTriangle];
		if ( !sirBvhMaterialExists( desc.pMaterials, desc.nMaterialCount, triangle.nSurfaceMaterialId ) ||
		     triangle.nSourceTriangleIndex >= desc.nTriangleCount ) {
			return false;
		}
		++pRemapScratch[triangle.nSourceTriangleIndex];
		pRemapScratch[desc.nTriangleCount + iTriangle] = triangle.nStableTriangleId;
	}
	if ( !sirBvhValidateTriangleRemap( pRemapScratch, desc.nTriangleCount ) ) {
		return false;
	}
	for ( uint32_t iProxy = 0; iProxy < desc.nProxyCount; ++iProxy ) {
		const sirSharedBvhProxy_t& proxy = desc.pProxies[iProxy];
		if ( proxy.nDomainMask == 0 || ( proxy.nDomainMask & ~15U ) != 0 || proxy.nFirstTriangle >= desc.nTriangleCount ||
		     proxy.nTriangleCount == 0 || proxy.nTriangleCount > desc.nTriangleCount - proxy.nFirstTriangle ||
		     !isfinite( proxy.fMaximumError ) || proxy.fMaximumError < 0 ) {
			return false;
		}
	}
	return true;
}

static void sirBvhWriteNode( uint8_t *pBytes, const sirSharedBvhNode_t& node ) noexcept
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		sirPackWrite32( pBytes + i * 4, sirBvhFloatBits( node.szMinimum[i] ) );
		sirPackWrite32( pBytes + 12 + i * 4, sirBvhFloatBits( node.szMaximum[i] ) );
	}
	sirPackWrite32( pBytes + 24, node.nFirstChild );
	sirPackWrite32( pBytes + 28, node.nSecondChild );
	sirPackWrite32( pBytes + 32, node.nFirstTriangle );
	sirPackWrite32( pBytes + 36, node.nTriangleCount );
	sirPackWrite32( pBytes + 40, sirBvhFloatBits( node.fGeometricError ) );
	sirPackWrite32( pBytes + 44, node.nFlags );
}

static void sirBvhReadNode( const uint8_t *pBytes, sirSharedBvhNode_t *pNode ) noexcept
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		pNode->szMinimum[i] = sirBvhFloatFromBits( sirPackRead32( pBytes + i * 4 ) );
		pNode->szMaximum[i] = sirBvhFloatFromBits( sirPackRead32( pBytes + 12 + i * 4 ) );
	}
	pNode->nFirstChild = sirPackRead32( pBytes + 24 );
	pNode->nSecondChild = sirPackRead32( pBytes + 28 );
	pNode->nFirstTriangle = sirPackRead32( pBytes + 32 );
	pNode->nTriangleCount = sirPackRead32( pBytes + 36 );
	pNode->fGeometricError = sirBvhFloatFromBits( sirPackRead32( pBytes + 40 ) );
	pNode->nFlags = sirPackRead32( pBytes + 44 );
}

static void sirBvhWriteTriangle( uint8_t *pBytes, const sirSharedBvhTriangle_t& triangle ) noexcept
{
	sirPackWrite32( pBytes, triangle.nStableTriangleId );
	sirPackWrite32( pBytes + 4, triangle.nSurfaceMaterialId );
	sirPackWrite32( pBytes + 8, triangle.nSurfaceSemanticId );
	sirPackWrite32( pBytes + 12, triangle.nSourceTriangleIndex );
}

static void sirBvhReadTriangle( const uint8_t *pBytes, sirSharedBvhTriangle_t *pTriangle ) noexcept
{
	pTriangle->nStableTriangleId = sirPackRead32( pBytes );
	pTriangle->nSurfaceMaterialId = sirPackRead32( pBytes + 4 );
	pTriangle->nSurfaceSemanticId = sirPackRead32( pBytes + 8 );
	pTriangle->nSourceTriangleIndex = sirPackRead32( pBytes + 12 );
}

static void sirBvhWriteProxy( uint8_t *pBytes, const sirSharedBvhProxy_t& proxy ) noexcept
{
	sirPackWrite32( pBytes, proxy.nDomainMask );
	sirPackWrite32( pBytes + 4, proxy.nProfileId );
	sirPackWrite32( pBytes + 8, proxy.nFirstTriangle );
	sirPackWrite32( pBytes + 12, proxy.nTriangleCount );
	sirPackWrite32( pBytes + 16, sirBvhFloatBits( proxy.fMaximumError ) );
	sirPackWrite32( pBytes + 20, proxy.nFlags );
}

static void sirBvhReadProxy( const uint8_t *pBytes, sirSharedBvhProxy_t *pProxy ) noexcept
{
	pProxy->nDomainMask = sirPackRead32( pBytes );
	pProxy->nProfileId = sirPackRead32( pBytes + 4 );
	pProxy->nFirstTriangle = sirPackRead32( pBytes + 8 );
	pProxy->nTriangleCount = sirPackRead32( pBytes + 12 );
	pProxy->fMaximumError = sirBvhFloatFromBits( sirPackRead32( pBytes + 16 ) );
	pProxy->nFlags = sirPackRead32( pBytes + 20 );
}

static void sirBvhWriteMaterial( uint8_t *pBytes, const sirSharedSurfaceMaterial_t& material ) noexcept
{
	sirPackWrite32( pBytes, material.nMaterialId );
	sirPackWrite32( pBytes + 4, material.nSurfaceSemantics );
	sirPackWrite64( pBytes + 8, material.nVisualResourceAssetId );
	sirPackWrite32( pBytes + 16, sirBvhFloatBits( material.fFriction ) );
	sirPackWrite32( pBytes + 20, sirBvhFloatBits( material.fRestitution ) );
	sirPackWrite32( pBytes + 24, sirBvhFloatBits( material.fDensity ) );
	sirPackWrite32( pBytes + 28, sirBvhFloatBits( material.fNavigationCost ) );
	sirPackWrite32( pBytes + 32, material.nNavigationAgentMask );
	sirPackWrite32( pBytes + 36, material.nVisualFacetId );
	for ( uint32_t iBand = 0; iBand < 4; ++iBand ) {
		sirPackWrite32( pBytes + 40 + iBand * 4, sirBvhFloatBits( material.szAcousticAbsorption[iBand] ) );
		sirPackWrite32( pBytes + 56 + iBand * 4, sirBvhFloatBits( material.szAcousticTransmission[iBand] ) );
	}
	sirPackWrite32( pBytes + 72, sirBvhFloatBits( material.fAcousticScattering ) );
	sirPackWrite32( pBytes + 76, 0 );
}

static void sirBvhReadMaterial( const uint8_t *pBytes, sirSharedSurfaceMaterial_t *pMaterial ) noexcept
{
	pMaterial->nMaterialId = sirPackRead32( pBytes );
	pMaterial->nSurfaceSemantics = sirPackRead32( pBytes + 4 );
	pMaterial->nVisualResourceAssetId = sirPackRead64( pBytes + 8 );
	pMaterial->fFriction = sirBvhFloatFromBits( sirPackRead32( pBytes + 16 ) );
	pMaterial->fRestitution = sirBvhFloatFromBits( sirPackRead32( pBytes + 20 ) );
	pMaterial->fDensity = sirBvhFloatFromBits( sirPackRead32( pBytes + 24 ) );
	pMaterial->fNavigationCost = sirBvhFloatFromBits( sirPackRead32( pBytes + 28 ) );
	pMaterial->nNavigationAgentMask = sirPackRead32( pBytes + 32 );
	pMaterial->nVisualFacetId = sirPackRead32( pBytes + 36 );
	for ( uint32_t iBand = 0; iBand < 4; ++iBand ) {
		pMaterial->szAcousticAbsorption[iBand] = sirBvhFloatFromBits( sirPackRead32( pBytes + 40 + iBand * 4 ) );
		pMaterial->szAcousticTransmission[iBand] = sirBvhFloatFromBits( sirPackRead32( pBytes + 56 + iBand * 4 ) );
	}
	pMaterial->fAcousticScattering = sirBvhFloatFromBits( sirPackRead32( pBytes + 72 ) );
}

SIRSharedGeometryBVH::SIRSharedGeometryBVH( void ) noexcept
    : m_pBytes( nullptr ), m_nBytes( 0 ), m_nNodes( 0 ), m_nTriangles( 0 ), m_nProxies( 0 ), m_nMaterials( 0 )
{
}

uint64_t SIRSharedGeometryBVH::requiredBytes( uint32_t nNodes, uint32_t nTriangles, uint32_t nProxies, uint32_t nMaterials ) noexcept
{
	if ( nNodes == 0 || nNodes > SIR_SHARED_BVH_MAX_RECORDS || nTriangles == 0 || nTriangles > SIR_SHARED_BVH_MAX_RECORDS ||
	     nProxies > SIR_SHARED_BVH_MAX_RECORDS || nMaterials == 0 || nMaterials > SIR_SHARED_BVH_MAX_RECORDS ) {
		return 0;
	}
	return SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )nNodes * SIR_SHARED_BVH_NODE_BYTES +
	       ( uint64_t )nTriangles * SIR_SHARED_BVH_TRIANGLE_BYTES + ( uint64_t )nProxies * SIR_SHARED_BVH_PROXY_BYTES +
	       ( uint64_t )nMaterials * SIR_SHARED_BVH_MATERIAL_BYTES;
}

uint32_t SIRSharedGeometryBVH::requiredValidationWords( uint32_t nNodes, uint32_t nTriangles ) noexcept
{
	if ( nNodes == 0 || nNodes > SIR_SHARED_BVH_MAX_RECORDS || nTriangles == 0 || nTriangles > SIR_SHARED_BVH_MAX_RECORDS ) {
		return 0;
	}
	return nNodes + 2 * nTriangles;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::write( const sirSharedBvhDescriptor_t& desc, void *pOutput, uint64_t nCapacity ) noexcept
{
	const uint64_t nRequired = requiredBytes( desc.nNodeCount, desc.nTriangleCount, desc.nProxyCount, desc.nMaterialCount );
	if ( nRequired == 0 || pOutput == nullptr || desc.pNodes == nullptr || desc.pTriangles == nullptr || desc.pMaterials == nullptr ||
	     desc.pParentScratch == nullptr ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	if ( ( desc.nProxyCount != 0 && desc.pProxies == nullptr ) ||
	     desc.nParentCapacity < requiredValidationWords( desc.nNodeCount, desc.nTriangleCount ) || nCapacity < nRequired ) {
		return SIR_SHARED_BVH_OUT_OF_CAPACITY;
	}
	if ( !sirBvhValidateValues( desc ) ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}

	uint8_t *pBytes = ( uint8_t * )pOutput;
	memset( pBytes, 0, ( size_t )nRequired );
	sirPackWrite32( pBytes, SIR_SHARED_BVH_FORMAT );
	sirPackWrite16( pBytes + 4, SIR_SHARED_BVH_VERSION );
	sirPackWrite16( pBytes + 6, ( uint16_t )SIR_SHARED_BVH_HEADER_BYTES );
	sirPackWrite32( pBytes + 8, SIR_PACK_ENDIAN_MARKER );
	sirPackWrite64( pBytes + 16, nRequired );
	sirPackWrite64( pBytes + 24, desc.nAssetId );
	sirPackWrite64( pBytes + 32, desc.nRevision );
	sirPackWrite32( pBytes + 40, desc.nNodeCount );
	sirPackWrite32( pBytes + 44, desc.nTriangleCount );
	sirPackWrite32( pBytes + 48, desc.nProxyCount );
	sirPackWrite32( pBytes + 52, desc.nMaterialCount );
	sirPackWrite32( pBytes + 56, 0 );
	uint64_t nOffset = SIR_SHARED_BVH_HEADER_BYTES;
	sirPackWrite64( pBytes + 64, nOffset );
	for ( uint32_t i = 0; i < desc.nNodeCount; ++i ) {
		sirBvhWriteNode( pBytes + nOffset + i * SIR_SHARED_BVH_NODE_BYTES, desc.pNodes[i] );
	}
	nOffset += ( uint64_t )desc.nNodeCount * SIR_SHARED_BVH_NODE_BYTES;
	sirPackWrite64( pBytes + 72, nOffset );
	for ( uint32_t i = 0; i < desc.nTriangleCount; ++i ) {
		sirBvhWriteTriangle( pBytes + nOffset + i * SIR_SHARED_BVH_TRIANGLE_BYTES, desc.pTriangles[i] );
	}
	nOffset += ( uint64_t )desc.nTriangleCount * SIR_SHARED_BVH_TRIANGLE_BYTES;
	sirPackWrite64( pBytes + 80, nOffset );
	for ( uint32_t i = 0; i < desc.nProxyCount; ++i ) {
		sirBvhWriteProxy( pBytes + nOffset + i * SIR_SHARED_BVH_PROXY_BYTES, desc.pProxies[i] );
	}
	nOffset += ( uint64_t )desc.nProxyCount * SIR_SHARED_BVH_PROXY_BYTES;
	sirPackWrite64( pBytes + 88, nOffset );
	for ( uint32_t i = 0; i < desc.nMaterialCount; ++i ) {
		sirBvhWriteMaterial( pBytes + nOffset + i * SIR_SHARED_BVH_MATERIAL_BYTES, desc.pMaterials[i] );
	}
	uint8_t szDigest[32] = {};
	sirPackSha256( pBytes + SIR_SHARED_BVH_HEADER_BYTES, nRequired - SIR_SHARED_BVH_HEADER_BYTES, szDigest );
	memcpy( pBytes + 96, szDigest, sizeof( szDigest ) );
	return SIR_SHARED_BVH_OK;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::open( const void *pBytes, uint64_t nBytes, uint32_t *pParentScratch,
                                                 uint32_t nParentCapacity ) noexcept
{
	reset();
	if ( pBytes == nullptr || pParentScratch == nullptr || nBytes > SIZE_MAX ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	if ( nBytes < SIR_SHARED_BVH_HEADER_BYTES ) {
		return SIR_SHARED_BVH_TRUNCATED;
	}
	const uint8_t *pInput = ( const uint8_t * )pBytes;
	if ( sirPackRead32( pInput ) != SIR_SHARED_BVH_FORMAT || sirPackRead32( pInput + 8 ) != SIR_PACK_ENDIAN_MARKER ||
	     sirPackRead16( pInput + 6 ) != SIR_SHARED_BVH_HEADER_BYTES ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	if ( sirPackRead16( pInput + 4 ) != SIR_SHARED_BVH_VERSION ) {
		return SIR_SHARED_BVH_UNSUPPORTED_VERSION;
	}
	const uint32_t nNodes = sirPackRead32( pInput + 40 );
	const uint32_t nTriangles = sirPackRead32( pInput + 44 );
	const uint32_t nProxies = sirPackRead32( pInput + 48 );
	const uint32_t nMaterials = sirPackRead32( pInput + 52 );
	const uint64_t nRequired = requiredBytes( nNodes, nTriangles, nProxies, nMaterials );
	if ( nRequired == 0 || nParentCapacity < requiredValidationWords( nNodes, nTriangles ) || sirPackRead64( pInput + 16 ) != nRequired ||
	     nRequired != nBytes ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	const uint64_t nTrianglesOffset = SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )nNodes * SIR_SHARED_BVH_NODE_BYTES;
	const uint64_t nProxiesOffset = nTrianglesOffset + ( uint64_t )nTriangles * SIR_SHARED_BVH_TRIANGLE_BYTES;
	const uint64_t nMaterialsOffset = nProxiesOffset + ( uint64_t )nProxies * SIR_SHARED_BVH_PROXY_BYTES;
	if ( sirPackRead32( pInput + 12 ) != 0 || sirPackRead64( pInput + 56 ) != 0 ||
	     sirPackRead64( pInput + 64 ) != SIR_SHARED_BVH_HEADER_BYTES || sirPackRead64( pInput + 72 ) != nTrianglesOffset ||
	     sirPackRead64( pInput + 80 ) != nProxiesOffset || sirPackRead64( pInput + 88 ) != nMaterialsOffset ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	uint8_t szDigest[32] = {};
	sirPackSha256( pInput + SIR_SHARED_BVH_HEADER_BYTES, nRequired - SIR_SHARED_BVH_HEADER_BYTES, szDigest );
	if ( memcmp( szDigest, pInput + 96, sizeof( szDigest ) ) != 0 ) {
		return SIR_SHARED_BVH_BAD_DIGEST;
	}
	// Validate via the same source-value contract as the writer, without
	// interpreting packed bytes as native C++ records or allocating.
	// A single bounded validation pass decodes records on demand.
	memset( pParentScratch, 0, ( size_t )nNodes * sizeof( uint32_t ) );
	uint32_t nNextTriangle = 0;
	for ( uint32_t iNode = 0; iNode < nNodes; ++iNode ) {
		sirSharedBvhNode_t node = {};
		sirBvhReadNode( pInput + SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )iNode * SIR_SHARED_BVH_NODE_BYTES, &node );
		if ( !sirBvhBoundsValid( node ) || ( node.nFlags & ~SIR_SHARED_BVH_LEAF ) != 0 ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
		if ( ( node.nFlags & SIR_SHARED_BVH_LEAF ) != 0 ) {
			if ( node.nFirstChild != SIR_SHARED_BVH_NO_CHILD || node.nSecondChild != SIR_SHARED_BVH_NO_CHILD || node.nTriangleCount == 0 ||
			     node.nFirstTriangle >= nTriangles || node.nTriangleCount > nTriangles - node.nFirstTriangle ||
			     node.nFirstTriangle != nNextTriangle ) {
				return SIR_SHARED_BVH_INVALID_FORMAT;
			}
			nNextTriangle += node.nTriangleCount;
			continue;
		}
		if ( node.nFirstChild <= iNode || node.nSecondChild <= iNode || node.nFirstChild == node.nSecondChild ||
		     node.nFirstChild >= nNodes || node.nSecondChild >= nNodes || node.nTriangleCount != 0 ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
		sirSharedBvhNode_t first = {};
		sirSharedBvhNode_t second = {};
		sirBvhReadNode( pInput + SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )node.nFirstChild * SIR_SHARED_BVH_NODE_BYTES, &first );
		sirBvhReadNode( pInput + SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )node.nSecondChild * SIR_SHARED_BVH_NODE_BYTES, &second );
		if ( !sirBvhChildContained( node, first ) || !sirBvhChildContained( node, second ) ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
		if ( ++pParentScratch[node.nFirstChild] > 1 || ++pParentScratch[node.nSecondChild] > 1 ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
	}
	if ( nNextTriangle != nTriangles ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	for ( uint32_t iNode = 1; iNode < nNodes; ++iNode ) {
		if ( pParentScratch[iNode] != 1 ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
	}
	uint32_t nPreviousId = 0;
	for ( uint32_t iMaterial = 0; iMaterial < nMaterials; ++iMaterial ) {
		sirSharedSurfaceMaterial_t material = {};
		sirBvhReadMaterial( pInput + nMaterialsOffset + ( uint64_t )iMaterial * SIR_SHARED_BVH_MATERIAL_BYTES, &material );
		if ( !sirBvhMaterialValid( material ) || ( iMaterial != 0 && material.nMaterialId <= nPreviousId ) ||
		     sirPackRead32( pInput + nMaterialsOffset + ( uint64_t )iMaterial * SIR_SHARED_BVH_MATERIAL_BYTES + 76 ) != 0 ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
		nPreviousId = material.nMaterialId;
	}
	uint32_t *pRemapScratch = pParentScratch + nNodes;
	memset( pRemapScratch, 0, ( size_t )nTriangles * sizeof( uint32_t ) );
	for ( uint32_t iTriangle = 0; iTriangle < nTriangles; ++iTriangle ) {
		sirSharedBvhTriangle_t triangle = {};
		sirBvhReadTriangle( pInput + nTrianglesOffset + ( uint64_t )iTriangle * SIR_SHARED_BVH_TRIANGLE_BYTES, &triangle );
		if ( triangle.nSourceTriangleIndex >= nTriangles ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
		uint32_t iFirst = 0;
		uint32_t iLast = nMaterials;
		while ( iFirst < iLast ) {
			const uint32_t iMiddle = iFirst + ( iLast - iFirst ) / 2;
			const uint32_t nMaterialId = sirPackRead32( pInput + nMaterialsOffset + ( uint64_t )iMiddle * SIR_SHARED_BVH_MATERIAL_BYTES );
			if ( nMaterialId < triangle.nSurfaceMaterialId ) {
				iFirst = iMiddle + 1;
			} else {
				iLast = iMiddle;
			}
		}
		if ( iFirst == nMaterials || sirPackRead32( pInput + nMaterialsOffset + ( uint64_t )iFirst * SIR_SHARED_BVH_MATERIAL_BYTES ) !=
		                                 triangle.nSurfaceMaterialId ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
		++pRemapScratch[triangle.nSourceTriangleIndex];
		pRemapScratch[nTriangles + iTriangle] = triangle.nStableTriangleId;
	}
	if ( !sirBvhValidateTriangleRemap( pRemapScratch, nTriangles ) ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	for ( uint32_t iProxy = 0; iProxy < nProxies; ++iProxy ) {
		sirSharedBvhProxy_t proxy = {};
		sirBvhReadProxy( pInput + nProxiesOffset + ( uint64_t )iProxy * SIR_SHARED_BVH_PROXY_BYTES, &proxy );
		if ( proxy.nDomainMask == 0 || ( proxy.nDomainMask & ~15U ) != 0 || proxy.nFirstTriangle >= nTriangles ||
		     proxy.nTriangleCount == 0 || proxy.nTriangleCount > nTriangles - proxy.nFirstTriangle || !isfinite( proxy.fMaximumError ) ||
		     proxy.fMaximumError < 0 ) {
			return SIR_SHARED_BVH_INVALID_FORMAT;
		}
	}
	m_pBytes = pInput;
	m_nBytes = nRequired;
	m_nNodes = nNodes;
	m_nTriangles = nTriangles;
	m_nProxies = nProxies;
	m_nMaterials = nMaterials;
	return SIR_SHARED_BVH_OK;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::openPublished( const SIRResourceView& resource, uint32_t nChunkId, uint32_t *pScratch,
                                                          uint32_t nScratchWords ) noexcept
{
	reset();
	SIRResourceInfo info = {};
	if ( resource.getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	if ( info.m_eKind != SIR_RESOURCE_KIND_GEOMETRY || info.m_nFormatId != SIR_SHARED_BVH_FORMAT ||
	     info.m_nSchemaVersion != SIR_SHARED_BVH_VERSION ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	SIRResourceConstBytes bytes = {};
	if ( resource.getChunk( { nChunkId }, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	const sirSharedBvhResult_t eResult = open( bytes.m_pData, bytes.m_nSize, pScratch, nScratchWords );
	if ( eResult != SIR_SHARED_BVH_OK ) {
		return eResult;
	}
	if ( info.m_Key.m_nAssetId != getAssetId() ) {
		reset();
		return SIR_SHARED_BVH_INVALID_FORMAT;
	}
	return SIR_SHARED_BVH_OK;
}

void SIRSharedGeometryBVH::reset( void ) noexcept
{
	m_pBytes = nullptr;
	m_nBytes = 0;
	m_nNodes = 0;
	m_nTriangles = 0;
	m_nProxies = 0;
	m_nMaterials = 0;
}

bool SIRSharedGeometryBVH::isOpen( void ) const noexcept
{
	return m_pBytes != nullptr;
}

uint64_t SIRSharedGeometryBVH::getAssetId( void ) const noexcept
{
	return m_pBytes != nullptr ? sirPackRead64( m_pBytes + 24 ) : 0;
}

uint64_t SIRSharedGeometryBVH::getRevision( void ) const noexcept
{
	return m_pBytes != nullptr ? sirPackRead64( m_pBytes + 32 ) : 0;
}

uint32_t SIRSharedGeometryBVH::getNodeCount( void ) const noexcept
{
	return m_nNodes;
}

uint32_t SIRSharedGeometryBVH::getTriangleCount( void ) const noexcept
{
	return m_nTriangles;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::nodeAt( uint32_t iNode, sirSharedBvhNode_t *pNode ) const noexcept
{
	if ( !isOpen() || iNode >= m_nNodes || pNode == nullptr ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	sirBvhReadNode( m_pBytes + SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )iNode * SIR_SHARED_BVH_NODE_BYTES, pNode );
	return SIR_SHARED_BVH_OK;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::triangleAt( uint32_t iTriangle, sirSharedBvhTriangle_t *pTriangle ) const noexcept
{
	if ( !isOpen() || iTriangle >= m_nTriangles || pTriangle == nullptr ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	const uint64_t nOffset = SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )m_nNodes * SIR_SHARED_BVH_NODE_BYTES;
	sirBvhReadTriangle( m_pBytes + nOffset + ( uint64_t )iTriangle * SIR_SHARED_BVH_TRIANGLE_BYTES, pTriangle );
	return SIR_SHARED_BVH_OK;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::proxyAt( uint32_t iProxy, sirSharedBvhProxy_t *pProxy ) const noexcept
{
	if ( !isOpen() || iProxy >= m_nProxies || pProxy == nullptr ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	const uint64_t nOffset = SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )m_nNodes * SIR_SHARED_BVH_NODE_BYTES +
	                         ( uint64_t )m_nTriangles * SIR_SHARED_BVH_TRIANGLE_BYTES;
	sirBvhReadProxy( m_pBytes + nOffset + ( uint64_t )iProxy * SIR_SHARED_BVH_PROXY_BYTES, pProxy );
	return SIR_SHARED_BVH_OK;
}

sirSharedBvhResult_t SIRSharedGeometryBVH::materialAt( uint32_t iMaterial, sirSharedSurfaceMaterial_t *pMaterial ) const noexcept
{
	if ( !isOpen() || iMaterial >= m_nMaterials || pMaterial == nullptr ) {
		return SIR_SHARED_BVH_INVALID_ARGUMENT;
	}
	const uint64_t nOffset = SIR_SHARED_BVH_HEADER_BYTES + ( uint64_t )m_nNodes * SIR_SHARED_BVH_NODE_BYTES +
	                         ( uint64_t )m_nTriangles * SIR_SHARED_BVH_TRIANGLE_BYTES + ( uint64_t )m_nProxies * SIR_SHARED_BVH_PROXY_BYTES;
	sirBvhReadMaterial( m_pBytes + nOffset + ( uint64_t )iMaterial * SIR_SHARED_BVH_MATERIAL_BYTES, pMaterial );
	return SIR_SHARED_BVH_OK;
}
