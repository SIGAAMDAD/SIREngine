#include "authoring/vt/integration/terrain_3d_composed_surface_editor_source.h"
#if defined( T3D_EDITOR_NATIVE )

#include "authoring/vt/build/terrain_3d_composed_surface_cold_pipeline.h"
#include "authoring/vt/build/terrain_3d_composed_surface_godot_decode.h"
#include "authoring/vt/build/terrain_3d_composed_surface_encoder_adapter.h"
#include "authoring/vt/integration/terrain_3d_composed_surface_coordinator_hooks.h"
#include "assets/catalog/terrain_3d_assets.h"
#include "assets/resources/terrain_3d_texture_asset.h"
#include "data/model/terrain_3d_data.h"
#include "rendering/material/terrain_3d_material.h"
#include "rendering/rd/shaders/terrain_3d_rd_shader_library.h"
#include "rendering/rd/vt/runtime/terrain_3d_terrain_megavt.h"

#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_cache.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"
#include "data/persistence/terrain_3d_file_publication.h"
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <vector>
#include <cmath>

using namespace godot;

// One additional control sample is needed for the four-corner interpolation.
#define T3D_COMP_SURF_EDITOR_SNAPSHOT_SIDE ( T3D_COMP_SURF_PHYSICAL_TILE + 4u )
#define T3D_COMP_SURF_EDITOR_SNAPSHOT_COUNT ( T3D_COMP_SURF_EDITOR_SNAPSHOT_SIDE * T3D_COMP_SURF_EDITOR_SNAPSHOT_SIDE )

struct t3dCompSurfEditorControlTile_t
{
	uint32_t m_nRegionIndex = 0u;
	uint32_t m_nPageX = 0u;
	uint32_t m_nPageY = 0u;
	uint32_t m_nMip = 0u;
	uint32_t m_nSnapshotSide = 0u;
	float m_fWorldOriginX = 0.0f;
	float m_fWorldOriginZ = 0.0f;
	float m_fWorldStep = 1.0f;
	int32_t m_nOriginX = 0;
	int32_t m_nOriginZ = 0;
	PackedInt32Array m_controls;
	// Immutable editor-only geometry snapshots, indexed by composed output
	// texel (including gutters). Present only for auto-shader compositions.
	PackedFloat32Array m_heights;
	PackedFloat32Array m_normalY;
	uint64_t m_nControlsRevision = 0u;
};

struct t3dCompSurfEditorSourceContext_t
{
	Terrain3DData *m_pData = nullptr;
	const Terrain3DTerrainMegaVT *m_pMegaVT = nullptr;
	const Terrain3DComposedSurfaceEditJournal *m_pEdits = nullptr;
	const Terrain3DAssets *m_pAssets = nullptr;
	const Terrain3DMaterial *m_pMaterial = nullptr;
	std::vector<t3dCompSurfEditorControlTile_t> m_tiles;
	const t3dCompSurfEditorControlTile_t *m_pActive = nullptr;
	uint64_t m_nTerrainRevision = 0u;
	uint64_t m_nMaterialRevision = 0u;
	uint64_t m_nTopologyRevision = 0u;
};

