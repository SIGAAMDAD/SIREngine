/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Mesh/SIRVGCPUPageViews.h>

#include <Resources/SIRResourceViews.h>
#include <stdint.h>

SIRVGCPUPageViews::SIRVGCPUPageViews( void ) noexcept : m_Config{}
{
}

sirVGResidencyResult_t SIRVGCPUPageViews::initialize( const sirVGCPUPageConfig_t& config ) noexcept
{
	if ( m_Config.pSchema != nullptr || config.pSchema == nullptr || !config.pSchema->isOpen() || config.pDirectory == nullptr ||
	     config.pArena == nullptr || config.nArenaBytes == 0 || config.nPhysicalBackingId == 0 ||
	     ( ( uintptr_t )config.pArena % alignof( sirMeshPackVertex_t ) ) != 0 ||
	     config.pDirectory->getBackingId() != config.nPhysicalBackingId ||
	     config.pDirectory->getBackingCapacityBytes() != config.nArenaBytes ) {
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	}
	m_Config = config;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGCPUPageViews::install( uint32_t nPage, const SIRResourceConstBytes& cooked, uint32_t nRequestEpoch ) noexcept
{
	if ( m_Config.pSchema == nullptr || cooked.m_pData == nullptr || nRequestEpoch == 0 ||
	     m_Config.pDirectory->getRequestEpoch() != nRequestEpoch ) {
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	}
	if ( m_Config.pDirectory->getState( nPage ) != SIR_VG_PAGE_MISSING ) {
		return SIR_VG_RESIDENCY_BUSY;
	}
	// A decoded derivative must never overwrite canonical GRDG/SIRPack bytes.
	const uintptr_t nSource = ( uintptr_t )cooked.m_pData;
	const uintptr_t nDestination = ( uintptr_t )m_Config.pArena;
	if ( cooked.m_nSize > UINTPTR_MAX - nSource || m_Config.nArenaBytes > UINTPTR_MAX - nDestination ) {
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	}
	if ( nSource < nDestination + m_Config.nArenaBytes && nDestination < nSource + cooked.m_nSize ) {
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	}
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	if ( m_Config.pSchema->getDecodedPageCounts( nPage, &nVertices, &nIndices ) != SIR_MESH_PACK_OK ) {
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	const uint64_t nVertexBytes = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t );
	const uint64_t nNativeBytes = nVertexBytes + ( uint64_t )nIndices * sizeof( uint32_t );
	uint64_t nOffset = 0;
	const sirVGResidencyResult_t eFree = m_Config.pDirectory->findFreeSpan( nNativeBytes, 16, &nOffset );
	if ( eFree != SIR_VG_RESIDENCY_OK ) {
		return eFree;
	}
	if ( nOffset > m_Config.nArenaBytes || nNativeBytes > m_Config.nArenaBytes - nOffset ) {
		return SIR_VG_RESIDENCY_OUT_OF_BUDGET;
	}
	const sirVGPageAdmission_t admission = { cooked.m_pData,
	                                         cooked.m_nSize,
	                                         nNativeBytes,
	                                         m_Config.nPhysicalBackingId,
	                                         nOffset,
	                                         1,
	                                         0,
	                                         0,
	                                         nPage,
	                                         m_Config.pDirectory->getDeviceGeneration(),
	                                         nRequestEpoch };
	const sirVGResidencyResult_t eAdmission = m_Config.pDirectory->admit( admission );
	if ( eAdmission != SIR_VG_RESIDENCY_OK ) {
		return eAdmission;
	}
	const sirMeshPackPageDecode_t decode = { cooked.m_pData,
	                                         cooked.m_nSize,
	                                         nPage,
	                                         ( sirMeshPackVertex_t * )( m_Config.pArena + nOffset ),
	                                         ( uint32_t * )( m_Config.pArena + nOffset + nVertexBytes ),
	                                         nVertices,
	                                         nIndices };
	if ( m_Config.pSchema->decodePage( decode ) != SIR_MESH_PACK_OK ) {
		( void )m_Config.pDirectory->complete( { 1, 0, 0, nPage, admission.nDeviceGeneration, nRequestEpoch, false } );
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	return m_Config.pDirectory->complete( { 1, 0, 0, nPage, admission.nDeviceGeneration, nRequestEpoch, true } );
}

sirVGResidencyResult_t SIRVGCPUPageViews::installGRDG( const SIRResourceView& source, uint32_t nPage, uint32_t nRequestEpoch ) noexcept
{
	if ( m_Config.pSchema == nullptr ) {
		return SIR_VG_RESIDENCY_NOT_READY;
	}
	const sirMeshKey_t key = m_Config.pSchema->getKey();
	SIRResourceInfo info = {};
	if ( source.getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS || info.m_eKind != SIR_RESOURCE_KIND_GEOMETRY ||
	     info.m_eState != SIR_RESOURCE_STATE_RESIDENT || info.m_Key.m_nAssetId != key.nAssetId || info.m_nRevision != key.nRevision ||
	     info.m_Key.m_nVariantId != key.nVariant ) {
		return SIR_VG_RESIDENCY_STALE;
	}
	sirMeshPackPage_t page = {};
	if ( m_Config.pSchema->pageAt( nPage, &page ) != SIR_MESH_PACK_OK ) {
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	SIRResourceConstBytes bytes = {};
	if ( source.getChunk( { page.nChunkId }, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS || bytes.m_nSize != page.nByteCount ) {
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	return install( nPage, bytes, nRequestEpoch );
}

sirVGResidencyResult_t SIRVGCPUPageViews::acquire( uint32_t nPage, sirVGCPUPageLease_t *pLease ) noexcept
{
	if ( m_Config.pSchema == nullptr || pLease == nullptr ) {
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	}
	sirVGPagePin_t pin = {};
	const sirVGResidencyResult_t ePin = m_Config.pDirectory->acquire( nPage, &pin );
	if ( ePin != SIR_VG_RESIDENCY_OK ) {
		return ePin;
	}
	sirVGPageResidentView_t resident = {};
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	if ( m_Config.pDirectory->getResidentView( nPage, &resident ) != SIR_VG_RESIDENCY_OK ||
	     m_Config.pSchema->getDecodedPageCounts( nPage, &nVertices, &nIndices ) != SIR_MESH_PACK_OK ||
	     resident.nPhysicalBackingId != m_Config.nPhysicalBackingId || resident.nNativeOffset > m_Config.nArenaBytes ) {
		( void )m_Config.pDirectory->release( pin );
		return SIR_VG_RESIDENCY_STALE;
	}
	const uint64_t nVertexBytes = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t );
	const uint64_t nBytes = nVertexBytes + ( uint64_t )nIndices * sizeof( uint32_t );
	if ( nBytes > resident.nNativeBytes || nBytes > m_Config.nArenaBytes - resident.nNativeOffset ) {
		( void )m_Config.pDirectory->release( pin );
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	pLease->pVertices = ( const sirMeshPackVertex_t * )( m_Config.pArena + resident.nNativeOffset );
	pLease->pIndices = ( const uint32_t * )( m_Config.pArena + resident.nNativeOffset + nVertexBytes );
	pLease->nVertices = nVertices;
	pLease->nIndices = nIndices;
	pLease->nPhysicalBackingId = resident.nPhysicalBackingId;
	pLease->nSourceRevision = m_Config.pSchema->getKey().nRevision;
	pLease->pin = pin;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGCPUPageViews::release( const sirVGCPUPageLease_t& lease ) noexcept
{
	if ( m_Config.pDirectory == nullptr || lease.nPhysicalBackingId != m_Config.nPhysicalBackingId ||
	     lease.nSourceRevision != m_Config.pSchema->getKey().nRevision ) {
		return SIR_VG_RESIDENCY_STALE;
	}
	return m_Config.pDirectory->release( lease.pin );
}

sirVGResidencyResult_t SIRVGCPUPageViews::shutdown( void ) noexcept
{
	if ( m_Config.pSchema == nullptr ) {
		return SIR_VG_RESIDENCY_NOT_READY;
	}
	m_Config = {};
	return SIR_VG_RESIDENCY_OK;
}
