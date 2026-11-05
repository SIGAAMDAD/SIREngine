/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Mesh/SIRVGFramePlan.h>

#include <stdint.h>

static bool sameVGMeshKey( const sirMeshKey_t& a, const sirMeshKey_t& b ) noexcept
{
	return a.nAssetId == b.nAssetId && a.nRevision == b.nRevision && a.nVariant == b.nVariant && a.nReserved == b.nReserved;
}

sirVGPlanResult_t sirVGValidateFramePlan( const sirVGFramePlan_t& plan ) noexcept
{
	if ( plan.nVersion != SIR_VG_FRAME_PLAN_VERSION ) {
		return SIR_VG_PLAN_UNSUPPORTED_VERSION;
	}
	if ( plan.nFrameId == 0 || plan.nPageSnapshotGeneration == 0 || plan.nReserved != 0 ||
	     ( plan.nGroupCount == 0 ) != ( plan.nRangeCount == 0 ) || ( plan.nGroupCount != 0 && plan.pGroups == nullptr ) ||
	     ( plan.nRangeCount != 0 && plan.pRanges == nullptr ) || ( plan.nDemandCount != 0 && plan.pDemands == nullptr ) ) {
		return SIR_VG_PLAN_INVALID_ARGUMENT;
	}

	uint32_t nNextRange = 0;
	for ( uint32_t iGroup = 0; iGroup < plan.nGroupCount; ++iGroup ) {
		const sirVGSelectedGroup_t& group = plan.pGroups[iGroup];
		if ( group.keyMesh.nAssetId == 0 || group.keyMesh.nRevision == 0 || group.keyMesh.nReserved != 0 || group.nInstanceId == 0 ||
		     group.nGroupId == 0 || group.nPageId == 0 || group.nExpectedClusterCount == 0 ) {
			return SIR_VG_PLAN_INVALID_ARGUMENT;
		}
		if ( group.nPageGeneration != plan.nPageSnapshotGeneration ) {
			return SIR_VG_PLAN_STALE_SNAPSHOT;
		}
		if ( group.nExpectedClusterCount != group.nSelectedClusterCount || group.nFirstRange != nNextRange || group.nRangeCount == 0 ||
		     group.nRangeCount > plan.nRangeCount - nNextRange ) {
			return SIR_VG_PLAN_INCOMPLETE_GROUP;
		}

		for ( uint32_t iRange = nNextRange; iRange < nNextRange + group.nRangeCount; ++iRange ) {
			const sirVGDrawRange_t& range = plan.pRanges[iRange];
			if ( range.nPageGeneration != plan.nPageSnapshotGeneration ) {
				return SIR_VG_PLAN_STALE_SNAPSHOT;
			}
			if ( !sameVGMeshKey( group.keyMesh, range.keyMesh ) || range.nInstanceId != group.nInstanceId ||
			     range.nGroupId != group.nGroupId || range.nPageId != group.nPageId ) {
				return SIR_VG_PLAN_INCOMPLETE_GROUP;
			}
			if ( range.nIndexCount == 0 || range.nIndexCount % 3U != 0 || range.nInstanceCount == 0 ) {
				return SIR_VG_PLAN_INVALID_ARGUMENT;
			}
			if ( range.nFirstIndex > UINT32_MAX - range.nIndexCount || range.nFirstInstance > UINT32_MAX - range.nInstanceCount ) {
				return SIR_VG_PLAN_OVERFLOW;
			}
		}
		nNextRange += group.nRangeCount;
	}
	if ( nNextRange != plan.nRangeCount ) {
		return SIR_VG_PLAN_INCOMPLETE_GROUP;
	}

	uint64_t nPreviousPage = 0;
	for ( uint32_t iDemand = 0; iDemand < plan.nDemandCount; ++iDemand ) {
		const sirVGPageDemand_t& demand = plan.pDemands[iDemand];
		if ( demand.nPageId == 0 || demand.nRequestEpoch == 0 ) {
			return SIR_VG_PLAN_INVALID_ARGUMENT;
		}
		for ( uint32_t iReserved = 0; iReserved < sizeof( demand.szReserved ); ++iReserved ) {
			if ( demand.szReserved[iReserved] != 0 ) {
				return SIR_VG_PLAN_INVALID_ARGUMENT;
			}
		}
		if ( demand.nManifestGeneration != plan.nPageSnapshotGeneration ) {
			return SIR_VG_PLAN_STALE_SNAPSHOT;
		}
		if ( demand.nPageId <= nPreviousPage ) {
			return SIR_VG_PLAN_DUPLICATE_DEMAND;
		}
		nPreviousPage = demand.nPageId;
	}
	return SIR_VG_PLAN_OK;
}
