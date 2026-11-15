/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Mesh/SIRVGFrameCompiler.h>

#include <algorithm>
#include <math.h>
#include <stdint.h>

static bool sirVGSameKey( const sirMeshKey_t& a, const sirMeshKey_t& b ) noexcept
{
	return a.nAssetId == b.nAssetId && a.nRevision == b.nRevision && a.nVariant == b.nVariant && a.nReserved == b.nReserved;
}

static bool sirVGValidView( const sirVGFrameView_t& view ) noexcept
{
	if ( view.nRequestEpoch == 0 || view.nFrustumPlaneCount > 6 || !isfinite( view.fProjectionPixelsPerUnit ) ||
	     !isfinite( view.fAllowedErrorPixels ) || !isfinite( view.fNearDistance ) || view.fProjectionPixelsPerUnit <= 0.0f ||
	     view.fAllowedErrorPixels <= 0.0f || view.fNearDistance <= 0.0f )
		return false;
	for ( uint32_t axis = 0; axis < 3; ++axis ) {
		if ( !isfinite( view.szCameraXYZ[axis] ) )
			return false;
	}
	for ( uint32_t i = 0; i < view.nFrustumPlaneCount; ++i ) {
		float fNormalLengthSquared = 0.0f;
		for ( uint32_t axis = 0; axis < 4; ++axis ) {
			if ( !isfinite( view.szFrustumPlanes[i][axis] ) )
				return false;
			if ( axis < 3 )
				fNormalLengthSquared += view.szFrustumPlanes[i][axis] * view.szFrustumPlanes[i][axis];
		}
		if ( !isfinite( fNormalLengthSquared ) || fNormalLengthSquared <= 0.0f )
			return false;
	}
	return true;
}

static bool sirVGVisibleInstance( const sirVGFrameInstance_t& instance, const sirVGFrameView_t& view ) noexcept
{
	for ( uint32_t i = 0; i < view.nFrustumPlaneCount; ++i ) {
		const float *pPlane = view.szFrustumPlanes[i];
		const float fSignedDistance =
		    pPlane[0] * instance.szCenterXYZ[0] + pPlane[1] * instance.szCenterXYZ[1] + pPlane[2] * instance.szCenterXYZ[2] + pPlane[3];
		const float fNormalLength = sqrtf( pPlane[0] * pPlane[0] + pPlane[1] * pPlane[1] + pPlane[2] * pPlane[2] );
		if ( fSignedDistance < -instance.fBoundingRadius * fNormalLength )
			return false;
	}
	return true;
}

static bool sirVGValidInstance( const sirVGFrameInstance_t& instance, const sirMeshKey_t& key ) noexcept
{
	if ( !sirVGSameKey( instance.keyMesh, key ) || !isfinite( instance.fBoundingRadius ) || !isfinite( instance.fWorldScale ) ||
	     instance.fBoundingRadius < 0.0f || instance.fWorldScale <= 0.0f )
		return false;
	for ( uint32_t axis = 0; axis < 3; ++axis ) {
		if ( !isfinite( instance.szCenterXYZ[axis] ) )
			return false;
	}
	return true;
}

static float sirVGProjectedErrorScale( const sirVGFrameInstance_t& instance, const sirVGFrameView_t& view ) noexcept
{
	float fDistanceSquared = 0.0f;
	for ( uint32_t axis = 0; axis < 3; ++axis ) {
		const float fDelta = instance.szCenterXYZ[axis] - view.szCameraXYZ[axis];
		fDistanceSquared += fDelta * fDelta;
	}
	if ( !isfinite( fDistanceSquared ) )
		return INFINITY;
	float fDistance = sqrtf( fDistanceSquared ) - instance.fBoundingRadius;
	if ( fDistance < view.fNearDistance )
		fDistance = view.fNearDistance;
	return view.fProjectionPixelsPerUnit * instance.fWorldScale / fDistance;
}

static sirVGPlanResult_t sirVGRecordDemand( const sirVGFrameCompileInput_t& input, const sirVGFrameCompileStorage_t& storage,
                                            sirVGFramePlan_t *pPlan, uint32_t nPage ) noexcept
{
	const uint64_t nPageId = ( uint64_t )nPage + 1;
	for ( uint32_t i = 0; i < pPlan->nDemandCount; ++i ) {
		if ( storage.pDemands[i].nPageId == nPageId )
			return SIR_VG_PLAN_OK;
	}
	if ( pPlan->nDemandCount >= storage.nDemandCapacity )
		return SIR_VG_PLAN_OVERFLOW;
	storage.pDemands[pPlan->nDemandCount++] = {
	    nPageId, pPlan->nPageSnapshotGeneration, input.view.nRequestEpoch, input.view.nDeadlineTick, input.view.nDemandPriority, {} };
	return SIR_VG_PLAN_OK;
}

