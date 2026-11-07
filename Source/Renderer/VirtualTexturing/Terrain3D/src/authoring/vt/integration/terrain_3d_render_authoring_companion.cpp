// Editor-owned VT baking and preview composition, absent from runtime builds.
#if defined( T3D_EDITOR_NATIVE )
#include "rendering/rd/core/terrain_3d_render_coordinator.h"
#include "authoring/vt/integration/terrain_3d_render_authoring_companion.h"
#include "authoring/vt/integration/terrain_3d_composed_surface_coordinator_hooks.h"
#include "authoring/vt/integration/terrain_3d_composed_surface_editor_source.h"
#include "rendering/rd/vt/core/terrain_3d_composed_surface_acceptance.h"
#include "authoring/vt/build/terrain_3d_vt_demand_cooker.h"
#include "assets/catalog/terrain_3d_assets.h"
#include "data/model/terrain_3d_data.h"
#include "terrain_3d.h"
#include "memory/policy/terrain_3d_memory_account.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>

bool Terrain3DRenderCoordinator::bakePreviewVirtualTexturesCold( String *const pError )
{
	Terrain3DMemoryAccountScope memory_scope( m_pTerrain ? m_pTerrain->getMemoryAccountInternal() : nullptr );
	Terrain3DMemoryAccountScope authoring_scope( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() );
	m_pAuthoring->m_cooker.retryDirtyRegionsCold();
	return m_previewService.bakePreviewVirtualTexturesCold( previewServices(), pError );
}

bool Terrain3DRenderCoordinator::preparePreviewAuthoringPackageCold( const Ref<Terrain3DAssets>& rAssets, String *const pError )
{
	Terrain3DMemoryAccountScope memory_scope( m_pTerrain ? m_pTerrain->getMemoryAccountInternal() : nullptr );
	Terrain3DMemoryAccountScope authoring_scope( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() );
	return m_previewService.preparePreviewAuthoringPackageCold( previewServices(), rAssets, pError );
}

Dictionary Terrain3DRenderCoordinator::forceRebakeVirtualTexturesCold( void )
{
	if ( m_pTerrain == NULL || !m_pTerrain->isEditorAuthoringMode() || Engine::get_singleton() == NULL ||
	     !Engine::get_singleton()->is_editor_hint() ) {
		Dictionary result;
		result["state"] = "failed";
		result["error"] = "Forced VT rebuild is editor-only";
		return result;
	}
	if ( m_pTerrain->dataObject() == NULL || m_pTerrain->getDataDirectory().is_empty() ) {
		Dictionary result;
		result["state"] = "failed";
		result["error"] = "Set a terrain data directory before forcing and saving VT rebuilds";
		return result;
	}
	if ( m_pAuthoring->m_bForceRebakeActive || m_pAuthoring->m_bForceRebakeQueued ) {
		return getForceRebakeVirtualTexturesStatusCold();
	}
	// Explicit all-region rebake must supersede any composed sidecar revision.
	if ( m_terrainMegaVT.isReady() )
		t3d_comp_surf_editor_invalidate_sources( m_terrainMegaVT, m_composedSurfaceEdits, m_nComposedSourceGeneration );
	m_pAuthoring->m_composedCandidateSourcePack.clear();
	m_pAuthoring->m_bComposedCandidatePending = false;
	m_pAuthoring->m_composedCandidateState = "queued_after_source_bake";
	m_pAuthoring->m_composedCandidate = {};
	m_pAuthoring->m_bForceRebakeAll = true;
	++m_pAuthoring->m_nForceRebakeRequestSerial;
	m_pAuthoring->m_bForceRebakeQueued = true; // Repeated clicks observe the active request.
	m_previewService.setForceOwnerPendingCold( true );
	if ( !m_pAuthoring->m_bForceRebakeActive ) {
		m_pAuthoring->m_forceRebakeState = "queued";
		m_pAuthoring->m_forceRebakeError = String();
	}
	return getForceRebakeVirtualTexturesStatusCold();
}

