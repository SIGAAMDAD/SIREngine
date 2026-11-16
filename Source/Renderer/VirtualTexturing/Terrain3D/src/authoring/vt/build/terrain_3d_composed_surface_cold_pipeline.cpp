#include "authoring/vt/build/terrain_3d_composed_surface_cold_pipeline.h"
#include "authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.h"

#include <cstring>

static bool t3d_composed_surface_cold_disjoint( const void *a, const uint64_t length_a, const void *b, const uint64_t length_b )
{
	if ( !length_a || !length_b )
		return true;
	if ( !a || !b || length_a > UINTPTR_MAX || length_b > UINTPTR_MAX )
		return false;
	const uintptr_t start_a = reinterpret_cast<uintptr_t>( a );
	const uintptr_t start_b = reinterpret_cast<uintptr_t>( b );
	if ( length_a > UINTPTR_MAX - start_a || length_b > UINTPTR_MAX - start_b )
		return false;
	return start_a + uintptr_t( length_a ) <= start_b || start_b + uintptr_t( length_b ) <= start_a;
}

static uint64_t t3d_composed_surface_cold_hash_bytes( uint64_t hash, const void *pData, const uint64_t length )
{
	const uint8_t *p = static_cast<const uint8_t *>( pData );
	for ( uint64_t i = 0u; i < length; ++i )
		hash = ( hash ^ p[i] ) * 1099511628211ull;
	return hash;
}

static uint64_t t3d_composed_surface_cold_hash_integer( uint64_t hash, const uint64_t n )
{
	// Explicit little-endian bytes avoid structure padding, host byte order and
	// compiler-dependent hashing of floating-point object representations.
	for ( uint32_t i = 0u; i < 8u; ++i )
		hash = ( hash ^ uint8_t( n >> ( i * 8u ) ) ) * 1099511628211ull;
	return hash;
}

static uint64_t t3d_composed_surface_cold_hash_float( uint64_t hash, float value )
{
	uint32_t bits = 0u;
	static_assert( sizeof( bits ) == sizeof( value ), "expected binary32" );
	std::memcpy( &bits, &value, sizeof( bits ) );
	return t3d_composed_surface_cold_hash_integer( hash, bits );
}

uint64_t t3d_composed_surface_source_fingerprint_cold( const t3dComposedSurfaceColdCookRequest_t& r )
{
	if ( !r.m_pSourcePackage || r.m_nSourcePackageBytes == 0u || !r.m_pAuthoredLayers || !r.m_nLayerCount ||
	     r.m_nLayerCount > T3D_COMP_SURF_STATIC_MAX_LAYERS || !r.m_nMaterialRevision || !r.m_nTerrainRevision || !r.m_nTopologyGeneration )
		return 0u;
	uint64_t h = 14695981039346656037ull;
	h = t3d_composed_surface_cold_hash_integer( h, T3D_COMP_SURF_COMPOSITION_ABI );
	h = t3d_composed_surface_cold_hash_integer( h, T3D_COMP_SURF_SOURCE_RECIPE_REVISION );
	h = t3d_composed_surface_cold_hash_integer( h, r.m_nSourcePackageBytes );
	h = t3d_composed_surface_cold_hash_bytes( h, r.m_pSourcePackage, r.m_nSourcePackageBytes );
	if ( r.m_bReferenceOnly && r.m_bPageLocalCompositionIdentity ) {
		// A brush stroke changes only its page records, not the immutable
		// V1/material identity. Invalidation still checks current revisions.
		h = t3d_composed_surface_cold_hash_integer( h, 0x43534c4f43414c31ull );
	} else {
		h = t3d_composed_surface_cold_hash_integer( h, r.m_nTerrainRevision );
		h = t3d_composed_surface_cold_hash_integer( h, r.m_nControlPayloadFingerprint );
	}
	h = t3d_composed_surface_cold_hash_integer( h, r.m_nMaterialRevision );
	h = t3d_composed_surface_cold_hash_integer( h, r.m_nTopologyGeneration );
	h = t3d_composed_surface_cold_hash_integer( h, r.m_nLayerCount );
	for ( uint32_t i = 0u; i < r.m_nLayerCount; ++i ) {
		// Logical indices are stable. Physical slot/allocation IDs are omitted
		// because they are assigned anew by a valid source V1 pack.
		h = t3d_composed_surface_cold_hash_integer( h, i );
		const auto& layer = r.m_pAuthoredLayers[i];
		for ( float v : layer.m_fColor )
			h = t3d_composed_surface_cold_hash_float( h, v );
		for ( float v : layer.m_fPbr0 )
			h = t3d_composed_surface_cold_hash_float( h, v );
		for ( float v : layer.m_fPbr1 )
			h = t3d_composed_surface_cold_hash_float( h, v );
	}
	const auto& s = r.m_source;
	h = t3d_composed_surface_cold_hash_integer( h, s.m_nSamplingMode );
	h = t3d_composed_surface_cold_hash_integer( h, s.m_nVtFiltering );
	h = t3d_composed_surface_cold_hash_float( h, s.m_fStaticMipBias );
	h = t3d_composed_surface_cold_hash_float( h, s.m_fBlendSharpness );
	h = t3d_composed_surface_cold_hash_integer( h, s.m_nDetilingMode );
	h = t3d_composed_surface_cold_hash_float( h, s.m_fAutoSlope );
	h = t3d_composed_surface_cold_hash_float( h, s.m_fAutoHeightReduction );
	h = t3d_composed_surface_cold_hash_integer( h, s.m_nAutoBaseTexture );
	h = t3d_composed_surface_cold_hash_integer( h, s.m_nAutoOverlayTexture );
	// Disallowed modes are still hashed so future eligibility expansions cannot
	// accidentally reuse an earlier composed result with different semantics.
	const bool flags[] = { s.m_bAutoShader,       s.m_bDualScale,        s.m_bSideProjection,      s.m_bDetiling,
	                       s.m_bAdaptiveSampling, s.m_bImplicitControls, s.m_bViewDependentMipBias };
	for ( bool enabled : flags )
		h = t3d_composed_surface_cold_hash_integer( h, enabled ? 1u : 0u );
	return h ? h : 1u;
}

