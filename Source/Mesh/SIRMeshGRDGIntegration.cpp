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
#include <Mesh/SIRMeshGRDGIntegration.h>

#include <Resources/SIRResourceGraph.h>
#include <Mesh/SIRMeshPackSchema.h>

#include <stdint.h>

sirMeshResult_t SIRMeshCache::releaseGRDGResource( sirMeshGRDGSourceLease_t *pLease ) noexcept
{
	if ( pLease == nullptr || !pLease->bActive || pLease->pCache != this || pLease->pGraph == nullptr ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}
	// A live source handle means either the asset was never retired or at
	// least one reader still owns the immutable bytes. Never release its pin.
	if ( m_bReady && pLease->hMesh.nOwnerId == m_nOwnerId && isValidHandle( pLease->hMesh ) ) {
		return SIR_MESH_BUSY;
	}
	const SIRResourceResult result = pLease->pGraph->releaseProviderPin( pLease->hPin );
	pLease->eLastRelease = result;
	if ( result != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_MESH_BUSY;
	}
	pLease->bActive = false;
	pLease->pGraph = nullptr;
	pLease->pCache = nullptr;
	pLease->hMesh = {};
	pLease->hPin = {};
	return SIR_MESH_OK;
}

// Immutable subranges of GRDG-owned SIRG V3 collision and BVH chunks.
// The SIRPack format is already little-endian; direct typed aliasing is
// restricted to native little-endian hosts. All other hosts need the GRDG
// canonical decoded representation before permitting zero-copy consumers.
static sirMeshResult_t sirMeshGRDGBorrowCooked( const sirMeshGRDGSourceRequest_t& request, sirMeshGeometrySource_t *pSource ) noexcept
{
	const uint16_t nEndian = 1;
	if ( *( const uint8_t * )&nEndian != 1 )
		return SIR_MESH_INVALID_ARGUMENT;
	SIRResourceConstBytes manifest = {}, collision = {}, bvh = {};
	if ( request.pView->getChunk( { 0 }, &manifest ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     request.pView->getChunk( request.packedCollisionChunk, &collision ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     request.pView->getChunk( request.packedBVHChunk, &bvh ) != SIR_RESOURCE_RESULT_SUCCESS )
		return SIR_MESH_NOT_READY;
	SIRMeshPackSchema schema;
	if ( schema.open( manifest.m_pData, manifest.m_nSize ) != SIR_MESH_PACK_OK )
		return SIR_MESH_INVALID_ARGUMENT;
	sirMeshPackCollision_t cooked = {};
	const sirMeshKey_t schemaKey = schema.getKey();
	if ( schemaKey.nAssetId != request.key.nAssetId || schemaKey.nRevision != request.key.nRevision ||
	     schemaKey.nVariant != request.key.nVariant || schema.collisionAt( request.nCollisionProfileIndex, &cooked ) != SIR_MESH_PACK_OK ||
	     cooked.nChunkId != request.packedCollisionChunk.m_nValue || cooked.nBVHChunkId != request.packedBVHChunk.m_nValue ||
	     cooked.nVertexCount != request.nVertexCount || cooked.nIndexCount != request.nIndexCount ||
	     schema.verifyCollision( request.nCollisionProfileIndex, collision.m_pData, collision.m_nSize ) != SIR_MESH_PACK_OK ||
	     schema.verifyBVH( request.nCollisionProfileIndex, bvh.m_pData, bvh.m_nSize ) != SIR_MESH_PACK_OK )
		return SIR_MESH_INVALID_ARGUMENT;
	const uint8_t *pCollision = ( const uint8_t * )collision.m_pData;
	const uint8_t *pBVH = ( const uint8_t * )bvh.m_pData;
	const uint64_t nPositionOffset = 16;
	const uint64_t nIndexOffset = nPositionOffset + ( uint64_t )cooked.nVertexCount * 3 * sizeof( float );
	const uint64_t nMaterialOffset = nIndexOffset + ( uint64_t )cooked.nIndexCount * sizeof( uint32_t );
	const uint64_t nOrderOffset = SIR_MESH_PACK_BVH_HEADER_BYTES + ( uint64_t )cooked.nBVHNodeCount * SIR_MESH_PACK_BVH_NODE_BYTES;
	if ( pCollision == nullptr || pBVH == nullptr || ( ( uintptr_t )( pCollision + nPositionOffset ) % alignof( float ) ) != 0 ||
	     ( ( uintptr_t )( pCollision + nIndexOffset ) % alignof( uint32_t ) ) != 0 ||
	     ( ( uintptr_t )( pBVH + nOrderOffset ) % alignof( uint32_t ) ) != 0 )
		return SIR_MESH_INVALID_ARGUMENT;
	pSource->pPositionsXYZ = ( const float * )( pCollision + nPositionOffset );
	pSource->pTriangleIndices = ( const uint32_t * )( pCollision + nIndexOffset );
	pSource->pSurfaceMaterials = ( const uint32_t * )( pCollision + nMaterialOffset );
	pSource->nSurfaceMaterialCount = cooked.nIndexCount / 3;
	pSource->pStaticBVH = pBVH + SIR_MESH_PACK_BVH_HEADER_BYTES;
	pSource->nStaticBVHBytes = ( uint64_t )cooked.nBVHNodeCount * SIR_MESH_PACK_BVH_NODE_BYTES;
	pSource->pBVHTriangleOrder = ( const uint32_t * )( pBVH + nOrderOffset );
	pSource->nBVHTriangleOrderCount = cooked.nBVHOrderCount;
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCache::insertGRDGResource( const sirMeshGRDGSourceRequest_t& request, sirMeshGRDGSourceLease_t *pLease,
                                                  sirMeshHandle_t *pHandle ) noexcept
{
	if ( pLease == nullptr || pHandle == nullptr || request.pGraph == nullptr || request.pScope == nullptr || request.pView == nullptr ||
	     request.nProviderId == 0 || request.nResourceTypeId == 0 || request.key.nAssetId == 0 || request.key.nRevision == 0 ||
	     request.key.nReserved != 0 || request.nVertexCount == 0 || request.nVertexCount > UINT32_MAX / 3 || request.nIndexCount == 0 ||
	     request.nIndexCount % 3 != 0 )
		return SIR_MESH_INVALID_ARGUMENT;
	if ( pLease->bActive )
		return SIR_MESH_BUSY;
	const bool bCooked = request.packedCollisionChunk.m_nValue != 0 || request.packedBVHChunk.m_nValue != 0;
	if ( bCooked != ( request.packedCollisionChunk.m_nValue != 0 && request.packedBVHChunk.m_nValue != 0 ) ||
	     ( bCooked && request.packedCollisionChunk.m_nValue == request.packedBVHChunk.m_nValue ) ||
	     ( !bCooked && request.positionsChunk.m_nValue == request.indicesChunk.m_nValue ) )
		return SIR_MESH_INVALID_ARGUMENT;
	const bool hasBVH = request.bvhNodesChunk.m_nValue != 0;
	if ( hasBVH != ( request.bvhOrderChunk.m_nValue != 0 ) || hasBVH != ( request.surfaceMaterialsChunk.m_nValue != 0 ) ||
	     ( bCooked && hasBVH ) )
		return SIR_MESH_INVALID_ARGUMENT;
	if ( hasBVH && ( request.bvhNodesChunk.m_nValue == request.positionsChunk.m_nValue ||
	                 request.bvhNodesChunk.m_nValue == request.indicesChunk.m_nValue ||
	                 request.bvhOrderChunk.m_nValue == request.surfaceMaterialsChunk.m_nValue ||
	                 request.bvhNodesChunk.m_nValue == request.bvhOrderChunk.m_nValue ) )
		return SIR_MESH_INVALID_ARGUMENT;
	SIRResourceInfo info = {};
	if ( request.pView->getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_eState != SIR_RESOURCE_STATE_RESIDENT )
		return SIR_MESH_NOT_READY;
	if ( info.m_eKind != SIR_RESOURCE_KIND_GEOMETRY || info.m_Key.m_nAssetId != request.key.nAssetId ||
	     info.m_Key.m_nTypeId != request.nResourceTypeId || info.m_Key.m_nVariantId != request.key.nVariant ||
	     info.m_nRevision != request.key.nRevision )
		return SIR_MESH_STALE_HANDLE;
	sirMeshGeometrySource_t source = { request.key, nullptr, nullptr, request.nVertexCount, request.nIndexCount, nullptr, nullptr };
	if ( bCooked ) {
		const sirMeshResult_t cookedResult = sirMeshGRDGBorrowCooked( request, &source );
		if ( cookedResult != SIR_MESH_OK )
			return cookedResult;
	} else {
		SIRResourceConstBytes vertices = {}, indices = {};
		if ( request.pView->getChunk( request.positionsChunk, &vertices ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     request.pView->getChunk( request.indicesChunk, &indices ) != SIR_RESOURCE_RESULT_SUCCESS || vertices.m_pData == nullptr ||
		     indices.m_pData == nullptr || vertices.m_nSize != ( uint64_t )request.nVertexCount * 3 * sizeof( float ) ||
		     indices.m_nSize != ( uint64_t )request.nIndexCount * sizeof( uint32_t ) ||
		     ( ( uintptr_t )vertices.m_pData % alignof( float ) ) != 0 || ( ( uintptr_t )indices.m_pData % alignof( uint32_t ) ) != 0 )
			return SIR_MESH_INVALID_ARGUMENT;
		source.pPositionsXYZ = ( const float * )vertices.m_pData;
		source.pTriangleIndices = ( const uint32_t * )indices.m_pData;
	}
	SIRResourcePinHandle pin = {};
	const SIRResourceResult pinResult = request.pGraph->pinForProvider( *request.pScope, *request.pView, request.nProviderId, &pin );
	if ( pinResult != SIR_RESOURCE_RESULT_SUCCESS )
		return pinResult == SIR_RESOURCE_RESULT_OUT_OF_BUDGET ? SIR_MESH_OUT_OF_CAPACITY : SIR_MESH_INVALID_ARGUMENT;
	pLease->eLastRelease = SIR_RESOURCE_RESULT_NOT_RESIDENT;
	if ( !bCooked && request.bvhNodesChunk.m_nValue != 0 ) {
		SIRResourceConstBytes nodes = {}, order = {}, materials = {};
		if ( request.pView->getChunk( request.bvhNodesChunk, &nodes ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     request.pView->getChunk( request.bvhOrderChunk, &order ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     request.pView->getChunk( request.surfaceMaterialsChunk, &materials ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     nodes.m_pData == nullptr || order.m_pData == nullptr || materials.m_pData == nullptr || nodes.m_nSize == 0 ||
		     nodes.m_nSize % 40 != 0 || order.m_nSize != ( uint64_t )( request.nIndexCount / 3 ) * sizeof( uint32_t ) ||
		     materials.m_nSize != order.m_nSize || ( ( uintptr_t )order.m_pData % alignof( uint32_t ) ) != 0 ||
		     ( ( uintptr_t )materials.m_pData % alignof( uint32_t ) ) != 0 ) {
			pLease->eLastRelease = request.pGraph->releaseProviderPin( pin );
			return SIR_MESH_INVALID_ARGUMENT;
		}
		source.pStaticBVH = nodes.m_pData;
		source.nStaticBVHBytes = nodes.m_nSize;
		source.pBVHTriangleOrder = ( const uint32_t * )order.m_pData;
		source.nBVHTriangleOrderCount = request.nIndexCount / 3;
		source.pSurfaceMaterials = ( const uint32_t * )materials.m_pData;
		source.nSurfaceMaterialCount = request.nIndexCount / 3;
	}
	const sirMeshResult_t result = insertImpl( &source, pHandle, true );
	if ( result != SIR_MESH_OK ) {
		pLease->eLastRelease = request.pGraph->releaseProviderPin( pin );
		return result;
	}
	pLease->pGraph = request.pGraph;
	pLease->pCache = this;
	pLease->hMesh = *pHandle;
	pLease->hPin = pin;
	pLease->bActive = true;
	return SIR_MESH_OK;
}