static sirVGPlanResult_t sirVGCanRefine( const sirVGFrameCompileInput_t& input, const sirVGFrameCompileStorage_t& storage,
                                         sirVGFramePlan_t *pPlan, const sirMeshPackGroup_t& parent, bool *pReady ) noexcept
{
	*pReady = true;
	for ( uint32_t child = parent.nFirstChild; child < parent.nFirstChild + parent.nChildCount; ++child ) {
		sirMeshPackGroup_t group = {};
		if ( input.pSchema->groupAt( child, &group ) != SIR_MESH_PACK_OK )
			return SIR_VG_PLAN_INVALID_ARGUMENT;
		if ( input.pDirectory->isResident( group.nPage ) )
			continue;
		*pReady = false;
		const sirVGPlanResult_t result = sirVGRecordDemand( input, storage, pPlan, group.nPage );
		if ( result != SIR_VG_PLAN_OK )
			return result;
	}
	return SIR_VG_PLAN_OK;
}

static sirVGPlanResult_t sirVGEmitGroup( const sirVGFrameCompileInput_t& input, const sirVGFrameCompileStorage_t& storage,
                                         sirVGFramePlan_t *pPlan, const sirVGFrameInstance_t& instance, uint32_t nGroup ) noexcept
{
	sirMeshPackGroup_t group = {};
	if ( input.pSchema->groupAt( nGroup, &group ) != SIR_MESH_PACK_OK || !input.pDirectory->isResident( group.nPage ) )
		return SIR_VG_PLAN_STALE_SNAPSHOT;
	if ( pPlan->nGroupCount >= storage.nGroupCapacity || group.nClusterCount > storage.nRangeCapacity - pPlan->nRangeCount )
		return SIR_VG_PLAN_OVERFLOW;
	const uint32_t nSnapshot = pPlan->nPageSnapshotGeneration;
	const uint64_t nPageId = ( uint64_t )group.nPage + 1;
	const uint32_t nFirstRange = pPlan->nRangeCount;
	for ( uint32_t i = 0; i < group.nClusterCount; ++i ) {
		sirVGClusterDrawLayout_t layout = {};
		const uint32_t nCluster = group.nFirstCluster + i;
		if ( input.pDirectory->getClusterLayout( nCluster, &layout ) != SIR_VG_RESIDENCY_OK || layout.nIndexCount == 0 ||
		     layout.nFirstIndex > UINT32_MAX - layout.nIndexCount )
			return SIR_VG_PLAN_STALE_SNAPSHOT;
		if ( layout.nFirstVertex > INT32_MAX || layout.nMaterialId == UINT32_MAX )
			return SIR_VG_PLAN_OVERFLOW;
		sirVGDrawRange_t& range = storage.pRanges[pPlan->nRangeCount++];
		range = { instance.keyMesh,
		          instance.nInstanceId,
		          nGroup,
		          layout.nMaterialId,
		          ( ( uint64_t )layout.nMaterialId << 32 ) | nPageId,
		          nPageId,
		          nSnapshot,
		          input.view.nPassId,
		          layout.nFirstIndex,
		          layout.nIndexCount,
		          ( int32_t )layout.nFirstVertex,
		          instance.nFirstInstance,
		          1 };
	}
	storage.pGroups[pPlan->nGroupCount++] = {
	    instance.keyMesh,    instance.nInstanceId, nGroup, nPageId, nSnapshot, nFirstRange, group.nClusterCount,
	    group.nClusterCount, group.nClusterCount };
	return SIR_VG_PLAN_OK;
}

