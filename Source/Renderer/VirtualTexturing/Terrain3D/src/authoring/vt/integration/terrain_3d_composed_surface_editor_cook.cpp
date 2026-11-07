#include "authoring/vt/integration/terrain_3d_composed_surface_editor_cook.h"
#include "authoring/vt/integration/terrain_3d_composed_surface_recook_bridge.h"
#include "authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.h"

struct t3dComposedSurfaceEditorCookCurrentContext_t
{
	const Terrain3DComposedSurfaceEditJournal *m_pJournal;
	const t3dComposedSurfaceEditTicket_t *m_pTicket;
	t3dComposedSurfaceColdCookCurrentFn m_pPreviousCurrent;
	void *m_pPreviousUser;
};

static bool t3d_composed_surface_editor_current( void *pUser, uint64_t nTerrain, uint64_t nMaterial, uint64_t nTopology )
{
	const t3dComposedSurfaceEditorCookCurrentContext_t& ctx = *static_cast<const t3dComposedSurfaceEditorCookCurrentContext_t *>( pUser );
	return ctx.m_pJournal->completionCurrentCold( *ctx.m_pTicket, ctx.m_pTicket->m_nSourceFingerprint,
	                                              ctx.m_pTicket->m_nTopologyGeneration ) &&
	       ctx.m_pPreviousCurrent( ctx.m_pPreviousUser, nTerrain, nMaterial, nTopology );
}

bool t3d_composed_surface_editor_cook_cold( Terrain3DComposedSurfaceEditJournal& journal,
                                            const t3dComposedSurfaceEditorCookRequest_t& request,
                                            t3dComposedSurfaceEditorCookResult_t& result )
{
	result = {};
	result.m_status = T3D_COMPOSED_SURFACE_EDITOR_COOK_INVALID_INPUT;
	const t3dComposedSurfaceColdCookRequest_t& input = request.m_cook;
	if ( !request.m_nRegionKey || !request.m_nJournalFingerprint || !request.m_nJournalTopologyGeneration ||
	     request.m_nRegionIndex >= T3D_COMP_SURF_EDITOR_REGION_COUNT || !request.m_pDirtyFlags ||
	     request.m_nDirtyCapacity < input.m_nPageCount || !request.m_pBeginCommit || !request.m_pEndCommit || !input.m_pCurrent ||
	     !input.m_pDestination || !input.m_pDestination[0] ||
	     !( input.m_source.m_bCanonicalMaterialParityAccepted || input.m_bReferenceOnly ) )
		return false;

	t3dComposedSurfaceEditTicket_t ticket{};
	if ( !journal.claimCold( request.m_nRegionIndex, ticket ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_EDITOR_COOK_NOT_DIRTY;
		return false;
	}
	result.m_nClaimRevision = ticket.m_nRevision;
	if ( ticket.m_nSourceFingerprint != request.m_nJournalFingerprint ||
	     ticket.m_nTopologyGeneration != request.m_nJournalTopologyGeneration ||
	     !journal.completionCurrentCold( ticket, request.m_nJournalFingerprint, request.m_nJournalTopologyGeneration ) ||
	     !t3d_comp_surf_make_recook_flags_cold( ticket, request.m_nRegionKey, request.m_nMip0Width, request.m_nMip0Height, input.m_pPages,
	                                            input.m_nPageCount, request.m_pDirtyFlags, request.m_nDirtyCapacity ) ) {
		journal.cancelCold( ticket );
		result.m_status = T3D_COMPOSED_SURFACE_EDITOR_COOK_STALE;
		return false;
	}

	t3dComposedSurfaceEditorCookCurrentContext_t context{ &journal, &ticket, input.m_pCurrent, input.m_pCurrentUser };
	t3dComposedSurfaceColdCookRequest_t cook = input;
	cook.m_pCurrent = t3d_composed_surface_editor_current;
	cook.m_pCurrentUser = &context;
	cook.m_pDirtyFlags = request.m_pDirtyFlags;
	// CS13 is memory-only here. Its own pre/post generation checks run while
	// composing; publication is deferred until the editor owner's commit lock.
	cook.m_pDestination = nullptr;
	if ( !t3d_composed_surface_cook_source_pack_cold( cook, result.m_cook ) ) {
		journal.cancelCold( ticket );
		result.m_status = result.m_cook.m_status == T3D_COMPOSED_SURFACE_COLD_COOK_STALE_REVISION
		                      ? T3D_COMPOSED_SURFACE_EDITOR_COOK_STALE
		                      : T3D_COMPOSED_SURFACE_EDITOR_COOK_COOK_FAILED;
		return false;
	}

	if ( !request.m_pBeginCommit( request.m_pCommitUser ) ) {
		journal.cancelCold( ticket );
		result.m_status = T3D_COMPOSED_SURFACE_EDITOR_COOK_COMMIT_FAILED;
		return false;
	}
	const bool current =
	    t3d_composed_surface_editor_current( &context, input.m_nTerrainRevision, input.m_nMaterialRevision, input.m_nTopologyGeneration );
	const bool written =
	    current && t3d_comp_surf_write_sidecar_cold( input.m_pDestination, input.m_pOutput, result.m_cook.m_nWrittenBytes );
	const bool accepted = written && journal.acceptCold( ticket, request.m_nJournalFingerprint, request.m_nJournalTopologyGeneration );
	request.m_pEndCommit( request.m_pCommitUser );
	if ( !accepted ) {
		journal.cancelCold( ticket );
		result.m_status = current ? T3D_COMPOSED_SURFACE_EDITOR_COOK_COMMIT_FAILED : T3D_COMPOSED_SURFACE_EDITOR_COOK_STALE;
		return false;
	}
	result.m_status = T3D_COMPOSED_SURFACE_EDITOR_COOK_SUCCESS;
	return true;
}