Dictionary Terrain3DRenderCoordinator::bakeChangedVirtualTexturesCold( void )
{
	if ( m_pTerrain == NULL || !m_pTerrain->isEditorAuthoringMode() || Engine::get_singleton() == NULL ||
	     !Engine::get_singleton()->is_editor_hint() || m_pTerrain->dataObject() == NULL || m_pTerrain->getDataDirectory().is_empty() ) {
		Dictionary result;
		result["state"] = "failed";
		result["error"] = "Bake Changed requires an editor terrain with a data directory";
		return result;
	}
	if ( m_pAuthoring->m_bForceRebakeActive || m_pAuthoring->m_bForceRebakeQueued ) {
		return getForceRebakeVirtualTexturesStatusCold();
	}
	m_pAuthoring->m_composedCandidateSourcePack.clear();
	m_pAuthoring->m_bComposedCandidatePending = false;
	m_pAuthoring->m_composedCandidateState = "queued_after_source_bake";
	m_pAuthoring->m_composedCandidate = {};
	m_pAuthoring->m_bForceRebakeAll = false;
	++m_pAuthoring->m_nForceRebakeRequestSerial;
	m_pAuthoring->m_bForceRebakeQueued = true;
	m_previewService.setForceOwnerPendingCold( true );
	m_pAuthoring->m_forceRebakeState = "queued";
	m_pAuthoring->m_forceRebakeError = String();
	return getForceRebakeVirtualTexturesStatusCold();
}

