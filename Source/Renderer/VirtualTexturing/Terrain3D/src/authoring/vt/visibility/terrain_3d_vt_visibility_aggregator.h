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

#ifndef TERRAIN3D_VT_VISIBILITY_AGGREGATOR_H
#define TERRAIN3D_VT_VISIBILITY_AGGREGATOR_H

#include "authoring/vt/visibility/terrain_3d_vt_visibility_baker.h"

struct t3dVTVisibilityAggregateStats_t
{
	uint32_t m_nCells = 0u;
	uint32_t m_nProbes = 0u;
	uint32_t m_nPartialCells = 0u;
	uint32_t m_nDroppedRecords = 0u;
	uint32_t m_nRecords = 0u;
	uint64_t m_nVisibleSamples = 0u;
	uint64_t m_szMipSamples[T3D_VT_DEMAND_BAKED_VISIBILITY_MIP_BINS]{};
};

class Terrain3DVTVisibilityAggregator
{
public:
	// Cold-only, deterministic. Per-bin MAX across camera probes retains the
	// union of visible mip urgency; zero bins do not fabricate mip demand.
	// A bound is imposed per cell, and every truncated cell is PARTIAL.
	static bool reduce( const t3dVTVisibilityProbeResult_t *pProbes, uint32_t nProbes, const uint64_t *pSortedStableKeys,
	                    uint32_t nMaterials, uint32_t nPerCellLimit, bool bForcedPartial, std::vector<t3dVTDemandVisibilityCell_t>& rCells,
	                    std::vector<t3dVTDemandBakedVisibilityRecord_t>& rRecords, t3dVTVisibilityAggregateStats_t& rStats );
};

#endif