static sirVGPlanResult_t sirVGCompileInstance( const sirVGFrameCompileInput_t& input, const sirVGFrameCompileStorage_t& storage,
                                               sirVGFramePlan_t *pPlan, const sirVGFrameInstance_t& instance ) noexcept
{
	if ( !sirVGValidInstance( instance, input.pSchema->getKey() ) )
		return SIR_VG_PLAN_INVALID_ARGUMENT;
	if ( !sirVGVisibleInstance( instance, input.view ) )
		return SIR_VG_PLAN_OK;
	const float fProjectedScale = sirVGProjectedErrorScale( instance, input.view );
	if ( !isfinite( fProjectedScale ) || storage.nStackCapacity == 0 )
		return SIR_VG_PLAN_OVERFLOW;
	uint32_t nStackCount = 1;
	storage.pGroupStack[0] = 0;
	while ( nStackCount != 0 ) {
		const uint32_t nGroup = storage.pGroupStack[--nStackCount];
		sirMeshPackGroup_t group = {};
		if ( input.pSchema->groupAt( nGroup, &group ) != SIR_MESH_PACK_OK )
			return SIR_VG_PLAN_INVALID_ARGUMENT;
		const bool bNeedsRefine = group.nChildCount != 0 && group.fError * fProjectedScale > input.view.fAllowedErrorPixels;
		if ( bNeedsRefine && group.nChildCount <= storage.nStackCapacity - nStackCount ) {
			bool bReady = false;
			const sirVGPlanResult_t readyResult = sirVGCanRefine( input, storage, pPlan, group, &bReady );
			if ( readyResult != SIR_VG_PLAN_OK )
				return readyResult;
			if ( bReady ) {
				for ( uint32_t i = group.nChildCount; i != 0; --i )
					storage.pGroupStack[nStackCount++] = group.nFirstChild + i - 1;
				continue;
			}
		}
		const sirVGPlanResult_t emitResult = sirVGEmitGroup( input, storage, pPlan, instance, nGroup );
		if ( emitResult != SIR_VG_PLAN_OK )
			return emitResult;
	}
	return SIR_VG_PLAN_OK;
}

struct sirVGSortPageDemand_s
{
	bool operator()( const sirVGPageDemand_t& a, const sirVGPageDemand_t& b ) const noexcept
	{
		return a.nPageId < b.nPageId;
	}
};

struct sirVGSortDrawRange_s
{
	const sirVGDrawRange_t *pRanges;
	bool operator()( uint32_t a, uint32_t b ) const noexcept
	{
		const sirVGDrawRange_t& x = pRanges[a];
		const sirVGDrawRange_t& y = pRanges[b];
		if ( x.nPassId != y.nPassId )
			return x.nPassId < y.nPassId;
		if ( x.nSortKey != y.nSortKey )
			return x.nSortKey < y.nSortKey;
		if ( x.nInstanceId != y.nInstanceId )
			return x.nInstanceId < y.nInstanceId;
		return a < b;
	}
};

sirVGPlanResult_t SIRVGFrameCompiler::compile( const sirVGFrameCompileInput_t& input, const sirVGFrameCompileStorage_t& storage,
                                               sirVGFramePlan_t *pPlan ) noexcept
{
	if ( pPlan == nullptr || input.pSchema == nullptr || input.pDirectory == nullptr || !input.pSchema->isOpen() ||
	     ( input.nInstanceCount != 0 && input.pInstances == nullptr ) || !sirVGValidView( input.view ) || storage.pGroups == nullptr ||
	     storage.pRanges == nullptr || storage.pDemands == nullptr || storage.pGroupStack == nullptr ||
	     storage.pSortedDrawOrder == nullptr || storage.nGroupCapacity == 0 || storage.nRangeCapacity == 0 || storage.nStackCapacity == 0 ||
	     storage.nDrawOrderCapacity < storage.nRangeCapacity )
		return SIR_VG_PLAN_INVALID_ARGUMENT;
	if ( input.view.nRequestEpoch != input.pDirectory->getRequestEpoch() )
		return SIR_VG_PLAN_STALE_SNAPSHOT;
	if ( !input.pDirectory->isResident( 0 ) )
		return SIR_VG_PLAN_STALE_SNAPSHOT;
	const uint32_t nSnapshot = input.pDirectory->getSnapshotGeneration();
	sirVGFramePlan_t result = {
	    storage.pGroups, storage.pRanges, storage.pDemands, input.view.nFrameId, SIR_VG_FRAME_PLAN_VERSION, nSnapshot, 0, 0, 0, 0 };
	for ( uint32_t i = 0; i < input.nInstanceCount; ++i ) {
		const sirVGPlanResult_t instanceResult = sirVGCompileInstance( input, storage, &result, input.pInstances[i] );
		if ( instanceResult != SIR_VG_PLAN_OK )
			return instanceResult;
	}
	std::sort( storage.pDemands, storage.pDemands + result.nDemandCount, sirVGSortPageDemand_s{} );
	for ( uint32_t i = 0; i < result.nRangeCount; ++i )
		storage.pSortedDrawOrder[i] = i;
	std::sort( storage.pSortedDrawOrder, storage.pSortedDrawOrder + result.nRangeCount, sirVGSortDrawRange_s{ result.pRanges } );
	if ( !input.pDirectory->isCurrentSnapshot( nSnapshot ) )
		return SIR_VG_PLAN_STALE_SNAPSHOT;
	const sirVGPlanResult_t validation = sirVGValidateFramePlan( result );
	if ( validation != SIR_VG_PLAN_OK )
		return validation;
	*pPlan = result;
	return SIR_VG_PLAN_OK;
}