Dictionary Terrain3DRenderCoordinator::getForceRebakeVirtualTexturesStatusCold( void ) const
{
	Dictionary result = m_previewService.getForceRebakeStatusCold();
	result["state"] = m_pAuthoring->m_forceRebakeState;
	result["complete"] = m_pAuthoring->m_forceRebakeState == "saved" && !m_pAuthoring->m_bForceRebakeQueued;
	result["request_serial"] = m_pAuthoring->m_nForceRebakeRequestSerial;
	result["completion_serial"] = m_pAuthoring->m_nForceRebakeCompletionSerial;
	result["forced_megavt_regions"] = m_pAuthoring->m_nForceRegionCount;
	result["dirty_regions"] = m_terrainMegaVT.getDirtyRegionCount();
	const bool approvedComposed = !m_pAuthoring->m_approvedComposedV1Fragment.is_empty();
	result["composed_v1_shared_package_ready"] = approvedComposed;
	result["composed_v1_shared_package_bytes"] = ( int64_t )m_pAuthoring->m_approvedComposedV1Fragment.size();
	result["composed_v1_import_status"] = m_pAuthoring->m_approvedComposedStatus;
	result["composed_cache_enabled"] = approvedComposed;
	result["composed_parity_supported"] = approvedComposed;
	result["composed_residency"] = approvedComposed ? "v1_shared_package_queued" : "not_initialized";
	result["composed_gpu_resident_pages"] = 0;
	result["composed_live_fallback_reason"] = "canonical_color_dnr_alpha_parity_unverified";
	result["composed_backend"] = approvedComposed ? "v1_shared_residency" : "live_until_parity_accepted";
	result["composed_bake"] = m_pAuthoring->m_composedCandidateState;
	result["composed_bake_reference_only"] = true;
	result["composed_bake_reference_ready"] = m_pAuthoring->m_composedCandidate.m_bSaved;
	result["composed_bake_path"] = m_pAuthoring->m_composedCandidate.m_path;
	result["composed_bake_error"] = m_pAuthoring->m_composedCandidate.m_error;
	result["composed_bake_pages"] = ( int64_t )m_pAuthoring->m_composedCandidate.m_nPages;
	result["composed_bake_bytes"] = ( int64_t )m_pAuthoring->m_composedCandidate.m_nWrittenBytes;
	result["composed_bake_reused_pages"] = ( int64_t )m_pAuthoring->m_composedCandidate.m_nReusedPages;
	result["composed_bake_boundary_clamped_pages"] = ( int64_t )m_pAuthoring->m_composedCandidate.m_nBoundaryClampedPages;
	result["composed_bake_fingerprint"] = ( int64_t )m_pAuthoring->m_composedCandidate.m_nSourceFingerprint;
	result["composed_bake_pending"] = m_pAuthoring->m_bComposedCandidatePending;
	result["composed_bake_request_serial"] = ( int64_t )m_pAuthoring->m_nComposedCandidateRequestSerial;
	result["composed_bake_completion_serial"] = ( int64_t )m_pAuthoring->m_nComposedCandidateCompletionSerial;
	result["composed_dirty_regions"] = m_composedSurfaceEdits.statsCold().m_nDirtyRegions;
	result["composed_dirty_edit_events"] = ( int64_t )m_composedSurfaceEdits.statsCold().m_nEditEvents;
	result["composed_source_invalidations"] = ( int64_t )m_composedSurfaceEdits.statsCold().m_nSourceInvalidations;
	result["composed_stale_discarded"] = ( int64_t )m_composedSurfaceEdits.statsCold().m_nStaleCompletions;
	result["composed_accepted_recooks"] = ( int64_t )m_composedSurfaceEdits.statsCold().m_nAcceptedCompletions;
	result["composed_bake_parity_ready"] = false;
	// Replacements remain forbidden until all independent acceptance gates pass.
	result["composed_legacy_middle_atlas_retirable"] =
	    t3d_comp_surf_may_retire_legacy_middle_atlas( t3dComposedSurfaceRetirementEvidence_t{} );
	result["mode"] = m_pAuthoring->m_bForceRebakeAll ? "rebuild_all" : "changed";
	result["cooked_megavt_regions"] = m_pAuthoring->m_cooker.getForceCookedCountCold();
	// Expose the actual work rather than only the manual request serial.
	const Dictionary status = m_previewService.getForceRebakeStatusCold();
	const Dictionary cooker = m_pAuthoring->m_cooker.getDiagnosticsCold();
	result["worker_active"] = cooker.get( "worker_active", false );
	result["worker_submitted"] = cooker.get( "submitted", 0 );
	result["worker_failures"] = cooker.get( "failures", 0 );
	result["worker_retries"] = cooker.get( "transient_retry_regions", 0 );
	result["worker_last_error"] = cooker.get( "last_error", String() );
	result["megavt_ready"] = m_terrainMegaVT.isReady();
	result["gpu_publication_pending"] = status.get( "pending", false );
	result["gpu_publication_error"] = status.get( "error", String() );
	result["preview_readback_pending"] = m_previewService.hasReadbackInFlight();
	String phase = "idle";
	if ( m_pAuthoring->m_bForceRebakeQueued && !m_terrainMegaVT.isReady() ) {
		phase = "waiting_for_terrain_topology";
	} else if ( m_pAuthoring->m_bForceRebakeQueued && ( bool )status.get( "pending", false ) ) {
		phase = "waiting_for_previous_vt_publication";
	} else if ( m_pAuthoring->m_bForceRebakeQueued ) {
		phase = "queued_for_region_cook";
	} else if ( m_pAuthoring->m_bForceRebakeActive && !m_terrainMegaVT.isReady() ) {
		phase = "waiting_for_runtime_vt";
	} else if ( m_pAuthoring->m_bForceRebakeActive && !m_pAuthoring->m_bForceMaterialsStaged ) {
		phase = "cooking_regions";
	} else if ( m_pAuthoring->m_bForceRebakeActive && ( bool )status.get( "pending", false ) ) {
		phase = "publishing_material_vt";
	} else if ( m_pAuthoring->m_bForceRebakeActive &&
	            ( uint64_t )status.get( "published_serial", 0u ) < m_pAuthoring->m_nForceRebakeActiveSerial ) {
		phase = "waiting_for_material_vt_publication";
	} else if ( m_pAuthoring->m_bForceRebakeActive ) {
		phase = "finalizing_runtime_package";
	} else if ( m_pAuthoring->m_forceRebakeState == "saved" ) {
		phase = "completed";
	} else if ( m_pAuthoring->m_forceRebakeState == "failed" ) {
		phase = "failed";
	}
	result["phase"] = phase;
	result["shipping_package_generation"] = m_pAuthoring->m_nForceShippingGeneration;
	result["output_path"] = m_pAuthoring->m_forceRebakeOutput;
	// Queued/working requests must not repeatedly display a cached failure from
	// an earlier automatic publication. The force owner records its own error.
	result["error"] = m_pAuthoring->m_forceRebakeError;
	return result;
}

