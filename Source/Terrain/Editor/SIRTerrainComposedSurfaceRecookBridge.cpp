/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Terrain/Editor/SIRTerrainComposedSurfaceRecookBridge.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/authoring/vt/integration/terrain_3d_composed_surface_recook_bridge.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/data/runtime/terrain_3d_composed_surface_sidecar.h>
#include <limits.h>

bool SIRTerrainComposedSurfaceRecookBridge::loadPreviousSidecar( const sirTerrainComposedSidecarLoadRequest_t& request,
                                                                 uint64_t& nLoaded ) noexcept
{
	nLoaded = 0u;
	if ( request.pszPath == nullptr || request.pszPath[0] == '\0' || request.pStorage == nullptr || request.nCapacity == 0u ||
	     request.nCapacity > SIZE_MAX ) {
		return false;
	}
	SIRPlatformFile file;
	if ( file.open( request.pszPath, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t nBytes = 0u;
	if ( file.getSize( &nBytes ) != SIR_FILE_OK || nBytes == 0u || nBytes > request.nCapacity ) {
		return false;
	}
	uint64_t nRead = 0u;
	const bool bRead = file.readAt( 0u, request.pStorage, nBytes, &nRead ) == SIR_FILE_OK && nRead == nBytes;
	const bool bClosed = file.close() == SIR_FILE_OK;
	if ( !bRead || !bClosed || !t3d_comp_surf_validate_sidecar_bytes( request.pStorage, ( size_t )nBytes, true ) ) {
		return false;
	}
	nLoaded = nBytes;
	return true;
}

void SIRTerrainComposedSurfaceRecookBridge::reset( void ) noexcept
{
	m_journal.resetCold();
}

bool SIRTerrainComposedSurfaceRecookBridge::dirtyRegion( const sirTerrainComposedDirtyRequest_t& request ) noexcept
{
	return m_journal.dirtyRegionCold( request.region, request.bounds, request.nTexelsPerMeter, request.nSourceFingerprint,
	                                  request.nTopologyGeneration );
}

bool SIRTerrainComposedSurfaceRecookBridge::invalidateRegion( const t3dComposedSurfaceEditRegion_t& region, uint64_t nFingerprint,
                                                              uint64_t nTopologyGeneration ) noexcept
{
	return m_journal.invalidateRegionCold( region, nFingerprint, nTopologyGeneration );
}

bool SIRTerrainComposedSurfaceRecookBridge::forgetRegion( uint32_t nRegionIndex, uint64_t nTopologyGeneration ) noexcept
{
	return m_journal.forgetRegionCold( nRegionIndex, nTopologyGeneration );
}

bool SIRTerrainComposedSurfaceRecookBridge::claim( uint32_t nRegionIndex, t3dComposedSurfaceEditTicket_t& ticket ) noexcept
{
	return m_journal.claimCold( nRegionIndex, ticket );
}

void SIRTerrainComposedSurfaceRecookBridge::cancel( const t3dComposedSurfaceEditTicket_t& ticket ) noexcept
{
	m_journal.cancelCold( ticket );
}

const t3dComposedSurfaceEditStats_t& SIRTerrainComposedSurfaceRecookBridge::stats( void ) const noexcept
{
	return m_journal.statsCold();
}

const t3dComposedSurfaceEditRecord_t *SIRTerrainComposedSurfaceRecookBridge::region( uint32_t nRegionIndex ) const noexcept
{
	return m_journal.regionCold( nRegionIndex );
}

sirTerrainComposedRecookResult_t
SIRTerrainComposedSurfaceRecookBridge::recookAndPublish( const sirTerrainComposedRecookRequest_t& request ) noexcept
{
	const t3dComposedSurfaceEditTicket_t *const pTicket = request.pTicket;
	const sirTerrainComposedRecookBuffers_t& buffers = request.buffers;
	const sirTerrainComposedRecookPageList_t& pages = request.pages;
	if ( pTicket == nullptr || request.pszDestination == nullptr || request.pszDestination[0] == '\0' || pages.pPages == nullptr ||
	     pages.nPageCount == 0u || pages.nRegionKey == 0u || buffers.pDirtyFlags == nullptr || buffers.pScratch == nullptr ||
	     buffers.pOutput == nullptr || buffers.nScratchBytes == 0u || buffers.nOutputCapacity == 0u ) {
		return SIR_TERRAIN_COMPOSED_RECOOK_INVALID_INPUT;
	}
	if ( !m_journal.completionCurrentCold( *pTicket, request.source.m_nSourceFingerprint, pTicket->m_nTopologyGeneration ) ) {
		return SIR_TERRAIN_COMPOSED_RECOOK_STALE;
	}
	uint64_t nWritten = 0u;
	const bool bCooked = t3d_comp_surf_recook_journal_ticket_cold(
	    m_journal, *pTicket, pages.nRegionKey, pages.nMip0Width, pages.nMip0Height, pages.pPages, pages.nPageCount, request.source,
	    buffers.pPreviousFile, buffers.nPreviousBytes, buffers.pDirtyFlags, buffers.nDirtyCapacity, buffers.pScratch, buffers.nScratchBytes,
	    buffers.pOutput, buffers.nOutputCapacity, nWritten );
	if ( !bCooked ) {
		return m_journal.completionCurrentCold( *pTicket, request.source.m_nSourceFingerprint, pTicket->m_nTopologyGeneration )
		           ? SIR_TERRAIN_COMPOSED_RECOOK_FAILED
				   : SIR_TERRAIN_COMPOSED_RECOOK_STALE;
	}
	if ( !m_journal.completionCurrentCold( *pTicket, request.source.m_nSourceFingerprint, pTicket->m_nTopologyGeneration ) ) {
		return SIR_TERRAIN_COMPOSED_RECOOK_STALE;
	}
	if ( !t3d_comp_surf_write_sidecar_cold( request.pszDestination, buffers.pOutput, nWritten ) ) {
		return SIR_TERRAIN_COMPOSED_RECOOK_PUBLISH_FAILED;
	}
	// The caller holds the editor/terrain publication lock here. A successful
	// atomic disk replace precedes consumption of the original dirty ticket.
	if ( !m_journal.acceptCold( *pTicket, request.source.m_nSourceFingerprint, pTicket->m_nTopologyGeneration ) ) {
		return SIR_TERRAIN_COMPOSED_RECOOK_STALE;
	}
	return SIR_TERRAIN_COMPOSED_RECOOK_SUCCESS;
}

bool SIRTerrainComposedSurfaceRecookBridge::cookEditorAndPublish( const t3dComposedSurfaceEditorCookRequest_t& request,
                                                                  t3dComposedSurfaceEditorCookResult_t& result ) noexcept
{
	return t3d_composed_surface_editor_cook_cold( m_journal, request, result );
}
