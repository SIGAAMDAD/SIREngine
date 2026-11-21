#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_cache.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"
#include <string.h>

static int t3d_comp_surf_compare_page( const t3dComposedSurfacePageRecord_t& a, const t3dComposedSurfacePageRecord_t& b )
{
	if ( a.m_nRegionKey != b.m_nRegionKey )
		return a.m_nRegionKey < b.m_nRegionKey ? -1 : 1;
	if ( a.m_nMip != b.m_nMip )
		return a.m_nMip < b.m_nMip ? -1 : 1;
	if ( a.m_nPageY != b.m_nPageY )
		return a.m_nPageY < b.m_nPageY ? -1 : 1;
	if ( a.m_nPageX != b.m_nPageX )
		return a.m_nPageX < b.m_nPageX ? -1 : 1;
	return 0;
}

bool t3d_comp_surf_sidecar_record( const t3dComposedSurfaceSidecarView_t& view, const uint32_t nPageIndex,
                                   t3dComposedSurfacePageRecord_t& out )
{
	if ( !view.m_pFile || nPageIndex >= view.m_header.m_nPageCount )
		return false;
	const uint64_t offset = view.m_header.m_nRecordOffset + uint64_t( nPageIndex ) * sizeof( out );
	if ( !t3d_comp_surf_range_within( offset, sizeof( out ), view.m_nFileBytes ) )
		return false;
	memcpy( &out, view.m_pFile + offset, sizeof( out ) );
	return true;
}

bool t3d_comp_surf_open_sidecar_cold( const uint8_t *pBytes, const size_t nBytes, const uint64_t nExpectedSourceFingerprint,
                                      t3dComposedSurfaceSidecarView_t& out )
{
	out = {};
	if ( !t3d_comp_surf_validate_sidecar_bytes( pBytes, nBytes, false ) )
		return false;
	t3dComposedSurfaceSidecarView_t next;
	next.m_pFile = pBytes;
	next.m_nFileBytes = nBytes;
	memcpy( &next.m_header, pBytes, sizeof( next.m_header ) );
	if ( !nExpectedSourceFingerprint || next.m_header.m_nSourceFingerprint != nExpectedSourceFingerprint )
		return false;
	t3dComposedSurfacePageRecord_t previous{}, current{};
	for ( uint32_t i = 0u; i < next.m_header.m_nPageCount; ++i ) {
		if ( !t3d_comp_surf_sidecar_record( next, i, current ) || ( i && t3d_comp_surf_compare_page( previous, current ) >= 0 ) )
			return false;
		previous = current;
	}
	out = next;
	return true;
}

bool t3d_comp_surf_find_page( const t3dComposedSurfaceSidecarView_t& view, const uint64_t nRegionKey, const uint32_t nMip,
                              const uint32_t nX, const uint32_t nY, const uint64_t nRevision, uint32_t& rPageIndex )
{
	rPageIndex = T3D_COMP_SURF_INVALID_SLOT;
	if ( !nRevision || !t3d_comp_surf_find_page_location( view, nRegionKey, nMip, nX, nY, rPageIndex ) )
		return false;
	t3dComposedSurfacePageRecord_t found{};
	if ( !t3d_comp_surf_sidecar_record( view, rPageIndex, found ) || found.m_nCompositionRevision != nRevision ) {
		rPageIndex = T3D_COMP_SURF_INVALID_SLOT;
		return false;
	}
	return true;
}

bool t3d_comp_surf_find_page_location( const t3dComposedSurfaceSidecarView_t& view, const uint64_t nRegionKey, const uint32_t nMip,
                                       const uint32_t nX, const uint32_t nY, uint32_t& rPageIndex )
{
	rPageIndex = T3D_COMP_SURF_INVALID_SLOT;
	if ( !nRegionKey || !view.m_pFile )
		return false;
	t3dComposedSurfacePageRecord_t key{};
	key.m_nRegionKey = nRegionKey;
	key.m_nMip = nMip;
	key.m_nPageX = nX;
	key.m_nPageY = nY;
	uint32_t low = 0u, high = view.m_header.m_nPageCount;
	while ( low < high ) {
		const uint32_t mid = low + ( high - low ) / 2u;
		t3dComposedSurfacePageRecord_t current{};
		if ( !t3d_comp_surf_sidecar_record( view, mid, current ) )
			return false;
		if ( t3d_comp_surf_compare_page( current, key ) < 0 )
			low = mid + 1u;
		else
			high = mid;
	}
	t3dComposedSurfacePageRecord_t found{};
	if ( !t3d_comp_surf_sidecar_record( view, low, found ) || t3d_comp_surf_compare_page( found, key ) != 0 )
		return false;
	rPageIndex = low;
	return true;
}