void Terrain3DRenderCoordinator::serviceForceRebakeCold( void )
{
	Terrain3DData *const data = m_pTerrain != NULL ? m_pTerrain->dataObject() : NULL;
	if ( data == NULL ) {
		return;
	}
	const t3dMegaVTAuthoringInputs_t inputs{ *data, m_pTerrain->getDataDirectory(),
	                                         t3dMemoryCookingLimit( m_pTerrain->getMemoryAccountInternal() ),
	                                         m_terrainMegaVT.getTexelsPerMeter() };
	Dictionary status = m_previewService.getForceRebakeStatusCold();
	if ( !m_pAuthoring->m_bForceRebakeActive && m_pAuthoring->m_bForceRebakeQueued ) {
		if ( !m_terrainMegaVT.isReady() || ( bool )status.get( "pending", false ) ) {
			return;
		}
		m_pAuthoring->m_bForceRebakeQueued = false;
		m_pAuthoring->m_bForceRebakeActive = true;
		m_pAuthoring->m_bForceMaterialsStaged = false;
		m_pAuthoring->m_nForceRebakeActiveSerial = m_pAuthoring->m_nForceRebakeRequestSerial;
		m_pAuthoring->m_forceRebakeState = "working";
		m_pAuthoring->m_forceRebakeError = String();
		m_pAuthoring->m_forceRebakeOutput = String();
		if ( m_pAuthoring->m_bForceRebakeAll ) {
			m_pAuthoring->m_cooker.requestForceRecookAllRegionsCold();
		} else {
			m_pAuthoring->m_cooker.requestRecookChangedRegionsCold();
		}
		return;
	}
	if ( !m_pAuthoring->m_bForceRebakeActive ) {
		return;
	}
	m_pAuthoring->m_nForceRegionCount = m_pAuthoring->m_cooker.getForceRegionCountCold();
	bool failed = m_pAuthoring->m_cooker.isForceRecookFailedCold();
	if ( failed ) {
		m_pAuthoring->m_forceRebakeError = m_pAuthoring->m_cooker.getLastErrorCold();
	}
	if ( !failed && !m_pAuthoring->m_bForceMaterialsStaged && m_pAuthoring->m_cooker.isForceRecookReadyCold() ) {
		m_pAuthoring->m_nForceRegionCount = m_pAuthoring->m_cooker.getForceRegionCountCold();
		if ( !m_pAuthoring->m_cooker.validateForceRecookSnapshotCold( m_terrainMegaVT ) ) {
			m_pAuthoring->m_forceRebakeError = "Terrain sources changed during forced region cook";
			failed = true;
		} else {
			m_pAuthoring->m_bForceMaterialsStaged = true;
			{
				Terrain3DMemoryAccountScope authoring_scope( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() );
				if ( !m_previewService.bakePreviewVirtualTexturesCold( previewServices(), &m_pAuthoring->m_forceRebakeError,
				                                                       m_pAuthoring->m_bForceRebakeAll,
				                                                       m_pAuthoring->m_nForceRebakeActiveSerial ) ) {
					failed = true;
				}
			}
			status = m_previewService.getForceRebakeStatusCold();
		}
	}
	if ( m_pAuthoring->m_bForceMaterialsStaged && !failed ) {
		if ( !m_pAuthoring->m_cooker.validateForceRecookSnapshotCold( m_terrainMegaVT ) ) {
			m_pAuthoring->m_forceRebakeError = "Terrain sources changed before forced VT publication";
			failed = true;
		}
	}
	if ( m_pAuthoring->m_bForceMaterialsStaged && !failed ) {
		const String error = status.get( "error", String() );
		if ( !error.is_empty() && !( bool )status.get( "pending", false ) ) {
			m_pAuthoring->m_forceRebakeError = error;
			failed = true;
		} else if ( ( uint64_t )status.get( "published_serial", 0u ) >= m_pAuthoring->m_nForceRebakeActiveSerial ) {
			m_pAuthoring->m_forceRebakeState = "published";
			uint32_t materials = 0u;
			PackedByteArray shippingPackage;
			Terrain3DMemoryAccountScope authoring_scope( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() );
			if ( !data->publishRuntimeVirtualTexturesCold( m_pAuthoring->m_forceRebakeOutput, m_pAuthoring->m_nForceShippingGeneration,
			                                               materials, m_pAuthoring->m_forceRebakeError, &shippingPackage,
			                                               m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() ) ) {
				failed = true;
			} else {
				PackedByteArray demandBytes;
				String demandPath;
				PackedByteArray v1Bytes;
				String v1Path;
				const Ref<Terrain3DAssets> assets = m_pTerrain->assetsResource();
				if ( assets.is_valid() && !assets->get_path().is_empty() &&
				     !t3d_vt_demand_cook_asset_sidecar_cold( *data, assets, shippingPackage, demandBytes, demandPath,
				                                             m_pAuthoring->m_forceRebakeError, &v1Bytes, &v1Path ) ) {
					failed = true;
				} else if ( !m_pAuthoring->m_cooker.finishForceRecookAllRegionsCold( inputs, m_terrainMegaVT, *this, true,
				                                                                     &m_pAuthoring->m_forceRebakeOutput, &shippingPackage,
				                                                                     &demandPath, &demandBytes, &v1Path, &v1Bytes ) ) {
					m_pAuthoring->m_forceRebakeError = m_pAuthoring->m_cooker.getLastErrorCold();
					failed = true;
				} else {
					// Preserve the exact, published V1 source bytes for the
					// optional cold candidate cook on a subsequent owner tick.
					m_pAuthoring->m_composedCandidateSourcePack = shippingPackage;
					m_pAuthoring->m_bComposedCandidatePending = !shippingPackage.is_empty();
					++m_pAuthoring->m_nComposedCandidateRequestSerial;
					m_pAuthoring->m_composedCandidateState =
					    m_pAuthoring->m_bComposedCandidatePending ? "queued" : "source_package_missing";
					m_pAuthoring->m_forceRebakeState = "saved";
				}
			}
		}
	}
	if ( failed || m_pAuthoring->m_forceRebakeState == "saved" ) {
		if ( failed ) {
			( void )m_pAuthoring->m_cooker.finishForceRecookAllRegionsCold( inputs, m_terrainMegaVT, *this, false );
			m_pAuthoring->m_forceRebakeState = "failed";
		}
		m_pAuthoring->m_nForceRebakeCompletionSerial = m_pAuthoring->m_nForceRebakeActiveSerial;
		m_pAuthoring->m_bForceRebakeActive = false;
		m_previewService.setForceOwnerPendingCold( m_pAuthoring->m_bForceRebakeQueued );
		if ( m_pAuthoring->m_bForceRebakeQueued ) {
			m_pAuthoring->m_forceRebakeState = "queued";
		}
	}
}