static uint64_t t3d_comp_surf_editor_material_revision( const Terrain3DAssets& rAssets, const Terrain3DMaterial& rMaterial )
{
	const uint64_t a = rAssets.getTerrainVtContentGeneration();
	const uint64_t b = rAssets.getInstanceVtMaterialSourceGeneration();
	uint64_t revision = a ^ ( b + 0x9e3779b97f4a7c15ull + ( a << 6u ) + ( a >> 2u ) );
	// The canonical raster reads authored blend sharpness independently of the
	// texture assets. Include it in the cold identity and in stale-job checks.
	const Variant authored = rMaterial.getShaderParam( StringName( "blend_sharpness" ) );
	const float sharpness = ( authored.get_type() == Variant::FLOAT || authored.get_type() == Variant::INT ) ? float( authored ) : 0.5f;
	uint32_t sharpnessBits = 0u;
	memcpy( &sharpnessBits, &sharpness, sizeof( sharpnessBits ) );
	revision ^= uint64_t( sharpnessBits ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
	// These authored controls are read again when the candidate is made.
	// A material edit during a long cold cook must invalidate that result,
	// including LOD/filtering changes that do not touch texture assets.
	const bool unified = Terrain3DRdShaderLibrary::isUnifiedCookedPreviewEnabledCold();
	const uint32_t flags[] = { uint32_t( unified ? rMaterial.getTerrainSamplingMode() : rMaterial.getMiddleTerrainSamplingMode() ),
	                           uint32_t( unified ? rMaterial.getCookedTextureFiltering() : rMaterial.getMiddleTextureFiltering() ),
	                           uint32_t( unified ? rMaterial.getDetilingMode() : rMaterial.getMiddleDetilingMode() ),
	                           uint32_t( rMaterial.getMiddleAutoShaderEnabled() ),
	                           uint32_t( rMaterial.getDualScalingEnabled() ),
	                           uint32_t( rMaterial.getProjectionEnabled() ) };
	for ( uint32_t flag : flags ) {
		revision ^= uint64_t( flag ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
	}
	// An auto-control blend changes when these authored values change,
	// even when the terrain VT source pack is byte-identical.
	const StringName autoKeys[] = { StringName( "auto_slope" ), StringName( "auto_height_reduction" ), StringName( "auto_base_texture" ),
	                                StringName( "auto_overlay_texture" ) };
	for ( const StringName& key : autoKeys ) {
		const Variant value = rMaterial.getShaderParam( key );
		const float number = ( value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT ) ? float( value ) : 0.0f;
		uint32_t bits = 0u;
		memcpy( &bits, &number, sizeof( bits ) );
		revision ^= uint64_t( bits ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
	}
	// Layer detiling values are also static cook inputs.
	const PackedVector2Array detiles = rAssets.getTextureDetiles();
	for ( int64_t i = 0; i < detiles.size(); ++i ) {
		const Vector2 value = detiles[i];
		const float pair[] = { float( value.x ), float( value.y ) };
		for ( float component : pair ) {
			uint32_t bits = 0u;
			memcpy( &bits, &component, sizeof( bits ) );
			revision ^= uint64_t( bits ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
		}
	}
	// The asset generation is a fast invalidation signal, but the canonical
	// material also reads individual authored layer uniforms. Hash those
	// values directly so a setter which misses the asset generation counter
	// cannot silently validate an obsolete long-running editor cook.
	const uint32_t layerCount = uint32_t( rAssets.getTextureCount() );
	revision ^= uint64_t( layerCount ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
	for ( uint32_t i = 0u; i < layerCount; ++i ) {
		const Ref<Terrain3DTextureAsset> asset = rAssets.getTextureAsset( int( i ) );
		if ( asset.is_null() ) {
			revision ^= 0x756e617661696c31ull + ( revision << 6u ) + ( revision >> 2u );
			continue;
		}
		const Color tint = asset->getAlbedoColor();
		const float values[] = { float( tint.r ),
		                         float( tint.g ),
		                         float( tint.b ),
		                         float( tint.a ),
		                         float( asset->getUvScale() ),
		                         float( asset->getNormalDepth() ),
		                         float( asset->getRoughness() ),
		                         float( asset->getMetallic() ),
		                         float( asset->getAoStrength() ),
		                         float( asset->getAoLightAffect() ) };
		for ( float value : values ) {
			uint32_t bits = 0u;
			memcpy( &bits, &value, sizeof( bits ) );
			revision ^= uint64_t( bits ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
		}
	}
	const float lodBias = float( unified ? rMaterial.getCookedMaterialLodBias() : rMaterial.getMiddleMaterialLodBias() );
	uint32_t lodBits = 0u;
	memcpy( &lodBits, &lodBias, sizeof( lodBits ) );
	revision ^= uint64_t( lodBits ) + 0x9e3779b97f4a7c15ull + ( revision << 6u ) + ( revision >> 2u );
	return revision ? revision : 1u;
}

static uint64_t t3d_comp_surf_editor_terrain_revision( const Terrain3DComposedSurfaceEditJournal& rJournal )
{
	const auto& s = rJournal.statsCold();
	uint64_t result = 1u + s.m_nEditEvents + s.m_nSourceInvalidations + s.m_nRemovedRegions;
	return result ? result : 1u;
}

static bool t3d_comp_surf_editor_current( void *pUser, const uint64_t nTerrain, const uint64_t nMaterial, const uint64_t nTopology )
{
	const auto& c = *static_cast<const t3dCompSurfEditorSourceContext_t *>( pUser );
	return c.m_pData && c.m_pMegaVT && c.m_pEdits && c.m_pAssets && c.m_pMaterial && nTerrain == c.m_nTerrainRevision &&
	       nMaterial == c.m_nMaterialRevision && nTopology == c.m_nTopologyRevision &&
	       t3d_comp_surf_editor_terrain_revision( *c.m_pEdits ) == nTerrain &&
	       t3d_comp_surf_editor_material_revision( *c.m_pAssets, *c.m_pMaterial ) == nMaterial &&
	       c.m_pMegaVT->getTopologyGeneration() == nTopology;
}

static bool t3d_comp_surf_editor_page( void *pUser, const t3dComposedSurfaceCookPage_t& rPage, t3dComposedStaticFootprint_t& rFootprint )
{
	auto& c = *static_cast<t3dCompSurfEditorSourceContext_t *>( pUser );
	c.m_pActive = nullptr;
	if ( rPage.m_nRegionKey == 0u || rPage.m_nRegionKey > T3D_COMP_SURF_EDITOR_REGION_COUNT )
		return false;
	const uint32_t nIndex = uint32_t( rPage.m_nRegionKey - 1u );
	for ( const auto& tile : c.m_tiles ) {
		if ( tile.m_nRegionIndex != nIndex || tile.m_nPageX != rPage.m_nPageX || tile.m_nPageY != rPage.m_nPageY ||
		     tile.m_nMip != rPage.m_nMip )
			continue;
		c.m_pActive = &tile;
		rFootprint = {};
		rFootprint.m_fWorldOrigin[0] = tile.m_fWorldOriginX;
		rFootprint.m_fWorldOrigin[1] = tile.m_fWorldOriginZ;
		rFootprint.m_fControlOrigin[0] = rFootprint.m_fWorldOrigin[0];
		rFootprint.m_fControlOrigin[1] = rFootprint.m_fWorldOrigin[1];
		rFootprint.m_fWorldStepX[0] = tile.m_fWorldStep;
		rFootprint.m_fWorldStepY[1] = tile.m_fWorldStep;
		rFootprint.m_fControlStepX[0] = tile.m_fWorldStep;
		rFootprint.m_fControlStepY[1] = tile.m_fWorldStep;
		return true;
	}
	return false;
}

static bool t3d_comp_surf_editor_control( void *pUser, const int32_t nX, const int32_t nZ, uint32_t& rControl )
{
	const auto& c = *static_cast<const t3dCompSurfEditorSourceContext_t *>( pUser );
	const auto *pTile = c.m_pActive;
	if ( !pTile )
		return false;
	const int64_t x = int64_t( nX ) - pTile->m_nOriginX;
	const int64_t z = int64_t( nZ ) - pTile->m_nOriginZ;
	if ( x < 0 || z < 0 || x >= pTile->m_nSnapshotSide || z >= pTile->m_nSnapshotSide )
		return false;
	const uint32_t index = uint32_t( z * pTile->m_nSnapshotSide + x );
	if ( index >= uint32_t( pTile->m_controls.size() ) )
		return false;
	rControl = uint32_t( pTile->m_controls[index] );
	// Missing source cells remain non-authoritative: no fabricated controls.
	return rControl != UINT32_MAX;
}

static bool t3d_comp_surf_editor_surface( void *pUser, uint32_t x, uint32_t y, float& height, float& normal_y )
{
	const auto& c = *static_cast<const t3dCompSurfEditorSourceContext_t *>( pUser );
	const auto *pTile = c.m_pActive;
	if ( !pTile || x >= T3D_COMP_SURF_PHYSICAL_TILE || y >= T3D_COMP_SURF_PHYSICAL_TILE )
		return false;
	const uint32_t i = y * T3D_COMP_SURF_PHYSICAL_TILE + x;
	if ( i >= uint32_t( pTile->m_heights.size() ) || i >= uint32_t( pTile->m_normalY.size() ) )
		return false;
	height = pTile->m_heights[i];
	normal_y = pTile->m_normalY[i];
	return std::isfinite( height ) && std::isfinite( normal_y );
}

bool t3d_comp_surf_cook_editor_candidate_cold( const PackedByteArray& rPack, Terrain3DData& rData, const Ref<Terrain3DAssets>& rAssets,
                                               const Ref<Terrain3DMaterial>& rMaterial, const Terrain3DTerrainMegaVT& rMegaVT,
                                               const Terrain3DComposedSurfaceEditJournal& rEdits, const String& rDirectory,
                                               t3dCompSurfEditorCandidateResult_t& rResult )
{
	rResult = {};
	rResult.m_error = "invalid_source";
	if ( rPack.is_empty() || rAssets.is_null() || rMaterial.is_null() || rDirectory.is_empty() || !rMegaVT.isReady() )
		return false;
	const bool unified = Terrain3DRdShaderLibrary::isUnifiedCookedPreviewEnabledCold();
	const uint32_t nMode = uint32_t( unified ? rMaterial->getTerrainSamplingMode() : rMaterial->getMiddleTerrainSamplingMode() );
	const t3dTextureFiltering_t filter = unified ? rMaterial->getCookedTextureFiltering() : rMaterial->getMiddleTextureFiltering();
	const t3dDetilingMode_t detilingMode = unified ? rMaterial->getDetilingMode() : rMaterial->getMiddleDetilingMode();
	if ( ( nMode != 1u && nMode != 2u && nMode != 5u ) || ( filter != LINEAR && filter != NEAREST ) ||
	     ( detilingMode != DETILING_DISABLED && detilingMode != DETILING_DISCRETE_8 ) || rMaterial->getDualScalingEnabled() ||
	     rMaterial->getProjectionEnabled() || rData.getVertexSpacingInternal() != 1.0 ) {
		rResult.m_error = "unsupported_static_material_mode";
		return false;
	}
	const uint32_t nLayers = uint32_t( rAssets->getTextureCount() );
	if ( !nLayers || nLayers > T3D_COMP_SURF_STATIC_MAX_LAYERS ) {
		rResult.m_error = "unsupported_material_count";
		return false;
	}
	const bool autoShader = rMaterial->getMiddleAutoShaderEnabled();
	const bool detiling = detilingMode != DETILING_DISABLED;
	const PackedVector2Array detiles = rAssets->getTextureDetiles();
	t3dComposedStaticLayer_t layers[T3D_COMP_SURF_STATIC_MAX_LAYERS]{};
	t3dComposedStaticLayer_t resolved[T3D_COMP_SURF_STATIC_MAX_LAYERS]{};
	for ( uint32_t i = 0u; i < nLayers; ++i ) {
		const Ref<Terrain3DTextureAsset> asset = rAssets->getTextureAsset( int( i ) );
		if ( asset.is_null() ) {
			rResult.m_error = "missing_material";
			return false;
		}
		const Color color = asset->getAlbedoColor();
		layers[i].m_fColor[0] = float( color.r );
		layers[i].m_fColor[1] = float( color.g );
		layers[i].m_fColor[2] = float( color.b );
		layers[i].m_fPbr0[0] = float( asset->getUvScale() );
		layers[i].m_fPbr0[1] = float( asset->getNormalDepth() );
		layers[i].m_fPbr0[2] = float( asset->getRoughness() );
		layers[i].m_fPbr0[3] = float( asset->getMetallic() );
		layers[i].m_fPbr1[0] = float( asset->getAoStrength() );
		layers[i].m_fPbr1[1] = float( asset->getAoLightAffect() );
		const Vector2 detile = i < uint32_t( detiles.size() ) ? detiles[int64_t( i )] : Vector2();
		layers[i].m_fPbr1[2] = float( detile.x );
		layers[i].m_fPbr1[3] = float( detile.y );
	}
	t3dCompSurfEditorSourceContext_t context{};
	context.m_pData = &rData;
	context.m_pMegaVT = &rMegaVT;
	context.m_pEdits = &rEdits;
	context.m_pAssets = rAssets.ptr();
	context.m_pMaterial = rMaterial.ptr();
	context.m_nTerrainRevision = t3d_comp_surf_editor_terrain_revision( rEdits );
	context.m_nMaterialRevision = t3d_comp_surf_editor_material_revision( *rAssets.ptr(), *rMaterial.ptr() );
	context.m_nTopologyRevision = rMegaVT.getTopologyGeneration();
	if ( !context.m_nTopologyRevision ) {
		rResult.m_error = "terrain_topology_unavailable";
		return false;
	}

	std::vector<t3dComposedSurfaceCookPage_t> pages;
	pages.reserve( 128u );
	context.m_tiles.reserve( 128u );
	uint64_t controlsHash = 14695981039346656037ull;
	for ( uint32_t region = 0u; region < T3D_COMP_SURF_EDITOR_REGION_COUNT; ++region ) {
		t3dComposedSurfaceEditRegion_t info{};
		if ( !t3d_comp_surf_editor_region( rMegaVT, region, info ) || !rData.getRegionPtr( Vector2i( info.m_nRegionX, info.m_nRegionZ ) ) )
			continue;
		// The existing MegaVT supports 2 or 4 texels per meter. Compose the
		// matching 2x2 / 4x4 pages rather than silently rejecting every editor
		// terrain at the default density. This remains reference-only output.
		const uint32_t tpm = rMegaVT.getTexelsPerMeter();
		if ( ( tpm != 2u && tpm != 4u ) || info.m_nWidth != tpm || info.m_nHeight != tpm ) {
			rResult.m_error = "unsupported_region_page_layout";
			return false;
		}
		// Bake the entire region-local authored mip hierarchy, rather than
		// discarding all but mip 0 and asking the V1 importer to invent it.
		// Each successive level covers the same 128 m region at half the
		// resolution; a real neighbor halo is still sampled at every level.
		const uint32_t mipCount = tpm == 4u ? 3u : 2u;
		if ( info.m_nMips < mipCount ) {
			rResult.m_error = "incomplete_composed_region_mips";
			return false;
		}
		for ( uint32_t mip = 0u; mip < mipCount; ++mip ) {
			const uint32_t pagesWide = MAX( 1u, tpm >> mip );
			const uint32_t texelsPerCell = 1u << mip;
			for ( uint32_t pageY = 0u; pageY < pagesWide; ++pageY ) {
				for ( uint32_t pageX = 0u; pageX < pagesWide; ++pageX ) {
					t3dCompSurfEditorControlTile_t tile{};
					tile.m_nRegionIndex = region;
					tile.m_nPageX = pageX;
					tile.m_nPageY = pageY;
					tile.m_nMip = mip;
					tile.m_fWorldStep = float( texelsPerCell ) / float( tpm );
					tile.m_fWorldOriginX =
					    float( info.m_nRegionX * 128 ) + float( pageX * T3D_COMP_SURF_PAGE_INTERIOR ) * tile.m_fWorldStep;
					tile.m_fWorldOriginZ =
					    float( info.m_nRegionZ * 128 ) + float( pageY * T3D_COMP_SURF_PAGE_INTERIOR ) * tile.m_fWorldStep;
					const uint32_t gutterCells = ( T3D_COMP_SURF_PAGE_GUTTER * texelsPerCell + tpm - 1u ) / tpm;
					const uint32_t interiorCells = ( T3D_COMP_SURF_PAGE_INTERIOR * texelsPerCell ) / tpm;
					tile.m_nOriginX = int32_t( floorf( tile.m_fWorldOriginX ) ) - int32_t( gutterCells );
					tile.m_nOriginZ = int32_t( floorf( tile.m_fWorldOriginZ ) ) - int32_t( gutterCells );
					tile.m_nSnapshotSide = interiorCells + gutterCells * 2u + 3u;
					const uint32_t count = tile.m_nSnapshotSide * tile.m_nSnapshotSide;
					if ( count > T3D_COMP_SURF_EDITOR_SNAPSHOT_COUNT )
						return false;
					PackedVector3Array positions;
					positions.resize( count );
					Vector3 *pPositions = positions.ptrw();
					for ( uint32_t z = 0u; z < tile.m_nSnapshotSide; ++z ) {
						for ( uint32_t x = 0u; x < tile.m_nSnapshotSide; ++x ) {
							pPositions[z * tile.m_nSnapshotSide + x] = Vector3( float( tile.m_nOriginX + int32_t( x ) ) + 0.5f, 0.0f,
							                                                    float( tile.m_nOriginZ + int32_t( z ) ) + 0.5f );
						}
					}
					tile.m_controls = rData.getControls( positions );
					if ( tile.m_controls.size() != count ) {
						rResult.m_error = "control_snapshot_failed";
						return false;
					}
					const int32_t *pControl = tile.m_controls.ptr();
					// A missing *exterior* neighbor cannot block a diagnostic bake of
					// an isolated region. Clamp only its physical gutter to the nearest
					// valid authored interior cell, recording the substitution. Interior
					// holes/failed source reads remain fatal. Boundary-clamped candidate
					// pages are NEVER renderer-authoritative without a seam parity test.
					bool boundaryClamped = false;
					const int32_t first = int32_t( gutterCells );
					const int32_t last = first + int32_t( interiorCells ) - 1;
					int32_t *pWritable = tile.m_controls.ptrw();
					for ( uint32_t z = 0u; z < tile.m_nSnapshotSide; ++z ) {
						for ( uint32_t x = 0u; x < tile.m_nSnapshotSide; ++x ) {
							const uint32_t i = z * tile.m_nSnapshotSide + x;
							if ( uint32_t( pWritable[i] ) != UINT32_MAX )
								continue;
							const int32_t ix = int32_t( x ), iz = int32_t( z );
							if ( ix >= first && ix <= last && iz >= first && iz <= last ) {
								rResult.m_error = "missing_interior_control";
								return false;
							}
							const int32_t sx = ix < first ? first : ( ix > last ? last : ix );
							const int32_t sz = iz < first ? first : ( iz > last ? last : iz );
							const uint32_t source = uint32_t( sz ) * tile.m_nSnapshotSide + uint32_t( sx );
							if ( uint32_t( pWritable[source] ) == UINT32_MAX ) {
								rResult.m_error = "missing_control_halo_source";
								return false;
							}
							pWritable[i] = pWritable[source];
							boundaryClamped = true;
						}
					}
					if ( boundaryClamped )
						++rResult.m_nBoundaryClampedPages;
					pControl = tile.m_controls.ptr();
					uint64_t pageHash = 14695981039346656037ull;
					bool pageUsesAuto = false;
					for ( uint32_t i = 0u; i < count; ++i ) {
						const uint32_t value = uint32_t( pControl[i] );
						pageUsesAuto |= ( value & 1u ) != 0u;
						for ( uint32_t b = 0u; b < 4u; ++b ) {
							const uint8_t byte = uint8_t( value >> ( b * 8u ) );
							pageHash = ( pageHash ^ byte ) * 1099511628211ull;
							controlsHash = ( controlsHash ^ byte ) * 1099511628211ull;
						}
					}
					// Avoid height/normal snapshots for authored pages without auto bits.
					// Their material fingerprint must remain independent of height edits.
					if ( autoShader && pageUsesAuto ) {
						PackedVector3Array surfacePositions;
						surfacePositions.resize( T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE );
						Vector3 *pSurfacePositions = surfacePositions.ptrw();
						for ( uint32_t py = 0u; py < T3D_COMP_SURF_PHYSICAL_TILE; ++py ) {
							for ( uint32_t px = 0u; px < T3D_COMP_SURF_PHYSICAL_TILE; ++px ) {
								const float sx = float( int32_t( px ) - int32_t( T3D_COMP_SURF_PAGE_GUTTER ) ) + 0.5f;
								const float sy = float( int32_t( py ) - int32_t( T3D_COMP_SURF_PAGE_GUTTER ) ) + 0.5f;
								pSurfacePositions[py * T3D_COMP_SURF_PHYSICAL_TILE + px] = Vector3(
								    tile.m_fWorldOriginX + sx * tile.m_fWorldStep, 0.0f, tile.m_fWorldOriginZ + sy * tile.m_fWorldStep );
							}
						}
						tile.m_heights = rData.getHeights( surfacePositions );
						const PackedVector3Array normals = rData.getNormals( surfacePositions );
						if ( tile.m_heights.size() != surfacePositions.size() || normals.size() != surfacePositions.size() ) {
							rResult.m_error = "auto_surface_snapshot_failed";
							return false;
						}
						tile.m_normalY.resize( normals.size() );
						float *pNormalY = tile.m_normalY.ptrw();
						const Vector3 *pNormals = normals.ptr();
						const float *pHeight = tile.m_heights.ptr();
						for ( uint32_t n = 0u; n < uint32_t( normals.size() ); ++n ) {
							pNormalY[n] = float( pNormals[n].y );
							// Fold sampled geometry into each page's material identity.
							// Height sculpting can alter an auto-control blend while the
							// authored packed control words remain unchanged.
							const float geometry[] = { pHeight[n], pNormalY[n] };
							for ( float sample : geometry ) {
								uint32_t bits = 0u;
								memcpy( &bits, &sample, sizeof( bits ) );
								for ( uint32_t b = 0u; b < 4u; ++b )
									pageHash = ( pageHash ^ uint8_t( bits >> ( b * 8u ) ) ) * 1099511628211ull;
							}
						}
					}
					tile.m_nControlsRevision = pageHash ? pageHash : 1u;
					context.m_tiles.push_back( tile );
					pages.push_back( { uint64_t( region ) + 1u, tile.m_nControlsRevision, mip, pageX, pageY, 1u } );
				}
			}
		}
	}
	if ( pages.empty() ) {
		rResult.m_error = "no_eligible_regions";
		return false;
	}
	if ( !t3d_comp_surf_editor_current( &context, context.m_nTerrainRevision, context.m_nMaterialRevision, context.m_nTopologyRevision ) ) {
		rResult.m_error = "source_changed_while_snapshotting";
		return false;
	}
	uint64_t bytes = 0u;
	if ( !t3d_comp_surf_cook_total_bytes( uint32_t( pages.size() ), bytes ) || bytes > INT32_MAX ) {
		rResult.m_error = "composed_budget_exceeded";
		return false;
	}
	PackedByteArray output, scratch, cache;
	output.resize( int64_t( bytes ) );
	scratch.resize( T3D_COMP_SURF_COOK_FUSED_SCRATCH_BYTES );
	cache.resize( 4u * T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES );
	if ( uint64_t( output.size() ) != bytes || scratch.is_empty() || cache.is_empty() ) {
		rResult.m_error = "authoring_memory_exhausted";
		return false;
	}
	const String path = rDirectory.path_join( "terrain_composed.t3sc.candidate" );
	// Preserve the last verified candidate until the new cook has been
	// reopened, CRC checked and source-generation checked. This uses the
	// existing rollback-capable terrain file transaction; the GPU is never
	// handed a partially authored candidate.
	const String stagedPath = path + String( ".staged" );
	Terrain3DFilePublication publication( path, stagedPath, path + String( ".previous" ) );
	if ( publication.prepare() != OK ) {
		rResult.m_error = String( "candidate_publication_prepare_failed: " ) + publication.getError();
		return false;
	}
	const CharString pathUtf8 = stagedPath.utf8();
	t3dComposedSurfaceColdCookRequest_t request{};
	request.m_pSourcePackage = rPack.ptr();
	request.m_nSourcePackageBytes = uint64_t( rPack.size() );
	request.m_pPages = pages.data();
	request.m_nPageCount = uint32_t( pages.size() );
	request.m_pAuthoredLayers = layers;
	request.m_nLayerCount = nLayers;
	request.m_pResolvedLayers = resolved;
	request.m_nResolvedLayerCapacity = T3D_COMP_SURF_STATIC_MAX_LAYERS;
	request.m_source.m_pUser = &context;
	request.m_source.m_pResolvePage = t3d_comp_surf_editor_page;
	request.m_source.m_pReadControl = t3d_comp_surf_editor_control;
	request.m_source.m_pReadTerrainSurface = autoShader ? t3d_comp_surf_editor_surface : nullptr;
	request.m_source.m_bAutoShader = autoShader;
	request.m_source.m_bDetiling = detiling;
	request.m_source.m_nDetilingMode = uint32_t( detilingMode );
	request.m_source.m_nSamplingMode = nMode;
	request.m_source.m_nVtFiltering = filter == LINEAR ? 1u : 3u;
	request.m_source.m_fStaticMipBias = float( unified ? rMaterial->getCookedMaterialLodBias() : rMaterial->getMiddleMaterialLodBias() );
	const Variant authoredBlend = rMaterial->getShaderParam( StringName( "blend_sharpness" ) );
	request.m_source.m_fBlendSharpness =
	    ( authoredBlend.get_type() == Variant::FLOAT || authoredBlend.get_type() == Variant::INT ) ? float( authoredBlend ) : 0.5f;
	const auto material_float = [&]( const char *key, float fallback ) -> float {
		const Variant v = rMaterial->getShaderParam( StringName( key ) );
		return ( v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT ) ? float( v ) : fallback;
	};
	request.m_source.m_fAutoSlope = material_float( "auto_slope", 1.0f );
	request.m_source.m_fAutoHeightReduction = material_float( "auto_height_reduction", 0.1f );
	const float autoBase = material_float( "auto_base_texture", 0.0f );
	const float autoOverlay = material_float( "auto_overlay_texture", 1.0f );
	if ( autoShader && ( !std::isfinite( autoBase ) || !std::isfinite( autoOverlay ) || autoBase < 0.0f || autoOverlay < 0.0f ||
	                     autoBase >= float( nLayers ) || autoOverlay >= float( nLayers ) ) ) {
		rResult.m_error = "unsupported_auto_material_id";
		return false;
	}
	request.m_source.m_nAutoBaseTexture = autoShader ? uint32_t( autoBase ) : 0u;
	request.m_source.m_nAutoOverlayTexture = autoShader ? uint32_t( autoOverlay ) : 1u;
	request.m_pEncode = t3d_comp_surf_reference_encode;
	request.m_pDecode = t3d_composed_surface_pack_decode_godot_image;
	request.m_pDecodedCache = cache.ptrw();
	request.m_nCacheSlots = 4u;
	request.m_pCookScratch = scratch.ptrw();
	request.m_nCookScratchBytes = uint32_t( scratch.size() );
	request.m_pOutput = output.ptrw();
	request.m_nOutputCapacity = bytes;
	request.m_nTerrainRevision = context.m_nTerrainRevision;
	request.m_nMaterialRevision = context.m_nMaterialRevision;
	request.m_nTopologyGeneration = context.m_nTopologyRevision;
	request.m_nControlPayloadFingerprint = controlsHash ? controlsHash : 1u;
	request.m_bPageLocalCompositionIdentity = true;
	request.m_pCurrent = t3d_comp_surf_editor_current;
	request.m_pCurrentUser = &context;
	request.m_bReferenceOnly = true;
	request.m_pDestination = pathUtf8.get_data();
	// Incremental reuse is valid only for a CRC-verified candidate with the
	// same immutable source/material/filter identity and per-page revision.
	const uint64_t identity = t3d_composed_surface_source_fingerprint_cold( request );
	PackedByteArray previous = FileAccess::get_file_as_bytes( path );
	t3dComposedSurfaceSidecarView_t priorView{};
	if ( !previous.is_empty() && t3d_comp_surf_validate_sidecar_bytes( previous.ptr(), size_t( previous.size() ), true ) &&
	     t3d_comp_surf_open_sidecar_cold( previous.ptr(), uint64_t( previous.size() ), identity, priorView ) ) {
		request.m_pPreviousSidecar = previous.ptr();
		request.m_nPreviousSidecarBytes = uint64_t( previous.size() );
		for ( const auto& page : pages ) {
			uint32_t index = 0u;
			if ( t3d_comp_surf_find_page( priorView, page.m_nRegionKey, page.m_nMip, page.m_nPageX, page.m_nPageY,
			                              page.m_nCompositionRevision, index ) )
				++rResult.m_nReusedPages;
		}
	}
	t3dComposedSurfaceColdCookResult_t cooked{};
	rResult.m_nPages = request.m_nPageCount;
	if ( !t3d_composed_surface_cook_source_pack_cold( request, cooked ) ) {
		( void )publication.rollback();
		rResult.m_error = String( "candidate_cook_failed_status=" ) + String::num_int64( cooked.m_status );
		rResult.m_nReusedPages = 0u;
		return false;
	}
	// Publication is successful only after the actual on-disk sidecar can
	// be reopened and every channel payload passes its CRC checks. This is
	// not a claim of canonical color/DNR/alpha material parity.
	PackedByteArray committed = FileAccess::get_file_as_bytes( stagedPath );
	t3dComposedSurfaceSidecarView_t verified{};
	if ( uint64_t( committed.size() ) != cooked.m_nWrittenBytes ||
	     !t3d_comp_surf_validate_sidecar_bytes( committed.ptr(), size_t( committed.size() ), true ) ||
	     !t3d_comp_surf_open_sidecar_cold( committed.ptr(), uint64_t( committed.size() ), identity, verified ) ||
	     verified.m_header.m_nPageCount != request.m_nPageCount ) {
		( void )publication.rollback();
		rResult.m_error = "candidate_reopen_or_crc_failed";
		rResult.m_nReusedPages = 0u;
		return false;
	}
	// CRC alone proves byte integrity, not a complete, current authored mip
	// hierarchy. Verify all emitted source pages after the atomic disk write
	// before presenting the candidate as a valid editor bake.
	for ( const auto& page : pages ) {
		uint32_t index = 0u;
		if ( !t3d_comp_surf_find_page( verified, page.m_nRegionKey, page.m_nMip, page.m_nPageX, page.m_nPageY, page.m_nCompositionRevision,
		                               index ) ) {
			( void )publication.rollback();
			rResult.m_error = "candidate_missing_or_stale_mip_page";
			rResult.m_nReusedPages = 0u;
			return false;
		}
	}
	if ( !t3d_comp_surf_editor_current( &context, context.m_nTerrainRevision, context.m_nMaterialRevision, context.m_nTopologyRevision ) ) {
		( void )publication.rollback();
		rResult.m_error = "source_changed_before_candidate_acceptance";
		rResult.m_nReusedPages = 0u;
		return false;
	}
	if ( publication.publish() != OK ) {
		( void )publication.rollback();
		rResult.m_error = String( "candidate_publish_failed: " ) + publication.getError();
		rResult.m_nReusedPages = 0u;
		return false;
	}
	if ( publication.commit() != OK ) {
		rResult.m_error = String( "candidate_commit_failed: " ) + publication.getError();
		rResult.m_nReusedPages = 0u;
		return false;
	}
	rResult.m_bSaved = true;
	rResult.m_nWrittenBytes = cooked.m_nWrittenBytes;
	rResult.m_nSourceFingerprint = cooked.m_nSourceFingerprint;
	rResult.m_nDecodedTiles = cooked.m_nDecodedTiles;
	rResult.m_path = path;
	rResult.m_error = String();
	return true;
}
#endif // T3D_EDITOR_NATIVE