bool Terrain3DComposedSurfaceCache::initializeCold( const t3dComposedSurfaceSidecarView_t& view, t3dComposedSurfaceSlot_t *pSlots,
                                                    const uint32_t nSlots, t3dComposedSurfaceRequest_t *pQueue,
                                                    const uint32_t nQueueCapacity, const uint32_t nDeviceGeneration )
{
	if ( !view.m_pFile || !pSlots || !pQueue || !nSlots || !nQueueCapacity || !nDeviceGeneration )
		return false;
	m_view = view;
	m_pSlots = pSlots;
	m_pRequests = pQueue;
	m_nCapacity = nSlots;
	m_nRequestCapacity = nQueueCapacity;
	resetDeviceCold( nDeviceGeneration );
	return true;
}

void Terrain3DComposedSurfaceCache::resetDeviceCold( const uint32_t nDeviceGeneration )
{
	m_nDeviceGeneration = nDeviceGeneration;
	m_stats = {};
	if ( m_pSlots ) {
		for ( uint32_t i = 0u; i < m_nCapacity; ++i )
			m_pSlots[i] = {};
	}
	if ( m_pRequests ) {
		for ( uint32_t i = 0u; i < m_nRequestCapacity; ++i )
			m_pRequests[i] = {};
	}
	// m_nNextToken is deliberately not reset on device change.
}

uint32_t Terrain3DComposedSurfaceCache::locate( const uint32_t nPageIndex ) const
{
	for ( uint32_t i = 0u; i < m_nCapacity; ++i ) {
		if ( m_pSlots[i].m_nState != T3D_COMP_SURF_SLOT_FREE && m_pSlots[i].m_nPageIndex == nPageIndex )
			return i;
	}
	return T3D_COMP_SURF_INVALID_SLOT;
}

bool Terrain3DComposedSurfaceCache::demandHot( const uint32_t nPageIndex, const uint32_t nPriority, const uint64_t nTouch )
{
	if ( !m_pSlots || !m_nDeviceGeneration || nPageIndex >= m_view.m_header.m_nPageCount )
		return false;
	const uint32_t slot = locate( nPageIndex );
	if ( slot != T3D_COMP_SURF_INVALID_SLOT ) {
		if ( m_pSlots[slot].m_nState == T3D_COMP_SURF_SLOT_RESIDENT ) {
			++m_stats.m_nHits;
			m_pSlots[slot].m_nLastTouch = nTouch;
			return true;
		}
		if ( m_pSlots[slot].m_nState != T3D_COMP_SURF_SLOT_RETIRING ) {
			++m_stats.m_nDeduplicated;
			return true;
		}
	}
	for ( uint32_t i = 0u; i < m_stats.m_nPending; ++i ) {
		if ( m_pRequests[i].m_nPageIndex == nPageIndex ) {
			if ( nPriority > m_pRequests[i].m_nPriority )
				m_pRequests[i].m_nPriority = nPriority;
			++m_stats.m_nDeduplicated;
			return true;
		}
	}
	if ( m_stats.m_nPending >= m_nRequestCapacity )
		return false;
	m_pRequests[m_stats.m_nPending++] = { nPageIndex, nPriority };
	++m_stats.m_nMisses;
	return true;
}

bool Terrain3DComposedSurfaceCache::owns( const t3dComposedSurfaceJob_t& job, const uint32_t nState ) const
{
	if ( job.m_nSlot >= m_nCapacity || !job.m_nToken || !m_nDeviceGeneration || job.m_nDeviceGeneration != m_nDeviceGeneration )
		return false;
	const t3dComposedSurfaceSlot_t& slot = m_pSlots[job.m_nSlot];
	return slot.m_nToken == job.m_nToken && slot.m_nPageIndex == job.m_nPageIndex && slot.m_nDeviceGeneration == job.m_nDeviceGeneration &&
	       slot.m_nState == nState;
}

