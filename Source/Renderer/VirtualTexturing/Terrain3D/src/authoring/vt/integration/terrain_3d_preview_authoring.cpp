// Editor-only material VT preview cooking. Runtime keeps preview readback/republish contracts.
#if defined( T3D_EDITOR_NATIVE )
#include "rendering/rd/preview/terrain_3d_rd_preview_service.h"
#include "authoring/vt/io/terrain_3d_vt_godot_cooker.h"
#include "terrain_3d.h"
#include "core/utility/logger.h"
#include "assets/catalog/terrain_3d_assets.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/time.hpp>
#include <string.h>

static uint64_t t3dRdPreviewVtRetryDelayUsec( const uint64_t nRetryCount )
{
	const uint32_t nShift = ( uint32_t )( MIN( nRetryCount > 0u ? nRetryCount - 1u : 0u, 4u ) );
	return 100000u << nShift;
}

static uint64_t t3dRdSharedVtMaterialSourceGeneration( const Ref<Terrain3DAssets>& rAssets )
{
	if ( rAssets.is_null() ) {
		return 0u;
	}
	uint64_t nGeneration = rAssets->getInstanceVtMaterialSourceGeneration();
	const uint64_t nTerrainGeneration = rAssets->getTerrainVtContentGeneration();
	nGeneration ^= nTerrainGeneration + 0x9e3779b97f4a7c15ULL + ( nGeneration << 6u ) + ( nGeneration >> 2u );
	return nGeneration == 0u ? 1u : nGeneration;
}

void Terrain3DRdPreviewService::updateAuthoringCold( const t3dPreviewServices_t& rServices )
{
	if ( rServices.m_pTerrain == NULL ) {
		return;
	}
	const Ref<Terrain3DAssets> assets = rServices.m_pTerrain->assetsResource();
	const uint64_t nSourceGeneration = t3dRdSharedVtMaterialSourceGeneration( assets );
	// Material source changes only mark the package stale. Baking is explicit.

	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		if ( assets.is_valid() && nSourceGeneration != m_nPreviewVtObservedMaterialSourceGeneration ) {
			m_nPreviewVtObservedMaterialSourceGeneration = nSourceGeneration;
			m_nPreviewVtDirtyMaterialSourceGeneration = nSourceGeneration;
			m_bPreviewVtAuthoringDirty = true;
		}
	}
	// Explicitly staged packages are submitted by their requesting owner only.
}

bool Terrain3DRdPreviewService::bakePreviewVirtualTexturesCold( const t3dPreviewServices_t& rServices, String *const pError,
                                                                const bool bForceRebuild, const uint64_t nForceSerial )
{
	if ( rServices.m_pTerrain == NULL || !rServices.m_pTerrain->isEditorAuthoringMode() || Engine::get_singleton() == NULL ||
	     !Engine::get_singleton()->is_editor_hint() ) {
		if ( pError ) {
			*pError = "Virtual-texture authoring bake is available only in Editor Authoring mode";
		}
		return false;
	}
	const Ref<Terrain3DAssets> assets = rServices.m_pTerrain->assetsResource();
	if ( assets.is_null() ) {
		if ( pError ) {
			*pError = "Virtual-texture authoring bake has no Terrain3DAssets source";
		}
		return false;
	}

	// Explicit user action is allowed to retry a quarantined generation. Keep the
	// content-addressed P08 cache intact so unchanged allocations/channels remain hits.
	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		m_bPreviewVtAuthoringDirty = true;
		m_nPreviewVtCookAfterUsec = 0u;
		m_nPreviewVtStagedMaterialSourceGeneration = 0u;
		m_nPreviewVtFailedMaterialSourceGeneration = 0u;
		m_nPreviewVtFailedFingerprint = 0u;
		m_previewVtLastCookError = String();
	}
	m_nPreviewVtObservedMaterialSourceGeneration = t3dRdSharedVtMaterialSourceGeneration( assets );
	return preparePreviewAuthoringPackageCold( rServices, assets, pError, true, bForceRebuild, nForceSerial );
}

