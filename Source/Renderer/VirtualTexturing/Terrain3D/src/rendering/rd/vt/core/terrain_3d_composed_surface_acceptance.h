/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

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
