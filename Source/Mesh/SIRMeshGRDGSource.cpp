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
#include <Mesh/SIRMeshGRDGSource.h>

#include <Resources/SIRResourceGraph.h>

#include <stdint.h>

void SIRMeshGRDGSource::releasePin( void *pContext ) noexcept
{
	sirMeshGRDGSourceLease_t *pLease = ( sirMeshGRDGSourceLease_t * )pContext;
	if ( pLease == nullptr || !pLease->bActive || pLease->pGraph == nullptr )
		return;
	pLease->eLastRelease = pLease->pGraph->releaseProviderPin( pLease->hPin );
	pLease->bActive = false;
	pLease->pGraph = nullptr;
	pLease->hPin = {};
}

sirMeshResult_t SIRMeshGRDGSource::insert( SIRMeshCache& cache, const sirMeshGRDGSourceRequest_t& request, sirMeshGRDGSourceLease_t *pLease,
                                           sirMeshHandle_t *pHandle ) noexcept
{
	if ( pLease == nullptr || pHandle == nullptr || request.pGraph == nullptr || request.pScope == nullptr || request.pView == nullptr ||
	     request.nProviderId == 0 || request.nResourceTypeId == 0 || request.key.nAssetId == 0 || request.key.nRevision == 0 ||
	     request.key.nReserved != 0 || request.nVertexCount == 0 || request.nVertexCount > UINT32_MAX / 3 || request.nIndexCount == 0 ||
	     request.nIndexCount % 3 != 0 || request.positionsChunk.m_nValue == request.indicesChunk.m_nValue )
		return SIR_MESH_INVALID_ARGUMENT;
	if ( pLease->bActive )
		return SIR_MESH_BUSY;
	SIRResourceInfo info = {};
	if ( request.pView->getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_eState != SIR_RESOURCE_STATE_RESIDENT )
		return SIR_MESH_NOT_READY;
	if ( info.m_eKind != SIR_RESOURCE_KIND_GEOMETRY || info.m_Key.m_nAssetId != request.key.nAssetId ||
	     info.m_Key.m_nTypeId != request.nResourceTypeId || info.m_Key.m_nVariantId != request.key.nVariant ||
	     info.m_nRevision != request.key.nRevision )
		return SIR_MESH_STALE_HANDLE;
	SIRResourceConstBytes vertices = {}, indices = {};
	if ( request.pView->getChunk( request.positionsChunk, &vertices ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     request.pView->getChunk( request.indicesChunk, &indices ) != SIR_RESOURCE_RESULT_SUCCESS || vertices.m_pData == nullptr ||
	     indices.m_pData == nullptr || vertices.m_nSize != ( uint64_t )request.nVertexCount * 3 * sizeof( float ) ||
	     indices.m_nSize != ( uint64_t )request.nIndexCount * sizeof( uint32_t ) ||
	     ( ( uintptr_t )vertices.m_pData % alignof( float ) ) != 0 || ( ( uintptr_t )indices.m_pData % alignof( uint32_t ) ) != 0 )
		return SIR_MESH_INVALID_ARGUMENT;
	SIRResourcePinHandle pin = {};
	if ( request.pGraph->pinForProvider( *request.pScope, *request.pView, request.nProviderId, &pin ) != SIR_RESOURCE_RESULT_SUCCESS )
		return SIR_MESH_OUT_OF_CAPACITY;
	pLease->pGraph = request.pGraph;
	pLease->hPin = pin;
	pLease->eLastRelease = SIR_RESOURCE_RESULT_NOT_RESIDENT;
	pLease->bActive = true;
	const sirMeshGeometrySource_t source = { request.key,
	                                         ( const float * )vertices.m_pData,
	                                         ( const uint32_t * )indices.m_pData,
	                                         request.nVertexCount,
	                                         request.nIndexCount,
	                                         pLease,
	                                         releasePin };
	const sirMeshResult_t result = cache.insert( &source, pHandle );
	if ( result != SIR_MESH_OK )
		releasePin( pLease );
	return result;
}