// Explicit bake follow-up, not called from brush edit callbacks or RD threads.
// The source VT bake has already completed and remains authoritative.
void Terrain3DRenderCoordinator::serviceComposedReferenceBakeCold( void )
{
	if ( !m_pAuthoring || !m_pAuthoring->m_bComposedCandidatePending || m_pAuthoring->m_bForceRebakeQueued ||
	     m_pAuthoring->m_bForceRebakeActive )
		return;
	m_pAuthoring->m_bComposedCandidatePending = false;
	m_pAuthoring->m_composedCandidateState = "working";
	// Candidate allocations use the editor cooking account, not the fixed
	// streaming/runtime residency budget.
	Terrain3DMemoryAccountScope authoring_scope( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() );
	const Ref<Terrain3DAssets> assets = m_pTerrain->assetsResource();
	const Ref<Terrain3DMaterial> material = m_pTerrain->materialResource();
	Terrain3DData *const data = m_pTerrain->dataObject();
	t3dCompSurfEditorCandidateResult_t candidate{};
	const bool ok = data && t3d_comp_surf_cook_editor_candidate_cold( m_pAuthoring->m_composedCandidateSourcePack, *data, assets, material,
	                                                                  m_terrainMegaVT, m_composedSurfaceEdits,
	                                                                  m_pTerrain->getDataDirectory(), candidate );
	const auto& stats = m_composedSurfaceEdits.statsCold();
	m_pAuthoring->m_nComposedCandidateEditStamp = stats.m_nEditEvents + stats.m_nSourceInvalidations + stats.m_nRemovedRegions;
	m_pAuthoring->m_composedCandidate = candidate;
	m_pAuthoring->m_composedCandidateSourcePack.clear();
	m_pAuthoring->m_composedCandidateState = ok ? "reference_saved" : "reference_unavailable";
	m_pAuthoring->m_nComposedCandidateCompletionSerial = m_pAuthoring->m_nComposedCandidateRequestSerial;
}

