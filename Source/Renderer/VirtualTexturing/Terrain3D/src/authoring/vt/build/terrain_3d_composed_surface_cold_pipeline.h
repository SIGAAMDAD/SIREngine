// CS13: cold, bounded source-pack-to-composed-sidecar pipeline.
// This is an editor/cook entrypoint, never called from a render-thread frame.
#ifndef TERRAIN3D_COMPOSED_SURFACE_V1_COLD_PIPELINE_H
#define TERRAIN3D_COMPOSED_SURFACE_V1_COLD_PIPELINE_H

#include "authoring/vt/build/terrain_3d_composed_surface_pack_sampler.h"

// Called before opening source bytes, after composition, and before commit.
// The editor owner must serialize final publication with edits; this predicate
// does not acquire a lock or acknowledge the edit journal on its behalf.
typedef bool ( *t3dComposedSurfaceColdCookCurrentFn )( void *pUser, uint64_t nTerrainRevision, uint64_t nMaterialRevision,
                                                       uint64_t nTopologyGeneration );

enum t3dComposedSurfaceColdCookStatus_t
{
	T3D_COMPOSED_SURFACE_COLD_COOK_SUCCESS = 0,
	T3D_COMPOSED_SURFACE_COLD_COOK_INVALID_INPUT,
	T3D_COMPOSED_SURFACE_COLD_COOK_STALE_REVISION,
	T3D_COMPOSED_SURFACE_COLD_COOK_INVALID_SOURCE_PACK,
	T3D_COMPOSED_SURFACE_COLD_COOK_MISSING_MATERIAL,
	T3D_COMPOSED_SURFACE_COLD_COOK_COMPOSE_FAILURE,
	T3D_COMPOSED_SURFACE_COLD_COOK_PUBLICATION_FAILURE
};

struct t3dComposedSurfaceColdCookRequest_t
{
	// The complete, mapped .t3sp file must outlive this call. No V1 ABI edits.
	const void *m_pSourcePackage;
	uint64_t m_nSourcePackageBytes;
	const t3dComposedSurfaceCookPage_t *m_pPages;
	uint32_t m_nPageCount;
	// Authored layers are indexed by logical terrain material ID (0..count-1).
	// Allocation IDs in these records are ignored and resolved from V1 keys.
	const t3dComposedStaticLayer_t *m_pAuthoredLayers;
	uint32_t m_nLayerCount;
	t3dComposedStaticLayer_t *m_pResolvedLayers;
	uint32_t m_nResolvedLayerCapacity;
	t3dComposedStaticSource_t m_source;
	t3dComposedSurfaceEncodeFn m_pEncode;
	void *m_pEncoderUser;
	t3dComposedSurfacePackDecodeFn m_pDecode;
	void *m_pDecoderUser;
	uint8_t *m_pDecodedCache;
	uint32_t m_nCacheSlots;
	uint8_t *m_pCookScratch;
	uint32_t m_nCookScratchBytes;
	uint8_t *m_pOutput;
	uint64_t m_nOutputCapacity;
	const uint8_t *m_pPreviousSidecar;
	uint64_t m_nPreviousSidecarBytes;
	const uint8_t *m_pDirtyFlags; // indexed 1:1 with sorted m_pPages
	// Caller-owned revision tokens; include all external control/geometry edits.
	uint64_t m_nTerrainRevision;
	uint64_t m_nMaterialRevision;
	uint64_t m_nTopologyGeneration;
	t3dComposedSurfaceColdCookCurrentFn m_pCurrent;
	void *m_pCurrentUser;
	// Diagnostic candidate only; no canonical material/DNR/alpha parity proof.
	bool m_bReferenceOnly = false;
	// Include actual control-map content in the immutable source identity.
	uint64_t m_nControlPayloadFingerprint = 0u;
	// Candidate-only incremental identity: control hashes belong to the
	// individual page composition revisions instead of the whole sidecar.
	// All live/parity-accepted cooks continue to hash global revisions.
	bool m_bPageLocalCompositionIdentity = false;
	// Optional cold atomic publication after all gates. Null = memory only.
	const char *m_pDestination;
};

struct t3dComposedSurfaceColdCookResult_t
{
	t3dComposedSurfaceColdCookStatus_t m_status;
	uint64_t m_nSourceFingerprint;
	uint64_t m_nWrittenBytes;
	uint32_t m_nDecodedTiles;
	uint32_t m_nCacheHits;
};

// Fingerprint the immutable source pack, material parameters and external
// authoring generations. Returns zero for invalid requests. No heap allocation.
uint64_t t3d_composed_surface_source_fingerprint_cold( const t3dComposedSurfaceColdCookRequest_t& rRequest );

// Does not enable parity itself. Fails before touching output when eligibility,
// callbacks, source pack, material keys or the revision check are invalid.
bool t3d_composed_surface_cook_source_pack_cold( const t3dComposedSurfaceColdCookRequest_t& rRequest,
                                                 t3dComposedSurfaceColdCookResult_t& rResult );

#endif
