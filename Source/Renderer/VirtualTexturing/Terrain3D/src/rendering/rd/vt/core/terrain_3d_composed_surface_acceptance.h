// CS08 cold release gate for retirement of the legacy middle-composite atlas.
#ifndef TERRAIN3D_COMPOSED_SURFACE_ACCEPTANCE_H
#define TERRAIN3D_COMPOSED_SURFACE_ACCEPTANCE_H

struct t3dComposedSurfaceRetirementEvidence_t
{
	bool m_bCanonicalSurfaceParity = false;
	bool m_bEditorDirtyRecookPasses = false;
	bool m_bMiddleCoverageComplete = false;
	bool m_bDepthDnrColorCoherent = false;
	bool m_bLowBudgetFallbackPasses = false;
	bool m_bVulkanPerformanceAcceptance = false;
	bool m_bStableResourcesTenThousandFrames = false;
};

// The caller can release the old atlas only when independently established
// evidence is true for every prerequisite. No producer sets these flags in
// the current shipping/runtime build; the compatibility atlas remains live.
inline bool t3d_comp_surf_may_retire_legacy_middle_atlas( const t3dComposedSurfaceRetirementEvidence_t& e )
{
	return e.m_bCanonicalSurfaceParity && e.m_bEditorDirtyRecookPasses && e.m_bMiddleCoverageComplete && e.m_bDepthDnrColorCoherent &&
	       e.m_bLowBudgetFallbackPasses && e.m_bVulkanPerformanceAcceptance && e.m_bStableResourcesTenThousandFrames;
}
#endif