bool Terrain3DComposedSurfaceCache::beginEvictionHot( t3dComposedSurfaceJob_t& rRetirement )
{
	rRetirement = {};
	if ( !m_pSlots || m_stats.m_nPending == 0u )
		return false;
	uint32_t victim = T3D_COMP_SURF_INVALID_SLOT;
	for ( uint32_t i = 0u; i < m_nCapacity; ++i ) {
		const t3dComposedSurfaceSlot_t& slot = m_pSlots[i];
		if ( slot.m_nState == T3D_COMP_SURF_SLOT_FREE )
			return false; // Can dispatch instead.
		if ( slot.m_nState == T3D_COMP_SURF_SLOT_RESIDENT &&
		     ( victim == T3D_COMP_SURF_INVALID_SLOT || slot.m_nLastTouch < m_pSlots[victim].m_nLastTouch ) ) {
			victim = i;
		}
	}
	if ( victim == T3D_COMP_SURF_INVALID_SLOT )
		return false;
	t3dComposedSurfaceSlot_t& slot = m_pSlots[victim];
	slot.m_nState = T3D_COMP_SURF_SLOT_RETIRING; // Invalidate mapping immediately.
	rRetirement = { slot.m_nToken, slot.m_nPageIndex, victim, m_nDeviceGeneration };
	++m_stats.m_nRetirements;
	return true;
}

bool Terrain3DComposedSurfaceCache::beginInvalidationHot( const uint32_t nPageIndex, t3dComposedSurfaceJob_t& rRetirement )
{
	rRetirement = {};
	if ( !m_pSlots || nPageIndex >= m_view.m_header.m_nPageCount )
		return false;
	const uint32_t slotIndex = locate( nPageIndex );
	if ( slotIndex == T3D_COMP_SURF_INVALID_SLOT || m_pSlots[slotIndex].m_nState != T3D_COMP_SURF_SLOT_RESIDENT )
		return false;
	t3dComposedSurfaceSlot_t& slot = m_pSlots[slotIndex];
	slot.m_nState = T3D_COMP_SURF_SLOT_RETIRING;
	rRetirement = { slot.m_nToken, slot.m_nPageIndex, slotIndex, m_nDeviceGeneration };
	++m_stats.m_nRetirements;
	return true;
}

bool Terrain3DComposedSurfaceCache::retirementPendingHot( const t3dComposedSurfaceJob_t& retirement ) const
{
	return owns( retirement, T3D_COMP_SURF_SLOT_RETIRING );
}

bool Terrain3DComposedSurfaceCache::cancelRetirementHot( const t3dComposedSurfaceJob_t& retirement )
{
	if ( !retirementPendingHot( retirement ) )
		return false;
	m_pSlots[retirement.m_nSlot].m_nState = T3D_COMP_SURF_SLOT_RESIDENT;
	return true;
}

bool Terrain3DComposedSurfaceCache::confirmEvictionHot( const t3dComposedSurfaceJob_t& retirement )
{
	if ( !owns( retirement, T3D_COMP_SURF_SLOT_RETIRING ) )
		return false;
	m_pSlots[retirement.m_nSlot] = {};
	return true;
}

