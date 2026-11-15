/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRVGFRAMECOMPILER_H__
#define __SIRENGINE_SIRVGFRAMECOMPILER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRVGFramePlan.h>
#include <Mesh/SIRVGPageDirectory.h>

// The WorldState/visibility owner supplies the same per-instance bounds and
// source identity to all rendering and physics consumers. This compiler owns
// no instance transforms or geometry; it selects a complete cooked LOD cut.
typedef struct sirVGFrameInstance_s
{
	sirMeshKey_t keyMesh;
	uint64_t nInstanceId;
	float szCenterXYZ[3];
	float fBoundingRadius;
	float fWorldScale;
	uint32_t nFirstInstance;
} sirVGFrameInstance_t;

typedef struct sirVGFrameView_s
{
	float szCameraXYZ[3];
	float szFrustumPlanes[6][4];
	float fProjectionPixelsPerUnit;
	float fAllowedErrorPixels;
	float fNearDistance;
	uint64_t nFrameId;
	uint64_t nDeadlineTick;
	uint32_t nRequestEpoch;
	uint32_t nPassId;
	uint32_t nFrustumPlaneCount;
	uint8_t nDemandPriority;
} sirVGFrameView_t;

typedef struct sirVGFrameCompileInput_s
{
	const SIRMeshPackSchema *pSchema;
	const SIRVGPageDirectory *pDirectory;
	const sirVGFrameInstance_t *pInstances;
	uint32_t nInstanceCount;
	sirVGFrameView_t view;
} sirVGFrameCompileInput_t;

// All buffers are fixed caller-owned scratch; sortedDrawOrder contains range
// indexes ordered by pass/material/page, preserving contiguous group ranges
// in the portable sirVGFramePlan_t. No heap is used during compilation.
typedef struct sirVGFrameCompileStorage_s
{
	sirVGSelectedGroup_t *pGroups;
	sirVGDrawRange_t *pRanges;
	sirVGPageDemand_t *pDemands;
	uint32_t *pGroupStack;
	uint32_t *pSortedDrawOrder;
	uint32_t nGroupCapacity;
	uint32_t nRangeCapacity;
	uint32_t nDemandCapacity;
	uint32_t nStackCapacity;
	uint32_t nDrawOrderCapacity;
} sirVGFrameCompileStorage_t;

class SIRVGFrameCompiler
{
public:
	static sirVGPlanResult_t compile( const sirVGFrameCompileInput_t& input, const sirVGFrameCompileStorage_t& storage,
	                                  sirVGFramePlan_t *pPlan ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRVGFRAMECOMPILER_H__
