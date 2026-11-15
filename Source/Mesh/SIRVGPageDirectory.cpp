/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Mesh/SIRVGPageDirectory.h>

#include <Resources/SIRPackFormat.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

SIRVGPageDirectory::SIRVGPageDirectory( void ) noexcept
    : m_pSchema( nullptr ), m_pPages( nullptr ), m_pClusterDraws( nullptr ), m_Arena{}, m_nCommittedBytes( 0 ), m_nPageCount( 0 ),
      m_nClusterCount( 0 ), m_nSnapshotGeneration( 0 )
{
}

SIRVGPageDirectory::~SIRVGPageDirectory( void ) noexcept
{
	assert( !isReady() && "SIRVGPageDirectory requires explicit shutdown" );
}

bool SIRVGPageDirectory::isReady( void ) const noexcept
{
	return m_pSchema != nullptr && m_pPages != nullptr && m_pClusterDraws != nullptr;
}

uint32_t SIRVGPageDirectory::getSnapshotGeneration( void ) const noexcept
{
	return m_nSnapshotGeneration;
}
uint32_t SIRVGPageDirectory::getDeviceGeneration( void ) const noexcept
{
	return m_Arena.nDeviceGeneration;
}
uint32_t SIRVGPageDirectory::getRequestEpoch( void ) const noexcept
{
	return m_Arena.nRequestEpoch;
}
uint64_t SIRVGPageDirectory::getCommittedBytes( void ) const noexcept
{
	return m_nCommittedBytes;
}
uint64_t SIRVGPageDirectory::getBackingCapacityBytes( void ) const noexcept
{
	return isReady() ? m_Arena.nBackingCapacityBytes : 0;
}

uint64_t SIRVGPageDirectory::getBackingId( void ) const noexcept
{
	return m_Arena.nPhysicalBackingId;
}
bool SIRVGPageDirectory::isResident( uint32_t nPage ) const noexcept
{
	return isReady() && nPage < m_nPageCount && m_pPages[nPage].eState == SIR_VG_PAGE_RESIDENT &&
	       m_pPages[nPage].nDeviceGeneration == m_Arena.nDeviceGeneration;
}
bool SIRVGPageDirectory::isCurrentSnapshot( uint32_t nGeneration ) const noexcept
{
	return isReady() && nGeneration != 0 && nGeneration == m_nSnapshotGeneration;
}
sirVGPageState_t SIRVGPageDirectory::getState( uint32_t nPage ) const noexcept
{
	if ( !isReady() || nPage >= m_nPageCount )
		return SIR_VG_PAGE_MISSING;
	return m_pPages[nPage].eState;
}

bool SIRVGPageDirectory::bumpSnapshot( void ) noexcept
{
	if ( m_nSnapshotGeneration == UINT32_MAX )
		return false;
	++m_nSnapshotGeneration;
	return true;
}

