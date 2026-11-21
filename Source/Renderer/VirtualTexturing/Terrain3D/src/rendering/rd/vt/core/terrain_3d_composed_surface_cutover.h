// BR15: fail-closed, source-authoritative composed material cutover policy.
// Not a mutable GPU ABI. Evaluated at a cold publication boundary only.
#ifndef TERRAIN3D_COMPOSED_SURFACE_CUTOVER_H
#define TERRAIN3D_COMPOSED_SURFACE_CUTOVER_H
#include <stdint.h>
#include "rendering/rd/vt/core/terrain_3d_composed_surface_contract.h"

struct t3dComposedSurfaceCutoverProof_t
{
	uint64_t m_nSourceFingerprint = 0u;
	uint64_t m_nControlsRevision = 0u;
	uint64_t m_nMaterialRevision = 0u;
	uint64_t m_nTopologyRevision = 0u;
	uint64_t m_nFilterIdentity = 0u;
	uint32_t m_nDeviceGeneration = 0u;
	bool m_bStaticMaterialParity = false;
	bool m_bDnrParity = false;
	bool m_bAlphaParity = false;
	bool m_bNoPendingEdits = false;
	// An authoring pipeline that bakes static decals must explicitly
	// prove decal-source revision and parity. Defaults preserve terrain-only pages.
	uint64_t m_nStaticDecalRevision = 0u;
	bool m_bStaticDecalsBaked = false;
	bool m_bStaticDecalParity = false;
};

inline bool t3d_comp_surf_cutover_proof_current( const t3dComposedSurfaceCutoverProof_t& rAccepted,
                                                 const t3dComposedSurfaceCutoverProof_t& rCurrent, const uint64_t nSidecarSourceFingerprint,
                                                 const uint32_t nDeviceGeneration )
{
	// Decal-containing cache pages require a matching immutable decal revision
	// and a separately accepted result. A terrain-only page retains old rules.
	if ( rAccepted.m_bStaticDecalsBaked != rCurrent.m_bStaticDecalsBaked )
		return false;
	if ( rAccepted.m_bStaticDecalsBaked &&
	     ( !rAccepted.m_bStaticDecalParity || !rCurrent.m_bStaticDecalParity || !rAccepted.m_nStaticDecalRevision ||
	       rAccepted.m_nStaticDecalRevision != rCurrent.m_nStaticDecalRevision ) )
		return false;
	if ( !nSidecarSourceFingerprint || !nDeviceGeneration || rAccepted.m_nSourceFingerprint != nSidecarSourceFingerprint ||
	     rCurrent.m_nSourceFingerprint != nSidecarSourceFingerprint || !rCurrent.m_bStaticMaterialParity || !rCurrent.m_bDnrParity ||
	     !rCurrent.m_bAlphaParity || !rCurrent.m_bNoPendingEdits || !rCurrent.m_nControlsRevision || !rCurrent.m_nMaterialRevision ||
	     !rCurrent.m_nTopologyRevision || !rCurrent.m_nFilterIdentity || rAccepted.m_nDeviceGeneration != nDeviceGeneration ||
	     rCurrent.m_nDeviceGeneration != nDeviceGeneration )
		return false;
	return rAccepted.m_nControlsRevision == rCurrent.m_nControlsRevision && rAccepted.m_nMaterialRevision == rCurrent.m_nMaterialRevision &&
	       rAccepted.m_nTopologyRevision == rCurrent.m_nTopologyRevision && rAccepted.m_nFilterIdentity == rCurrent.m_nFilterIdentity &&
	       rAccepted.m_bStaticMaterialParity && rAccepted.m_bDnrParity && rAccepted.m_bAlphaParity && rAccepted.m_bNoPendingEdits;
}
#endif // TERRAIN3D_COMPOSED_SURFACE_CUTOVER_H