bool Terrain3DRenderCoordinator::composePreviewTerrainPackageCold( PackedByteArray& rPackage, uint32_t& rGeneration, uint64_t& rFingerprint,
                                                                   Terrain3DSharedMemoryReservation& rPackageMemory )
{
	if ( m_pAuthoring == NULL )
		return false;
	const auto& stats = m_composedSurfaceEdits.statsCold();
	const uint64_t editStamp = stats.m_nEditEvents + stats.m_nSourceInvalidations + stats.m_nRemovedRegions;
	const bool approved = !m_pAuthoring->m_approvedComposedV1Fragment.is_empty() && m_pAuthoring->m_nApprovedComposedFingerprint != 0u &&
	                      m_pAuthoring->m_nApprovedComposedSourceGeneration == m_nComposedSourceGeneration &&
	                      m_pAuthoring->m_nApprovedComposedTopologyGeneration == m_terrainMegaVT.getTopologyGeneration() &&
	                      m_pAuthoring->m_nApprovedComposedEditStamp == editStamp;
	return m_pAuthoring->m_cooker.composePackageCold( rPackage, rGeneration, rFingerprint, &rPackageMemory,
	                                                  approved ? &m_pAuthoring->m_approvedComposedV1Fragment : NULL );
}
String Terrain3DRenderCoordinator::previewTerrainCompositionErrorCold() const
{
	return m_pAuthoring ? m_pAuthoring->m_cooker.getLastErrorCold() : String( "VT authoring companion is unavailable" );
}