sirVGResidencyResult_t SIRVGPageDirectory::initialize( const SIRMeshPackSchema& schema, const sirVGPageStorage_t& storage,
                                                       const sirVGPageArena_t& arena ) noexcept
{
	if ( isReady() )
		return SIR_VG_RESIDENCY_BUSY;
	if ( !schema.isOpen() || storage.pPages == nullptr || storage.pClusterDraws == nullptr ||
	     storage.nPageCapacity < schema.getPageCount() || storage.nClusterCapacity < schema.getClusterCount() ||
	     arena.nPhysicalBackingId == 0 || arena.nDeviceGeneration == 0 || arena.nRequestEpoch == 0 || arena.nBackingCapacityBytes == 0 ||
	     arena.nResidentBudgetBytes == 0 || arena.nResidentBudgetBytes > arena.nBackingCapacityBytes )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;

	// The manifest records compressed cooked bytes, while the sealed device
	// budget is charged against the physical native reservation at admission.
	// Rejecting a root by its cooked size can exclude a legal resident root.
	sirMeshPackPage_t root = {};
	if ( schema.pageAt( 0, &root ) != SIR_MESH_PACK_OK )
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	uint32_t nRootVertices = 0;
	uint32_t nRootIndices = 0;
	if ( schema.getDecodedPageCounts( 0, &nRootVertices, &nRootIndices ) != SIR_MESH_PACK_OK ) {
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	const uint64_t nRootNativeBytes =
	    ( uint64_t )nRootVertices * sizeof( sirMeshPackVertex_t ) + ( uint64_t )nRootIndices * sizeof( uint32_t );
	if ( nRootNativeBytes > arena.nBackingCapacityBytes || nRootNativeBytes > arena.nResidentBudgetBytes ) {
		return SIR_VG_RESIDENCY_OUT_OF_BUDGET;
	}

	// Calculate the legal decoded page-local index/vertex offsets before
	// publication. Reject any layout that exceeds native indexed arguments.
	for ( uint32_t iPage = 0; iPage < schema.getPageCount(); ++iPage ) {
		sirMeshPackPage_t page = {};
		if ( schema.pageAt( iPage, &page ) != SIR_MESH_PACK_OK )
			return SIR_VG_RESIDENCY_BAD_SOURCE;
		uint32_t nFirstIndex = 0;
		uint32_t nFirstVertex = 0;
		for ( uint32_t j = 0; j < page.nClusterCount; ++j ) {
			sirMeshPackCluster_t cluster = {};
			if ( schema.clusterAt( page.nFirstCluster + j, &cluster ) != SIR_MESH_PACK_OK || cluster.nTriangleCount > UINT32_MAX / 3 ||
			     nFirstIndex > UINT32_MAX - 3 * cluster.nTriangleCount || nFirstVertex > UINT32_MAX - cluster.nVertexCount )
				return SIR_VG_RESIDENCY_BAD_SOURCE;
			nFirstIndex += cluster.nTriangleCount * 3;
			nFirstVertex += cluster.nVertexCount;
		}
	}
	m_pSchema = &schema;
	m_pPages = storage.pPages;
	m_pClusterDraws = storage.pClusterDraws;
	m_nPageCount = schema.getPageCount();
	m_nClusterCount = schema.getClusterCount();
	m_Arena = arena;
	m_nCommittedBytes = 0;
	m_nSnapshotGeneration = 1;
	memset( m_pPages, 0, ( size_t )m_nPageCount * sizeof( *m_pPages ) );
	memset( m_pClusterDraws, 0, ( size_t )m_nClusterCount * sizeof( *m_pClusterDraws ) );
	for ( uint32_t iPage = 0; iPage < m_nPageCount; ++iPage ) {
		sirMeshPackPage_t page = {};
		( void )schema.pageAt( iPage, &page );
		uint32_t nFirstIndex = 0;
		uint32_t nFirstVertex = 0;
		for ( uint32_t j = 0; j < page.nClusterCount; ++j ) {
			sirMeshPackCluster_t cluster = {};
			( void )schema.clusterAt( page.nFirstCluster + j, &cluster );
			sirVGClusterDrawLayout_t& layout = m_pClusterDraws[page.nFirstCluster + j];
			layout.nFirstIndex = nFirstIndex;
			layout.nIndexCount = cluster.nTriangleCount * 3;
			layout.nFirstVertex = nFirstVertex;
			layout.nMaterialId = UINT32_MAX;
			nFirstIndex += layout.nIndexCount;
			nFirstVertex += cluster.nVertexCount;
		}
	}
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::advanceEpoch( uint32_t nNextEpoch ) noexcept
{
	if ( !isReady() || nNextEpoch <= m_Arena.nRequestEpoch )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	m_Arena.nRequestEpoch = nNextEpoch;
	return SIR_VG_RESIDENCY_OK;
}

bool SIRVGPageDirectory::canReserve( const sirVGPageAdmission_t& admission ) const noexcept
{
	if ( admission.nNativeBytes > m_Arena.nResidentBudgetBytes - m_nCommittedBytes ||
	     admission.nNativeBytes > m_Arena.nBackingCapacityBytes - admission.nNativeOffset )
		return false;
	const uint64_t nEnd = admission.nNativeOffset + admission.nNativeBytes;
	for ( uint32_t iPage = 0; iPage < m_nPageCount; ++iPage ) {
		const sirVGPageSlot_t& slot = m_pPages[iPage];
		if ( slot.eState == SIR_VG_PAGE_MISSING || iPage == admission.nPage )
			continue;
		if ( admission.nNativeOffset < slot.nNativeOffset + slot.nBytes && slot.nNativeOffset < nEnd )
			return false;
	}
	return true;
}

sirVGResidencyResult_t SIRVGPageDirectory::admit( const sirVGPageAdmission_t& admission ) noexcept
{
	if ( !isReady() || admission.nPage >= m_nPageCount || admission.pCookedBytes == nullptr || admission.nCookedBytes == 0 ||
	     admission.nNativeBytes == 0 || admission.nNativeOffset > m_Arena.nBackingCapacityBytes ||
	     admission.nPhysicalBackingId != m_Arena.nPhysicalBackingId )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	if ( admission.nDeviceGeneration != m_Arena.nDeviceGeneration || admission.nRequestEpoch != m_Arena.nRequestEpoch )
		return SIR_VG_RESIDENCY_STALE;
	if ( admission.nCopyFence == 0 )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[admission.nPage];
	if ( slot.eState != SIR_VG_PAGE_MISSING )
		return SIR_VG_RESIDENCY_BUSY;
	if ( slot.nGeneration == UINT32_MAX || m_nSnapshotGeneration == UINT32_MAX )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	sirMeshPackPage_t page = {};
	if ( m_pSchema->pageAt( admission.nPage, &page ) != SIR_MESH_PACK_OK || page.nByteCount != admission.nCookedBytes )
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	uint32_t nDecodedVertices = 0;
	uint32_t nDecodedIndices = 0;
	if ( m_pSchema->getDecodedPageCounts( admission.nPage, &nDecodedVertices, &nDecodedIndices ) != SIR_MESH_PACK_OK ) {
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	const uint64_t nRequiredNativeBytes =
	    ( uint64_t )nDecodedVertices * sizeof( sirMeshPackVertex_t ) + ( uint64_t )nDecodedIndices * sizeof( uint32_t );
	if ( admission.nNativeBytes < nRequiredNativeBytes ) {
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	}
	if ( admission.nPage != 0 && !isResident( page.nParent ) )
		return SIR_VG_RESIDENCY_NOT_READY;
	if ( !canReserve( admission ) )
		return SIR_VG_RESIDENCY_OUT_OF_BUDGET;
	if ( m_pSchema->verifyPage( admission.nPage, admission.pCookedBytes, admission.nCookedBytes ) != SIR_MESH_PACK_OK )
		return SIR_VG_RESIDENCY_BAD_SOURCE;
	const uint8_t *pBytes = ( const uint8_t * )admission.pCookedBytes;
	for ( uint32_t i = 0; i < page.nClusterCount; ++i ) {
		sirMeshPackCluster_t cluster = {};
		( void )m_pSchema->clusterAt( page.nFirstCluster + i, &cluster );
		m_pClusterDraws[page.nFirstCluster + i].nMaterialId = sirPackRead32( pBytes + cluster.nByteOffset + 4 );
	}
	slot.nNativeOffset = admission.nNativeOffset;
	slot.nBytes = admission.nNativeBytes;
	slot.nCopyFence = admission.nCopyFence;
	slot.nComputeFence = admission.nComputeFence;
	slot.nGraphicsFence = admission.nGraphicsFence;
	slot.nLastGraphicsUseFence = 0;
	slot.nDeviceGeneration = admission.nDeviceGeneration;
	slot.nRequestEpoch = admission.nRequestEpoch;
	++slot.nGeneration;
	slot.eState = SIR_VG_PAGE_UPLOADING;
	m_nCommittedBytes += slot.nBytes;
	return SIR_VG_RESIDENCY_OK;
}

void SIRVGPageDirectory::discardSlot( uint32_t nPage ) noexcept
{
	sirVGPageSlot_t& slot = m_pPages[nPage];
	assert( slot.nBytes <= m_nCommittedBytes );
	m_nCommittedBytes -= slot.nBytes;
	const uint32_t nGeneration = slot.nGeneration;
	memset( &slot, 0, sizeof( slot ) );
	slot.nGeneration = nGeneration;
}

sirVGResidencyResult_t SIRVGPageDirectory::complete( const sirVGPageCompletion_t& completion ) noexcept
{
	if ( !isReady() || completion.nPage >= m_nPageCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[completion.nPage];
	if ( slot.eState != SIR_VG_PAGE_UPLOADING )
		return SIR_VG_RESIDENCY_NOT_READY;
	if ( completion.nDeviceGeneration != slot.nDeviceGeneration || completion.nRequestEpoch != slot.nRequestEpoch )
		return SIR_VG_RESIDENCY_STALE;
	if ( completion.nCompletedCopyFence < slot.nCopyFence || completion.nCompletedComputeFence < slot.nComputeFence ||
	     completion.nCompletedGraphicsFence < slot.nGraphicsFence )
		return SIR_VG_RESIDENCY_NOT_READY;
	if ( !completion.bSuccess || slot.nRequestEpoch != m_Arena.nRequestEpoch ) {
		if ( !bumpSnapshot() )
			return SIR_VG_RESIDENCY_EXHAUSTED;
		discardSlot( completion.nPage );
		return SIR_VG_RESIDENCY_STALE;
	}
	if ( !bumpSnapshot() )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	slot.eState = SIR_VG_PAGE_RESIDENT;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::attachSubmission( uint32_t nPage, uint32_t nRequestEpoch, uint64_t nCopyFence ) noexcept
{
	if ( !isReady() || nPage >= m_nPageCount || nCopyFence == 0 || nCopyFence == UINT64_MAX )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( slot.nRequestEpoch != nRequestEpoch || slot.nDeviceGeneration != m_Arena.nDeviceGeneration )
		return SIR_VG_RESIDENCY_STALE;
	if ( slot.eState != SIR_VG_PAGE_UPLOADING || slot.nCopyFence != UINT64_MAX )
		return SIR_VG_RESIDENCY_BUSY;
	slot.nCopyFence = nCopyFence;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::abortAdmission( uint32_t nPage, uint32_t nRequestEpoch ) noexcept
{
	if ( !isReady() || nPage >= m_nPageCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( slot.nRequestEpoch != nRequestEpoch || slot.nDeviceGeneration != m_Arena.nDeviceGeneration )
		return SIR_VG_RESIDENCY_STALE;
	// A submitted command must be allowed to finish before its span is reused.
	if ( slot.eState != SIR_VG_PAGE_UPLOADING || slot.nCopyFence != UINT64_MAX )
		return SIR_VG_RESIDENCY_BUSY;
	if ( !bumpSnapshot() )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	discardSlot( nPage );
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::findFreeSpan( uint64_t nNativeBytes, uint64_t nAlignment, uint64_t *pOffset ) const noexcept
{
	if ( !isReady() || pOffset == nullptr || nNativeBytes == 0 || nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	if ( nNativeBytes > m_Arena.nBackingCapacityBytes || nNativeBytes > m_Arena.nResidentBudgetBytes - m_nCommittedBytes )
		return SIR_VG_RESIDENCY_OUT_OF_BUDGET;
	uint64_t nCandidate = 0;
	while ( nCandidate <= m_Arena.nBackingCapacityBytes - nNativeBytes ) {
		bool bCollision = false;
		uint64_t nNext = nCandidate;
		for ( uint32_t i = 0; i < m_nPageCount; ++i ) {
			const sirVGPageSlot_t& slot = m_pPages[i];
			if ( slot.eState == SIR_VG_PAGE_MISSING )
				continue;
			if ( slot.nNativeOffset >= nCandidate + nNativeBytes || slot.nNativeOffset + slot.nBytes <= nCandidate )
				continue;
			bCollision = true;
			const uint64_t nEnd = slot.nNativeOffset + slot.nBytes;
			if ( nEnd > nNext )
				nNext = nEnd;
		}
		if ( !bCollision ) {
			*pOffset = nCandidate;
			return SIR_VG_RESIDENCY_OK;
		}
		if ( nNext > UINT64_MAX - ( nAlignment - 1 ) )
			break;
		nCandidate = ( nNext + nAlignment - 1 ) & ~( nAlignment - 1 );
	}
	return SIR_VG_RESIDENCY_OUT_OF_BUDGET;
}

sirVGResidencyResult_t SIRVGPageDirectory::getResidentView( uint32_t nPage, sirVGPageResidentView_t *pView ) const noexcept
{
	if ( pView == nullptr || !isReady() || nPage >= m_nPageCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	const sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( slot.eState != SIR_VG_PAGE_RESIDENT || slot.nDeviceGeneration != m_Arena.nDeviceGeneration )
		return SIR_VG_RESIDENCY_NOT_READY;
	*pView = { m_Arena.nPhysicalBackingId, slot.nNativeOffset,     slot.nBytes,
	           slot.nGeneration,           slot.nDeviceGeneration, m_nSnapshotGeneration };
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::getClusterLayout( uint32_t nCluster, sirVGClusterDrawLayout_t *pLayout ) const noexcept
{
	if ( !isReady() || pLayout == nullptr || nCluster >= m_nClusterCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirMeshPackCluster_t cluster = {};
	if ( m_pSchema->clusterAt( nCluster, &cluster ) != SIR_MESH_PACK_OK || !isResident( cluster.nPage ) )
		return SIR_VG_RESIDENCY_NOT_READY;
	*pLayout = m_pClusterDraws[nCluster];
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::acquire( uint32_t nPage, sirVGPagePin_t *pPin ) noexcept
{
	if ( pPin == nullptr || !isResident( nPage ) )
		return SIR_VG_RESIDENCY_NOT_READY;
	sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( slot.nReaders == UINT32_MAX )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	++slot.nReaders;
	*pPin = { nPage, slot.nGeneration, slot.nDeviceGeneration };
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::release( const sirVGPagePin_t& pin ) noexcept
{
	if ( !isReady() || pin.nPage >= m_nPageCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[pin.nPage];
	if ( slot.eState != SIR_VG_PAGE_RESIDENT || slot.nGeneration != pin.nGeneration || slot.nDeviceGeneration != pin.nDeviceGeneration ||
	     slot.nReaders == 0 )
		return SIR_VG_RESIDENCY_STALE;
	--slot.nReaders;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::markSubmitted( uint32_t nPage, uint64_t nGraphicsFence ) noexcept
{
	if ( !isResident( nPage ) || nGraphicsFence == 0 )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( nGraphicsFence < slot.nLastGraphicsUseFence )
		return SIR_VG_RESIDENCY_STALE;
	slot.nLastGraphicsUseFence = nGraphicsFence;
	return SIR_VG_RESIDENCY_OK;
}

bool SIRVGPageDirectory::hasLiveChildren( uint32_t nPage ) const noexcept
{
	for ( uint32_t i = nPage + 1; i < m_nPageCount; ++i ) {
		if ( m_pPages[i].eState == SIR_VG_PAGE_MISSING )
			continue;
		sirMeshPackPage_t page = {};
		if ( m_pSchema->pageAt( i, &page ) == SIR_MESH_PACK_OK && page.nParent == nPage )
			return true;
	}
	return false;
}

sirVGResidencyResult_t SIRVGPageDirectory::beginRetire( uint32_t nPage ) noexcept
{
	if ( !isReady() || nPage == 0 || nPage >= m_nPageCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( slot.eState != SIR_VG_PAGE_RESIDENT )
		return SIR_VG_RESIDENCY_NOT_READY;
	if ( slot.nReaders != 0 || hasLiveChildren( nPage ) )
		return SIR_VG_RESIDENCY_BUSY;
	if ( !bumpSnapshot() )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	slot.eState = SIR_VG_PAGE_RETIRING;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::finishRetire( uint32_t nPage, uint64_t nCompletedGraphicsFence ) noexcept
{
	if ( !isReady() || nPage == 0 || nPage >= m_nPageCount )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	const sirVGPageSlot_t& slot = m_pPages[nPage];
	if ( slot.eState != SIR_VG_PAGE_RETIRING )
		return SIR_VG_RESIDENCY_NOT_READY;
	if ( slot.nReaders != 0 || nCompletedGraphicsFence < slot.nLastGraphicsUseFence )
		return SIR_VG_RESIDENCY_BUSY;
	if ( !bumpSnapshot() )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	discardSlot( nPage );
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::resetDevice( const sirVGPageArena_t& arena ) noexcept
{
	if ( !isReady() || arena.nDeviceGeneration <= m_Arena.nDeviceGeneration || arena.nPhysicalBackingId == 0 ||
	     arena.nBackingCapacityBytes == 0 || arena.nResidentBudgetBytes == 0 || arena.nResidentBudgetBytes > arena.nBackingCapacityBytes ||
	     arena.nRequestEpoch == 0 )
		return SIR_VG_RESIDENCY_INVALID_ARGUMENT;
	for ( uint32_t i = 0; i < m_nPageCount; ++i ) {
		if ( m_pPages[i].nReaders != 0 )
			return SIR_VG_RESIDENCY_BUSY;
	}
	if ( !bumpSnapshot() )
		return SIR_VG_RESIDENCY_EXHAUSTED;
	for ( uint32_t i = 0; i < m_nPageCount; ++i )
		discardSlot( i );
	m_Arena = arena;
	return SIR_VG_RESIDENCY_OK;
}

sirVGResidencyResult_t SIRVGPageDirectory::shutdown( void ) noexcept
{
	if ( !isReady() )
		return SIR_VG_RESIDENCY_NOT_READY;
	for ( uint32_t i = 0; i < m_nPageCount; ++i ) {
		if ( m_pPages[i].nReaders != 0 || m_pPages[i].eState == SIR_VG_PAGE_UPLOADING || m_pPages[i].eState == SIR_VG_PAGE_RETIRING )
			return SIR_VG_RESIDENCY_BUSY;
	}
	m_pSchema = nullptr;
	m_pPages = nullptr;
	m_pClusterDraws = nullptr;
	m_Arena = {};
	m_nCommittedBytes = 0;
	m_nPageCount = 0;
	m_nClusterCount = 0;
	m_nSnapshotGeneration = 0;
	return SIR_VG_RESIDENCY_OK;
}