bool Terrain3DComposedSurfaceCache::claimNextHot( const bool bTransferBudgetGranted, t3dComposedSurfaceJob_t& rJob )
{
	rJob = {};
	if ( !bTransferBudgetGranted || !m_pSlots || !m_stats.m_nPending || !m_nDeviceGeneration || m_nNextToken == UINT64_MAX )
		return false;
	uint32_t slot_id = T3D_COMP_SURF_INVALID_SLOT;
	for ( uint32_t i = 0u; i < m_nCapacity; ++i ) {
		if ( m_pSlots[i].m_nState == T3D_COMP_SURF_SLOT_FREE ) {
			slot_id = i;
			break;
		}
	}
	if ( slot_id == T3D_COMP_SURF_INVALID_SLOT )
		return false;
	uint32_t best = 0u;
	for ( uint32_t i = 1u; i < m_stats.m_nPending; ++i ) {
		if ( m_pRequests[i].m_nPriority > m_pRequests[best].m_nPriority )
			best = i;
	}
	const uint32_t page = m_pRequests[best].m_nPageIndex;
	for ( uint32_t i = best + 1u; i < m_stats.m_nPending; ++i )
		m_pRequests[i - 1u] = m_pRequests[i];
	--m_stats.m_nPending;
	t3dComposedSurfaceSlot_t& slot = m_pSlots[slot_id];
	slot = {};
	slot.m_nPageIndex = page;
	slot.m_nDeviceGeneration = m_nDeviceGeneration;
	slot.m_nToken = m_nNextToken++;
	slot.m_nState = T3D_COMP_SURF_SLOT_READING;
	rJob = { slot.m_nToken, page, slot_id, m_nDeviceGeneration };
	return true;
}

bool Terrain3DComposedSurfaceCache::readJobCold( const t3dComposedSurfaceJob_t& job, uint8_t *pOutput, const uint32_t nOutputCapacity )
{
	if ( !owns( job, T3D_COMP_SURF_SLOT_READING ) || !pOutput || nOutputCapacity < T3D_COMP_SURF_PAGE_PAYLOAD_BYTES )
		return false;
	t3dComposedSurfacePageRecord_t page{};
	if ( !t3d_comp_surf_sidecar_record( m_view, job.m_nPageIndex, page ) )
		return false;
	uint32_t dest = 0u;
	// Validate all channels before making anything uploadable.
	for ( uint32_t i = 0u; i < T3D_COMP_SURF_CHANNEL_COUNT; ++i ) {
		const t3dComposedSurfaceChannelRecord_t& channel = page.m_channels[i];
		if ( !t3d_comp_surf_range_within( channel.m_nFileOffset, channel.m_nByteCount, m_view.m_nFileBytes ) ||
		     channel.m_nByteCount != t3d_comp_surf_channel_bytes( i ) ||
		     !t3d_comp_surf_validate_channel_payload( channel, m_view.m_pFile + channel.m_nFileOffset, channel.m_nByteCount ) ) {
			++m_stats.m_nCorruptPages;
			return false;
		}
	}
	for ( uint32_t i = 0u; i < T3D_COMP_SURF_CHANNEL_COUNT; ++i ) {
		const t3dComposedSurfaceChannelRecord_t& channel = page.m_channels[i];
		memcpy( pOutput + dest, m_view.m_pFile + channel.m_nFileOffset, channel.m_nByteCount );
		dest += channel.m_nByteCount;
	}
	m_pSlots[job.m_nSlot].m_nState = T3D_COMP_SURF_SLOT_READY;
	return true;
}

bool Terrain3DComposedSurfaceCache::readyForUploadHot( const t3dComposedSurfaceJob_t& job, t3dComposedSurfacePageRecord_t& rRecord ) const
{
	rRecord = {};
	return owns( job, T3D_COMP_SURF_SLOT_READY ) && t3d_comp_surf_sidecar_record( m_view, job.m_nPageIndex, rRecord );
}

bool Terrain3DComposedSurfaceCache::uploadingRecordHot( const t3dComposedSurfaceJob_t& job, t3dComposedSurfacePageRecord_t& rRecord ) const
{
	rRecord = {};
	return owns( job, T3D_COMP_SURF_SLOT_UPLOADING ) && t3d_comp_surf_sidecar_record( m_view, job.m_nPageIndex, rRecord );
}

bool Terrain3DComposedSurfaceCache::markUploadSubmittedHot( const t3dComposedSurfaceJob_t& job )
{
	if ( !owns( job, T3D_COMP_SURF_SLOT_READY ) )
		return false;
	m_pSlots[job.m_nSlot].m_nState = T3D_COMP_SURF_SLOT_UPLOADING;
	return true;
}

