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

#ifndef __SIRENGINE_SIRVGFRAMEPLAN_H__
#define __SIRENGINE_SIRVGFRAMEPLAN_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCache.h>

// Runtime CPU reference plan only. Pointers borrow caller-owned immutable
// arrays until consumption; none is a native GPU/physics/streaming handle.
// GRDG owns resource publication, Streaming owns demand, and the renderer
// owns native execution. This plan cannot transfer ownership to any of them.
static constexpr uint32_t SIR_VG_FRAME_PLAN_VERSION = 1;

typedef enum sirVGPlanResult_e : uint8_t
{
	SIR_VG_PLAN_OK = 0,
	SIR_VG_PLAN_INVALID_ARGUMENT = 1,
	SIR_VG_PLAN_UNSUPPORTED_VERSION = 2,
	SIR_VG_PLAN_STALE_SNAPSHOT = 3,
	SIR_VG_PLAN_INCOMPLETE_GROUP = 4,
	SIR_VG_PLAN_OVERFLOW = 5,
	SIR_VG_PLAN_DUPLICATE_DEMAND = 6
} sirVGPlanResult_t;

// A group is selected atomically: all its cooked clusters must be selected
// together. The cooker/manifest is responsible for supplying the expected
// count; plan validation forbids partial selection and partial range spans.
typedef struct sirVGSelectedGroup_s
{
	sirMeshKey_t keyMesh;
	uint64_t nInstanceId;
	uint64_t nGroupId;
	uint64_t nPageId;
	uint32_t nPageGeneration;
	uint32_t nFirstRange;
	uint32_t nRangeCount;
	uint32_t nExpectedClusterCount;
	uint32_t nSelectedClusterCount;
} sirVGSelectedGroup_t;

// CPU indexed draw arguments. Both baseVertex and firstInstance are retained
// even for devices that will need a reference emulation path.
typedef struct sirVGDrawRange_s
{
	sirMeshKey_t keyMesh;
	uint64_t nInstanceId;
	uint64_t nGroupId;
	uint64_t nMaterialId;
	uint64_t nSortKey;
	uint64_t nPageId;
	uint32_t nPageGeneration;
	uint32_t nPassId;
	uint32_t nFirstIndex;
	uint32_t nIndexCount;
	int32_t nBaseVertex;
	uint32_t nFirstInstance;
	uint32_t nInstanceCount;
} sirVGDrawRange_t;

// Sorted, unique logical demand from the CPU selector; no provider ticket,
// physical allocation, native descriptor or stream adapter is stored here.
typedef struct sirVGPageDemand_s
{
	uint64_t nPageId;
	uint32_t nManifestGeneration;
	uint32_t nRequestEpoch;
	uint64_t nDeadlineTick;
	uint8_t nPriority;
	uint8_t szReserved[7];
} sirVGPageDemand_t;

typedef struct sirVGFramePlan_s
{
	const sirVGSelectedGroup_t *pGroups;
	const sirVGDrawRange_t *pRanges;
	const sirVGPageDemand_t *pDemands;
	uint64_t nFrameId;
	uint32_t nVersion;
	uint32_t nPageSnapshotGeneration;
	uint32_t nGroupCount;
	uint32_t nRangeCount;
	uint32_t nDemandCount;
	uint32_t nReserved;
} sirVGFramePlan_t;

// Validates shape and local consistency only; never claims a GRDG pin,
// residency snapshot or fence is current. Caller still validates source
// revisions and publication epochs against the canonical owner.
sirVGPlanResult_t sirVGValidateFramePlan( const sirVGFramePlan_t& plan ) noexcept;

#endif // __SIRENGINE_SIRVGFRAMEPLAN_H__