static bool t3d_composed_surface_cold_current( const t3dComposedSurfaceColdCookRequest_t& r )
{
	return r.m_pCurrent && r.m_pCurrent( r.m_pCurrentUser, r.m_nTerrainRevision, r.m_nMaterialRevision, r.m_nTopologyGeneration );
}

bool t3d_composed_surface_cook_source_pack_cold( const t3dComposedSurfaceColdCookRequest_t& r, t3dComposedSurfaceColdCookResult_t& result )
{
	result = {};
	result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_INVALID_INPUT;
	uint64_t expected_bytes = 0u;
	if ( !r.m_pPages || !r.m_nPageCount || !t3d_comp_surf_cook_total_bytes( r.m_nPageCount, expected_bytes ) || !r.m_pResolvedLayers ||
	     r.m_nResolvedLayerCapacity < r.m_nLayerCount || !r.m_pDecode || !r.m_pDecodedCache || r.m_nCacheSlots == 0u ||
	     r.m_nCacheSlots > T3D_COMPOSED_SURFACE_PACK_CACHE_MAX_SLOTS || !r.m_pEncode || !r.m_pCookScratch ||
	     r.m_nCookScratchBytes < T3D_COMP_SURF_COOK_SCRATCH_BYTES || !r.m_pOutput || r.m_nOutputCapacity < expected_bytes ||
	     !r.m_source.m_pResolvePage || !r.m_source.m_pReadControl || ( r.m_pPreviousSidecar == r.m_pOutput ) ||
	     ( !r.m_pPreviousSidecar && r.m_nPreviousSidecarBytes ) ||
	     !( r.m_source.m_bCanonicalMaterialParityAccepted || r.m_bReferenceOnly ) ||
	     ( r.m_bPageLocalCompositionIdentity && !r.m_bReferenceOnly ) ||
	     ( r.m_bReferenceOnly &&
	       ( !r.m_nControlPayloadFingerprint || ( r.m_pDestination && !strstr( r.m_pDestination, ".candidate" ) ) ) ) )
		return false;

	// The cooker clears the output before invoking source sampling. Reject
	// overlapping backing spans rather than corrupting a mapped .t3sp or
	// previously validated .t3sc during an incremental rewrite.
	const uint64_t cache_bytes = uint64_t( r.m_nCacheSlots ) * T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES;
	if ( !t3d_composed_surface_cold_disjoint( r.m_pOutput, expected_bytes, r.m_pSourcePackage, r.m_nSourcePackageBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pOutput, expected_bytes, r.m_pPreviousSidecar, r.m_nPreviousSidecarBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pOutput, expected_bytes, r.m_pDecodedCache, cache_bytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pOutput, expected_bytes, r.m_pCookScratch, r.m_nCookScratchBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pDecodedCache, cache_bytes, r.m_pSourcePackage, r.m_nSourcePackageBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pCookScratch, r.m_nCookScratchBytes, r.m_pSourcePackage, r.m_nSourcePackageBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pCookScratch, r.m_nCookScratchBytes, r.m_pPreviousSidecar, r.m_nPreviousSidecarBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pDecodedCache, cache_bytes, r.m_pPreviousSidecar, r.m_nPreviousSidecarBytes ) ||
	     !t3d_composed_surface_cold_disjoint( r.m_pCookScratch, r.m_nCookScratchBytes, r.m_pDecodedCache, cache_bytes ) )
		return false;

	const uint64_t fingerprint = t3d_composed_surface_source_fingerprint_cold( r );
	if ( !fingerprint )
		return false;
	result.m_nSourceFingerprint = fingerprint;
	if ( !t3d_composed_surface_cold_current( r ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_STALE_REVISION;
		return false;
	}

	// The pack view is owned by this stack frame. The sampler points to it,
	// and the bridge points to the sampler, so all stay alive during cooking.
	t3dVTPackBaselineView_t view{};
	t3dComposedSurfacePackSampler_t sampler{};
	if ( !t3d_composed_surface_pack_open_source_pack( r.m_pSourcePackage, r.m_nSourcePackageBytes, view, sampler, r.m_pDecode,
	                                                  r.m_pDecoderUser, r.m_pDecodedCache, r.m_nCacheSlots ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_INVALID_SOURCE_PACK;
		return false;
	}
	for ( uint32_t i = 0u; i < r.m_nLayerCount; ++i ) {
		uint32_t allocation = UINT32_MAX;
		if ( !t3d_composed_surface_pack_find_material_allocation( view, i, allocation ) ) {
			result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_MISSING_MATERIAL;
			return false;
		}
		r.m_pResolvedLayers[i] = r.m_pAuthoredLayers[i];
		r.m_pResolvedLayers[i].m_nAllocation = allocation;
	}
	t3dComposedStaticCookAdapter_t adapter{};
	adapter.m_source = r.m_source;
	adapter.m_source.m_bReferenceOnly = r.m_bReferenceOnly;
	adapter.m_source.m_pLayers = r.m_pResolvedLayers;
	adapter.m_source.m_nLayerCount = r.m_nLayerCount;
	adapter.m_pEncoder = r.m_pEncode;
	adapter.m_pEncoderUser = r.m_pEncoderUser;
	t3dComposedSurfacePackProducerBridge_t bridge{};
	if ( !t3d_composed_surface_pack_bind_static_source( sampler, bridge, adapter.m_source ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_INVALID_INPUT;
		return false;
	}
	t3dComposedSurfaceCookSource_t cook{};
	if ( !t3d_comp_surf_static_baseline_make_cook_source( adapter, fingerprint, cook ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_INVALID_INPUT;
		return false;
	}
	uint64_t written = 0u;
	const bool composed = r.m_pPreviousSidecar
	                          ? t3d_comp_surf_recook_incremental_cold( r.m_pPages, r.m_nPageCount, cook, r.m_pPreviousSidecar,
	                                                                   r.m_nPreviousSidecarBytes, r.m_pDirtyFlags, r.m_pCookScratch,
	                                                                   r.m_nCookScratchBytes, r.m_pOutput, r.m_nOutputCapacity, written )
	                          : t3d_comp_surf_cook_pages_cold( r.m_pPages, r.m_nPageCount, cook, r.m_pCookScratch, r.m_nCookScratchBytes,
	                                                           r.m_pOutput, r.m_nOutputCapacity, written );
	result.m_nDecodedTiles = sampler.m_nDecodeCalls;
	result.m_nCacheHits = sampler.m_nCacheHits;
	if ( !composed ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_COMPOSE_FAILURE;
		return false;
	}
	if ( !t3d_composed_surface_cold_current( r ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_STALE_REVISION;
		return false;
	}
	if ( r.m_pDestination && !t3d_comp_surf_write_sidecar_cold( r.m_pDestination, r.m_pOutput, written ) ) {
		result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_PUBLICATION_FAILURE;
		return false;
	}
	result.m_nWrittenBytes = written;
	result.m_status = T3D_COMPOSED_SURFACE_COLD_COOK_SUCCESS;
	return true;
}
