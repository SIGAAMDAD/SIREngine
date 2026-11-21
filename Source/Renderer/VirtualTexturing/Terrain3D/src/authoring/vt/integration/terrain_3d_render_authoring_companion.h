// Editor-only owner for long-lived terrain VT cooking and export state.
#ifndef TERRAIN3D_RENDER_AUTHORING_COMPANION_H
#define TERRAIN3D_RENDER_AUTHORING_COMPANION_H
#include "authoring/vt/build/terrain_3d_incremental_megavt_cooker.h"
#include "authoring/vt/integration/terrain_3d_composed_surface_editor_source.h"
class Terrain3DRenderAuthoringCompanion
{
public:
	Terrain3DIncrementalMegaVTCooker m_cooker;
	uint64_t m_nForceRebakeRequestSerial = 0u;
	uint64_t m_nForceRebakeActiveSerial = 0u;
	uint64_t m_nForceRebakeCompletionSerial = 0u;
	bool m_bForceRebakeQueued = false;
	bool m_bForceRebakeAll = true;
	bool m_bForceRebakeActive = false;
	bool m_bForceMaterialsStaged = false;
	String m_forceRebakeState = "idle";
	String m_forceRebakeError;
	String m_forceRebakeOutput;
	uint32_t m_nForceRegionCount = 0u;
	uint32_t m_nForceShippingGeneration = 0u;
	// Explicit VT bake follow-up: candidate-only static composed material cook.
	// No RD allocations; it cannot change the active raster material source.
	// Only a successful CS14 canonical parity transaction may populate this
	// fragment. It is composited into V1 with the regular source/region packs.
	PackedByteArray m_approvedComposedV1Fragment;
	uint64_t m_nApprovedComposedFingerprint = 0u;
	uint64_t m_nApprovedComposedSourceGeneration = 0u;
	uint64_t m_nApprovedComposedTopologyGeneration = 0u;
	uint64_t m_nApprovedComposedEditStamp = 0u;
	String m_approvedComposedStatus = "none";
	PackedByteArray m_composedCandidateSourcePack;
	t3dCompSurfEditorCandidateResult_t m_composedCandidate;
	bool m_bComposedCandidatePending = false;
	String m_composedCandidateState = "idle";
	uint64_t m_nComposedCandidateRequestSerial = 0u;
	uint64_t m_nComposedCandidateCompletionSerial = 0u;
	uint64_t m_nComposedCandidateEditStamp = 0u;
};
#endif // TERRAIN3D_RENDER_AUTHORING_COMPANION_H