bool Terrain3DComposedSurfaceCache::completeUploadHot( const t3dComposedSurfaceJob_t& job, const uint32_t nUploadedChannelMask,
                                                       const bool bSubmissionConfirmed )
{
	if ( !owns( job, T3D_COMP_SURF_SLOT_UPLOADING ) ) {
		++m_stats.m_nStaleCompletions;
		return false;
	}
	if ( !bSubmissionConfirmed || nUploadedChannelMask != T3D_COMP_SURF_CHANNEL_MASK_ALL )
		return false;
	m_pSlots[job.m_nSlot].m_nState = T3D_COMP_SURF_SLOT_RESIDENT;
	return true;
}

bool Terrain3DComposedSurfaceCache::cancelJobHot( const t3dComposedSurfaceJob_t& job )
{
	if ( !owns( job, T3D_COMP_SURF_SLOT_READING ) && !owns( job, T3D_COMP_SURF_SLOT_READY ) && !owns( job, T3D_COMP_SURF_SLOT_UPLOADING ) )
		return false;
	m_pSlots[job.m_nSlot] = {};
	return true;
}

uint32_t Terrain3DComposedSurfaceCache::stateForPageHot( const uint32_t nPageIndex ) const
{
	if ( !m_pSlots || nPageIndex >= m_view.m_header.m_nPageCount )
		return T3D_COMP_SURF_SLOT_FREE;
	const uint32_t slot = locate( nPageIndex );
	return slot == T3D_COMP_SURF_INVALID_SLOT ? T3D_COMP_SURF_SLOT_FREE : m_pSlots[slot].m_nState;
}

bool Terrain3DComposedSurfaceCache::cancelUnsubmittedPageHot( const uint32_t nPageIndex )
{
	if ( !m_pSlots || nPageIndex >= m_view.m_header.m_nPageCount )
		return false;
	const uint32_t slot = locate( nPageIndex );
	if ( slot == T3D_COMP_SURF_INVALID_SLOT ||
	     ( m_pSlots[slot].m_nState != T3D_COMP_SURF_SLOT_READING && m_pSlots[slot].m_nState != T3D_COMP_SURF_SLOT_READY ) )
		return false;
	m_pSlots[slot] = {};
	return true;
}

bool Terrain3DComposedSurfaceCache::forgetPendingDemandHot( const uint32_t nPageIndex )
{
	if ( !m_pRequests )
		return false;
	for ( uint32_t i = 0u; i < m_stats.m_nPending; ++i ) {
		if ( m_pRequests[i].m_nPageIndex != nPageIndex )
			continue;
		for ( uint32_t j = i + 1u; j < m_stats.m_nPending; ++j ) {
			m_pRequests[j - 1u] = m_pRequests[j];
		}
		--m_stats.m_nPending;
		return true;
	}
	return false;
}

bool Terrain3DComposedSurfaceCache::residentMappingHot( const uint32_t nPageIndex, t3dComposedSurfacePageMapping_t& rMapping ) const
{
	rMapping = {};
	if ( !m_pSlots )
		return false;
	const uint32_t i = locate( nPageIndex );
	if ( i == T3D_COMP_SURF_INVALID_SLOT || m_pSlots[i].m_nState != T3D_COMP_SURF_SLOT_RESIDENT )
		return false;
	t3dComposedSurfacePageRecord_t page{};
	if ( !t3d_comp_surf_sidecar_record( m_view, nPageIndex, page ) )
		return false;
	rMapping.m_nRegionKey = page.m_nRegionKey;
	rMapping.m_nCompositionRevision = page.m_nCompositionRevision;
	rMapping.m_nPageX = page.m_nPageX;
	rMapping.m_nPageY = page.m_nPageY;
	rMapping.m_nMip = page.m_nMip;
	rMapping.m_nGeneration = page.m_nGeneration;
	rMapping.m_nDeviceGeneration = m_nDeviceGeneration;
	rMapping.m_nPhysicalSlot = i;
	rMapping.m_nResidentChannels = T3D_COMP_SURF_CHANNEL_MASK_ALL;
	rMapping.m_nState = T3D_COMP_SURF_PAGE_RESIDENT;
	return t3d_comp_surf_validate_mapping( rMapping, m_nDeviceGeneration, page.m_nCompositionRevision );
}
