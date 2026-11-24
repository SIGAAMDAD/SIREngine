/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Mesh/SIRGRDGGeometryBridge.h>

#include <stdint.h>
#include <stdlib.h>

static bool wellFormedMeshPayload( const SIRResourceConstBytes& rPositions, const SIRResourceConstBytes& rIndices ) noexcept
{
	if ( !rPositions.m_pData || !rIndices.m_pData || rPositions.m_nSize == 0 || rIndices.m_nSize == 0 ||
	     ( rPositions.m_nSize % ( 3 * sizeof( float ) ) ) != 0 || ( rIndices.m_nSize % ( 3 * sizeof( uint32_t ) ) ) != 0 ||
	     ( reinterpret_cast<uintptr_t>(rPositions.m_pData) % alignof( float ) ) != 0 || ( reinterpret_cast<uintptr_t>(rIndices.m_pData) % alignof( uint32_t ) ) != 0 ||
	     rPositions.m_nSize / ( 3 * sizeof( float ) ) > UINT32_MAX || rIndices.m_nSize / sizeof( uint32_t ) > UINT32_MAX ) {
		return false;
	}
	return true;
}

static bool optionalGeometryChunk( const SIRResourceView& rSource, SIRResourceChunkId id, SIRResourceConstBytes& rBytes ) noexcept
{
	if ( id.m_nValue == 0 ) {
		rBytes = {};
		return true;
	}
	return rSource.getChunk( id, &rBytes ) == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS && rBytes.m_pData != nullptr && rBytes.m_nSize > 0;
}

void SIRGRDGGeometryBridge::releaseSourcePin( void *pContext ) noexcept
{
	sirGRDGMeshPin_t *pState = static_cast<sirGRDGMeshPin_t *>(pContext);
	if ( pState && pState->bActive && pState->pGraph ) {
		const SIRResourceResult eRelease = pState->pGraph->releaseProviderPin( pState->hPin );
		if ( eRelease != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			abort();
		}
		*pState = {};
	}
}

SIRResourceResult SIRGRDGGeometryBridge::attach( SIRResourceGraph& rGraph, SIRMeshCache& rCache, const SIRResourceReadScope& rScope,
                                                 const SIRResourceView& rSource, const sirGRDGMeshChunkSelection_t& rChunks,
                                                 sirGRDGMeshPin_t *pPin, sirMeshHandle_t *pHandle ) noexcept
{
	if ( !pPin || pPin->bActive || !pHandle || !rChunks.nProviderId || rChunks.positionChunk.m_nValue == rChunks.indexChunk.m_nValue ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceInfo info {};
	if ( rSource.getInfo( &info ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS || info.m_eKind != SIRResourceKind::SIR_RESOURCE_KIND_GEOMETRY ||
	     info.m_eState != SIRResourceState::SIR_RESOURCE_STATE_RESIDENT || !info.m_Key.m_nAssetId || !info.m_nRevision ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	SIRResourceConstBytes positions = {}, indices = {};
	if ( rSource.getChunk( rChunks.positionChunk, &positions ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ||
	     rSource.getChunk( rChunks.indexChunk, &indices ) != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_MISSING_CHUNK;
	}
	if ( !wellFormedMeshPayload( positions, indices ) ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	const uint64_t ids[] = { rChunks.positionChunk.m_nValue, rChunks.indexChunk.m_nValue, rChunks.staticBVHChunk.m_nValue,
	                         rChunks.surfaceMaterialChunk.m_nValue, rChunks.lodHierarchyChunk.m_nValue };
	for ( uint32_t a = 0; a < 5; ++a ) {
		for ( uint32_t b = a + 1; b < 5; ++b ) {
			if ( ids[a] && ids[a] == ids[b] ) {
				return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
			}
		}
	}
	SIRResourceConstBytes bvh = {}, materials = {}, hierarchy = {};
	if ( !optionalGeometryChunk( rSource, rChunks.staticBVHChunk, bvh ) ||
	     !optionalGeometryChunk( rSource, rChunks.surfaceMaterialChunk, materials ) ||
	     !optionalGeometryChunk( rSource, rChunks.lodHierarchyChunk, hierarchy ) ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_MISSING_CHUNK;
	}
	sirMeshKey_t meshKey = { info.m_Key.m_nAssetId, info.m_nRevision, rChunks.nVariant, 0, info.m_Key.m_nTypeId, info.m_nSchemaVersion };
	sirMeshHandle_t existing {};
	if ( rCache.find( &meshKey, &existing ) == SIRMeshResult::SIR_MESH_OK ) {
		sirMeshGeometryRead_t current {};
		if ( rCache.acquire( existing, &current ) != SIRMeshResult::SIR_MESH_OK ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		const bool bSame = static_cast<const void *>(current.pPositionsXYZ) == positions.m_pData &&
		                   static_cast<const void *>(current.pTriangleIndices) == indices.m_pData &&
		                   static_cast<uint64_t>(current.nVertexCount) * 3 * sizeof( float ) == positions.m_nSize &&
		                   static_cast<uint64_t>(current.nIndexCount) * sizeof( uint32_t ) == indices.m_nSize && current.pStaticBVH == bvh.m_pData &&
		                   current.nStaticBVHBytes == bvh.m_nSize && current.pSurfaceMaterials == materials.m_pData &&
		                   current.nSurfaceMaterialBytes == materials.m_nSize && current.pLodHierarchy == hierarchy.m_pData &&
		                   current.nLodHierarchyBytes == hierarchy.m_nSize;
		if ( rCache.release( existing ) != SIRMeshResult::SIR_MESH_OK ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_FAILED;
		}
		if ( !bSame ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		*pHandle = existing;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
	SIRResourcePinHandle pin {};
	const SIRResourceResult ePin = rGraph.pinForProvider( rScope, rSource, rChunks.nProviderId, &pin );
	if ( ePin != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return ePin;
	}
	*pPin = { &rGraph, pin, true };
	const sirMeshGeometrySource_t input = { meshKey,
	                                        reinterpret_cast<const float *>(positions.m_pData),
	                                        reinterpret_cast<const uint32_t *>(indices.m_pData),
	                                        static_cast<uint32_t>(  positions.m_nSize / ( 3 * sizeof( float ) )  ),
	                                        static_cast<uint32_t>(  indices.m_nSize / sizeof( uint32_t )  ),
	                                        pPin,
	                                        &SIRGRDGGeometryBridge::releaseSourcePin,
	                                        bvh.m_pData,
	                                        bvh.m_nSize,
	                                        materials.m_pData,
	                                        materials.m_nSize,
	                                        hierarchy.m_pData,
	                                        hierarchy.m_nSize };
	const SIRMeshResult eInsert = rCache.insert( &input, pHandle );
	if ( eInsert != SIRMeshResult::SIR_MESH_OK ) {
		releaseSourcePin( pPin );
		return eInsert == SIRMeshResult::SIR_MESH_OUT_OF_CAPACITY ? SIRResourceResult::SIR_RESOURCE_RESULT_OUT_OF_BUDGET : SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}