void Terrain3DRenderCoordinator::serviceAuthoringUpdateCold( void )
{
	if ( m_pAuthoring == NULL ) {
		return;
	}
	const auto& csStats = m_composedSurfaceEdits.statsCold();
	const uint64_t editStamp = csStats.m_nEditEvents + csStats.m_nSourceInvalidations + csStats.m_nRemovedRegions;
	// Any authored edits or source topology/material revisions invalidate the
	// previously approved fragment, before another V1 package is composed.
	if ( !m_pAuthoring->m_approvedComposedV1Fragment.is_empty() &&
	     ( editStamp != m_pAuthoring->m_nApprovedComposedEditStamp ||
	       m_pAuthoring->m_nApprovedComposedSourceGeneration != m_nComposedSourceGeneration ||
	       m_pAuthoring->m_nApprovedComposedTopologyGeneration != m_terrainMegaVT.getTopologyGeneration() ) ) {
		m_pAuthoring->m_approvedComposedV1Fragment.clear();
		m_pAuthoring->m_nApprovedComposedFingerprint = 0u;
		m_pAuthoring->m_approvedComposedStatus = "invalidated_by_source_edit";
		requestMegaVTPackageRecomposeCold();
	}
	if ( m_pAuthoring->m_composedCandidate.m_bSaved && editStamp != m_pAuthoring->m_nComposedCandidateEditStamp ) {
		m_pAuthoring->m_composedCandidate.m_bSaved = false;
		m_pAuthoring->m_composedCandidateState = "reference_stale_after_edit";
	}
	// Existing material-source generations are observed on the editor owner.
	// Invalidate only after a source/topology event, not on every frame.
	const Ref<Terrain3DAssets> composedAssets = m_pTerrain->assetsResource();
	if ( composedAssets.is_valid() && m_terrainMegaVT.isReady() ) {
		uint64_t sourceGeneration = composedAssets->getInstanceVtMaterialSourceGeneration();
		const uint64_t terrainGeneration = composedAssets->getTerrainVtContentGeneration();
		sourceGeneration ^= terrainGeneration + 0x9e3779b97f4a7c15ULL + ( sourceGeneration << 6u ) + ( sourceGeneration >> 2u );
		if ( !sourceGeneration )
			sourceGeneration = 1u;
		const uint64_t topologyGeneration = m_terrainMegaVT.getTopologyGeneration();
		if ( sourceGeneration != m_nComposedSourceGeneration || topologyGeneration != m_nComposedTopologyGeneration ) {
			m_nComposedSourceGeneration = sourceGeneration;
			m_nComposedTopologyGeneration = topologyGeneration;
			t3d_comp_surf_editor_invalidate_sources( m_terrainMegaVT, m_composedSurfaceEdits, sourceGeneration );
		}
	}
	if ( m_pTerrain->isEditorAuthoringMode() && Engine::get_singleton() != NULL && Engine::get_singleton()->is_editor_hint() ) {
		Terrain3DData *const pSource = m_pTerrain->dataObject();
		if ( pSource != NULL && ( m_pAuthoring->m_bForceRebakeQueued || m_pAuthoring->m_bForceRebakeActive ) ) {
			m_pAuthoring->m_cooker.updateCold( { *pSource, m_pTerrain->getDataDirectory(),
			                                     t3dMemoryCookingLimit( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() ) },
			                                   m_terrainMegaVT, *this );
		}
		serviceForceRebakeCold();
		serviceComposedReferenceBakeCold();
		// A forced owner pauses automatic recooking, but publications deferred by
		// streaming/readbacks still need render-thread callbacks on later ticks.
		if ( m_pAuthoring->m_bForceRebakeActive || m_pAuthoring->m_bForceRebakeQueued ) {
			( void )m_previewService.queuePreviewVtRepublishCold( previewServices() );
		}
		if ( !m_pAuthoring->m_bForceRebakeActive && !m_pAuthoring->m_bForceRebakeQueued ) {
			Terrain3DMemoryAccountScope authoring_scope( m_pAuthoring->m_cooker.getAuthoringMemoryAccountCold() );
			m_previewService.updateAuthoringCold( previewServices() );
		}
	}
}

Camera3D *Terrain3DRenderCoordinator::editorViewportCameraCold( void ) const
{
	EditorInterface *const editor = EditorInterface::get_singleton();
	SubViewport *const viewport = editor ? editor->get_editor_viewport_3d( 0 ) : NULL;
	return viewport ? viewport->get_camera_3d() : NULL;
}

#endif // T3D_EDITOR_NATIVE