bool Terrain3DRdPreviewService::preparePreviewAuthoringPackageCold( const t3dPreviewServices_t& rServices,
                                                                    const Ref<Terrain3DAssets>& rAssets, String *const pError,
                                                                    const bool bImmediate, const bool bForceRebuild,
                                                                    const uint64_t nForceSerial )
{
	if ( rAssets.is_null() ) {
		if ( pError ) {
			*pError = "RD preview VT has no Terrain3DAssets source";
		}
		return false;
	}
	const uint64_t nSourceGeneration = t3dRdSharedVtMaterialSourceGeneration( rAssets );
	const t3dRenderLifecycleStatus_t initialStatus = rServices.m_host.previewLifecycleStatus();
	const bool bLive = initialStatus.m_nSharedResourceClass != T3D_VT_SHARED_RESOURCES_NONE;
	uint64_t nCookRevision = 0u;
	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		if ( m_bPreviewVtRepublishPending || m_bPreviewVtRepublishInProgress ) {
			return true;
		}
		if ( bLive && !bImmediate ) {
			if ( nSourceGeneration != m_nPreviewVtStagedMaterialSourceGeneration && !m_bPreviewVtAuthoringDirty ) {
				m_bPreviewVtAuthoringDirty = true;
				m_nPreviewVtDirtyMaterialSourceGeneration = nSourceGeneration;
				m_nPreviewVtCookAfterUsec = ( Time::get_singleton() != NULL ? Time::get_singleton()->get_ticks_usec() : 0u ) + 150000u;
			}
			return true;
		}
		if ( m_bForceOwnerPending && !bForceRebuild ) {
			return true;
		}
		nCookRevision = m_nPreviewVtRecomposeRevision;
		m_bPreviewVtAuthoringDirty = false;
	}
	m_nPreviewVtObservedMaterialSourceGeneration = nSourceGeneration;
	bool bRepublishPending = false;
	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		if ( !bForceRebuild && nSourceGeneration != 0u && nSourceGeneration == m_nPreviewVtFailedMaterialSourceGeneration ) {
			// Viewport/thumbnail polls keep using the last publication. Failure
			// details remain in status; an explicit bake clears this quarantine.
			if ( !bImmediate ) {
				return true;
			}
			if ( pError ) {
				*pError = !m_previewVtLastRepublishError.is_empty() ? m_previewVtLastRepublishError
				          : m_previewVtLastCookError.is_empty()     ? String( "RD preview VT transient package cook failed" )
				                                                    : m_previewVtLastCookError;
			}
			return false;
		}
		if ( !bForceRebuild && nSourceGeneration != 0u && nSourceGeneration == m_nPreviewVtStagedMaterialSourceGeneration ) {
			bRepublishPending = m_bPreviewVtRepublishPending;
			if ( !bRepublishPending ) {
				return true;
			}
		}
	}
	if ( bRepublishPending ) {
		( void )queuePreviewVtRepublishCold( rServices );
		return true;
	}

	// Failed source generations stay quarantined until a texture edit or explicit bake.
	t3dVTGodotAssetMaterials_t materials;
	String collectError;
	if ( !t3d_vt_godot_collect_asset_materials( rAssets, materials, &collectError ) ) {
		Terrain3DScopedLock lock( m_previewVtMutex );
		m_nPreviewVtFailedMaterialSourceGeneration = nSourceGeneration;
		m_previewVtLastCookError = collectError.is_empty() ? String( "RD preview VT material collection failed" ) : collectError;
		++m_nPreviewVtCookFailureCount;
		if ( pError ) {
			*pError = m_previewVtLastCookError;
		}
		return false;
	}

	const uint32_t nMaterialCount = ( uint32_t )materials.m_stableKeys.size();
	PackedByteArray packageBytes;
	Terrain3DSharedMemoryReservation package_memory;
	uint64_t nFingerprint = 0u;
	uint32_t nPackageGeneration = 0u;
	t3dVTEditorCookTelemetry_t cookTelemetry{};
	bool bStageNeutral = nMaterialCount == 0u;
	if ( !bStageNeutral ) {
		if ( m_previewVtEditorCache.m_CacheDirectory.is_empty() ) {
			String cacheDirectory;
			ProjectSettings *const pSettings = ProjectSettings::get_singleton();
			if ( pSettings != NULL ) {
				cacheDirectory = pSettings->globalize_path( "res://.godot/terrain3d/vt_cache" );
			}
			if ( cacheDirectory.is_empty() && rServices.m_pTerrain != NULL ) {
				cacheDirectory = rServices.m_pTerrain->getDataDirectory().path_join( ".vt_cache" );
			}
			t3d_vt_godot_editor_cache_set_directory( m_previewVtEditorCache, cacheDirectory );
		}
		String cookError;
		if ( !t3d_vt_godot_cook_editor_package_incremental(
		         rAssets, materials.m_stableKeys.data(), materials.m_records.data(), nMaterialCount, m_previewVtEditorCache, packageBytes,
		         nFingerprint, nPackageGeneration, cookTelemetry, &cookError, &package_memory, bForceRebuild ) ||
		     nFingerprint == 0u || packageBytes.is_empty() ) {
			Terrain3DScopedLock lock( m_previewVtMutex );
			m_previewVtEditorTelemetry = cookTelemetry;
			m_nPreviewVtFailedMaterialSourceGeneration = nSourceGeneration;
			m_nPreviewVtFailedFingerprint = nFingerprint;
			m_previewVtLastCookError = cookError.is_empty() ? String( "RD preview VT transient package cook failed" ) : cookError;
			++m_nPreviewVtCookFailureCount;
			if ( pError ) {
				*pError = m_previewVtLastCookError;
			}
			return false;
		}
	}

	if ( !rServices.m_host.composePreviewTerrainPackageCold( packageBytes, nPackageGeneration, nFingerprint, package_memory ) ) {
		const String compositionError = rServices.m_host.previewTerrainCompositionErrorCold();
		Terrain3DScopedLock lock( m_previewVtMutex );
		m_nPreviewVtFailedMaterialSourceGeneration = nSourceGeneration;
		m_previewVtLastCookError = compositionError;
		++m_nPreviewVtCookFailureCount;
		if ( pError ) {
			*pError = m_previewVtLastCookError;
		}
		return false;
	}
	bStageNeutral = packageBytes.is_empty();
	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		if ( nSourceGeneration != t3dRdSharedVtMaterialSourceGeneration( rAssets ) || nCookRevision != m_nPreviewVtRecomposeRevision ) {
			m_bPreviewVtAuthoringDirty = true;
			m_nPreviewVtCookAfterUsec = ( Time::get_singleton() != NULL ? Time::get_singleton()->get_ticks_usec() : 0u ) + 150000u;
			if ( bForceRebuild ) {
				m_previewVtLastCookError = "Terrain or material sources changed during forced VT cook";
				if ( pError ) {
					*pError = m_previewVtLastCookError;
				}
				return false;
			}
			return true;
		}
	}

	const t3dRenderLifecycleStatus_t renderStatus = rServices.m_host.previewLifecycleStatus();
	const bool bConsumersLive = renderStatus.m_nSharedResourceClass != T3D_VT_SHARED_RESOURCES_NONE ||
	                            ( renderStatus.m_nReadyMask & T3D_RENDER_READY_RESOURCE_CONSUMERS ) != 0u;
	const t3dVTSharedResourceClass_t eLiveClass = ( t3dVTSharedResourceClass_t )renderStatus.m_nSharedResourceClass;
	bool bNeedsPublication = false;
	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		if ( !bForceRebuild && nFingerprint == m_nPreviewVtFailedFingerprint ) {
			m_nPreviewVtFailedMaterialSourceGeneration = nSourceGeneration;
			return true;
		}
		bNeedsPublication = bForceRebuild || bStageNeutral != m_bPreviewVtPublishedNeutral ||
		                    ( !bStageNeutral && nFingerprint != m_nPreviewVtPublishedFingerprint );
		m_nForceStagedSerial = bForceRebuild ? nForceSerial : 0u;
		m_previewVtPackageBytes = packageBytes;
		m_packageMemory = package_memory;
		m_nPreviewVtFingerprint = nFingerprint;
		m_nPreviewVtPackageGeneration = nPackageGeneration;
		m_nPreviewVtStagedMaterialSourceGeneration = nSourceGeneration;
		m_nPreviewVtFailedMaterialSourceGeneration = 0u;
		m_nPreviewVtFailedFingerprint = 0u;
		m_previewVtLastCookError = String();
		m_previewVtLastRepublishError = String();
		m_previewVtEditorTelemetry = cookTelemetry;
		m_bPreviewVtPackageValid = !bStageNeutral;
		m_bPreviewVtStageNeutral = bStageNeutral;
		if ( !bStageNeutral && cookTelemetry.m_nDirtyAllocations != 0u ) {
			++m_nPreviewVtRecookCount;
		}
		m_bPreviewVtRepublishPending = bConsumersLive && bNeedsPublication;
		if ( m_bPreviewVtRepublishPending ) {
			m_ePreviewVtRepublishResourceClass = eLiveClass;
		}
	}

	if ( !bNeedsPublication ) {
		return true;
	}
	if ( bConsumersLive ) {
		( void )queuePreviewVtRepublishCold( rServices );
		return true;
	}

	const bool bPublished = bStageNeutral ? rServices.m_virtualTexture.clearPackageCold()
	                                      : rServices.m_host.loadPreviewPackage( packageBytes.ptr(), ( uint64_t )packageBytes.size() );
	if ( !bPublished ) {
		const uint64_t nNowUsec = Time::get_singleton() != NULL ? Time::get_singleton()->get_ticks_usec() : 0u;
		{
			Terrain3DScopedLock lock( m_previewVtMutex );
			m_bPreviewVtRepublishPending = true;
			m_ePreviewVtRepublishResourceClass = T3D_VT_SHARED_RESOURCES_NONE;
			++m_nPreviewVtRepublishRetryCount;
			m_nPreviewVtRepublishRetryAfterUsec = nNowUsec + t3dRdPreviewVtRetryDelayUsec( m_nPreviewVtRepublishRetryCount );
			if ( bForceRebuild ) {
				m_bPreviewVtRepublishPending = false;
				m_nPreviewVtRepublishRetryAfterUsec = 0u;
			}
			m_previewVtLastRepublishError = bStageNeutral ? String( "RD preview VT neutral publication failed; retry scheduled" )
			                                              : String( "RD preview VT transient package publication failed; retry scheduled" );
			++m_nPreviewVtRepublishFailureCount;
			if ( pError ) {
				*pError = m_previewVtLastRepublishError;
			}
		}
		return true;
	}
	{
		Terrain3DScopedLock lock( m_previewVtMutex );
		m_nForcePublishedSerial = MAX( m_nForcePublishedSerial, m_nForceStagedSerial );
		m_previewVtPublishedPackageBytes = packageBytes;
		m_publishedPackageMemory = m_packageMemory;
		m_nPreviewVtPublishedFingerprint = nFingerprint;
		m_nPreviewVtPublishedPackageGeneration = nPackageGeneration;
		m_bPreviewVtPublishedNeutral = bStageNeutral;
		m_previewVtLastRepublishError = String();
		m_nPreviewVtRepublishRetryCount = 0u;
		m_nPreviewVtRepublishRetryAfterUsec = 0u;
		++m_nPreviewVtPublishCount;
	}
	return true;
}

#endif // T3D_EDITOR_NATIVE
