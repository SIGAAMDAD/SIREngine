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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>

static_assert( std::is_standard_layout<sirVGFramePlan_t>::value, "frame plan must be standard layout" );
static_assert( std::is_trivially_copyable<sirVGFramePlan_t>::value, "frame plan must be trivially copyable" );
static_assert( std::is_standard_layout<sirVGDrawRange_t>::value, "draw range must be standard layout" );
static_assert( std::is_trivially_copyable<sirVGSelectedGroup_t>::value, "selected group must be trivially copyable" );
static_assert( sizeof( sirVGPageDemand_t ) == 32, "page demands require a fixed 32-byte ABI" );
static_assert( sizeof( sirMeshKey_t ) == 24, "mesh key must stay separate from runtime handles" );
static_assert( offsetof( sirVGDrawRange_t, nBaseVertex ) < offsetof( sirVGDrawRange_t, nFirstInstance ),
               "baseVertex/firstInstance draw arguments must remain distinct" );

static void checkVG( bool bPass, const char *pszDescription ) noexcept
{
	if ( !bPass ) {
		fprintf( stderr, "VG frame plan FAILED: %s\n", pszDescription );
		abort();
	}
}

static void expectVG( const sirVGFramePlan_t& plan, sirVGPlanResult_t eResult, const char *pszDescription ) noexcept
{
	checkVG( sirVGValidateFramePlan( plan ) == eResult, pszDescription );
}

int main( void )
{
	sirVGSelectedGroup_t groups[2] = {};
	sirVGDrawRange_t ranges[3] = {};
	sirVGPageDemand_t demands[2] = {};
	sirVGFramePlan_t plan = { groups, ranges, demands, 100, SIR_VG_FRAME_PLAN_VERSION, 4, 2, 3, 2, 0 };
	groups[0] = { { 42, 7, 3, 0 }, 1001, 70, 99, 4, 0, 2, 3, 3 };
	groups[1] = { { 42, 7, 3, 0 }, 1002, 71, 100, 4, 2, 1, 1, 1 };
	ranges[0] = { { 42, 7, 3, 0 }, 1001, 70, 9, 100, 99, 4, 2, 3, 6, -4, 7, 2 };
	ranges[1] = { { 42, 7, 3, 0 }, 1001, 70, 10, 101, 99, 4, 2, 9, 3, 6, 9, 2 };
	ranges[2] = { { 42, 7, 3, 0 }, 1002, 71, 9, 105, 100, 4, 4, 0, 3, 0, 0, 1 };
	demands[0] = { 101, 4, 2, 999, 100, {} };
	demands[1] = { 103, 4, 2, 1000, 200, {} };
	expectVG( plan, SIR_VG_PLAN_OK, "full CPU reference plan" );
	checkVG( ranges[0].nBaseVertex == -4 && ranges[0].nFirstInstance == 7 && ranges[0].nInstanceCount == 2,
	         "preserve baseVertex and firstInstance" );

	// Version/snapshot/request epochs must be explicitly supplied and coherent.
	plan.nVersion = 2;
	expectVG( plan, SIR_VG_PLAN_UNSUPPORTED_VERSION, "version mismatch" );
	plan.nVersion = 1;
	plan.nFrameId = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "frame identity missing" );
	plan.nFrameId = 100;
	plan.nPageSnapshotGeneration = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "snapshot identity missing" );
	plan.nPageSnapshotGeneration = 4;
	plan.nReserved = 1;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "nonzero reserved field" );
	plan.nReserved = 0;
	plan.nGroupCount = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "ranges without groups" );
	plan.nGroupCount = 2;
	plan.pGroups = nullptr;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "null groups" );
	plan.pGroups = groups;
	plan.pRanges = nullptr;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "null ranges" );
	plan.pRanges = ranges;
	plan.pDemands = nullptr;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "null demands" );
	plan.pDemands = demands;

	// Selected group ranges must tile the draw array and carry identical IDs.
	groups[0].keyMesh.nAssetId = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "empty mesh asset" );
	groups[0].keyMesh.nAssetId = 42;
	groups[0].keyMesh.nRevision = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing mesh revision" );
	groups[0].keyMesh.nRevision = 7;
	groups[0].keyMesh.nReserved = 3;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "mesh key reserved bits" );
	groups[0].keyMesh.nReserved = 0;
	groups[0].nInstanceId = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing instance ID" );
	groups[0].nInstanceId = 1001;
	groups[0].nGroupId = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing group ID" );
	groups[0].nGroupId = 70;
	groups[0].nPageId = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing page ID" );
	groups[0].nPageId = 99;
	groups[0].nExpectedClusterCount = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing cluster count" );
	groups[0].nExpectedClusterCount = 3;
	groups[0].nPageGeneration = 5;
	expectVG( plan, SIR_VG_PLAN_STALE_SNAPSHOT, "stale group snapshot" );
	groups[0].nPageGeneration = 4;
	groups[0].nSelectedClusterCount = 2;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "missing cluster in group" );
	groups[0].nSelectedClusterCount = 3;
	groups[0].nFirstRange = 1;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "leading gap" );
	groups[0].nFirstRange = 0;
	groups[0].nRangeCount = 0;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "empty group" );
	groups[0].nRangeCount = 2;
	groups[0].nRangeCount = 4;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "group exceeds plan" );
	groups[0].nRangeCount = 2;
	groups[1].nFirstRange = 1;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "overlapping ranges" );
	groups[1].nFirstRange = 2;
	groups[1].nRangeCount = 0;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "empty second group" );
	groups[1].nRangeCount = 1;
	groups[1].nRangeCount = 2;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "tail out of bounds" );
	groups[1].nRangeCount = 1;
	plan.nRangeCount = 4;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "uncovered final range" );
	plan.nRangeCount = 3;

	ranges[0].nPageGeneration = 5;
	expectVG( plan, SIR_VG_PLAN_STALE_SNAPSHOT, "stale draw snapshot" );
	ranges[0].nPageGeneration = 4;
	ranges[0].keyMesh.nVariant = 5;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign mesh revision" );
	ranges[0].keyMesh.nVariant = 3;
	ranges[0].keyMesh.nAssetId = 77;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign asset key" );
	ranges[0].keyMesh.nAssetId = 42;
	ranges[0].keyMesh.nRevision = 9;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign revision key" );
	ranges[0].keyMesh.nRevision = 7;
	ranges[0].keyMesh.nReserved = 1;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign key reserved bits" );
	ranges[0].keyMesh.nReserved = 0;
	ranges[0].nInstanceId = 55;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign instance" );
	ranges[0].nInstanceId = 1001;
	ranges[0].nGroupId = 78;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign group" );
	ranges[0].nGroupId = 70;
	ranges[0].nPageId = 98;
	expectVG( plan, SIR_VG_PLAN_INCOMPLETE_GROUP, "foreign page" );
	ranges[0].nPageId = 99;
	ranges[0].nIndexCount = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "empty draw" );
	ranges[0].nIndexCount = 5;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "non-triangle draw" );
	ranges[0].nIndexCount = 6;
	ranges[0].nInstanceCount = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "empty instance run" );
	ranges[0].nInstanceCount = 2;
	ranges[0].nFirstIndex = UINT32_MAX - 3;
	expectVG( plan, SIR_VG_PLAN_OVERFLOW, "index end overflow" );
	ranges[0].nFirstIndex = 3;
	ranges[0].nFirstInstance = UINT32_MAX;
	expectVG( plan, SIR_VG_PLAN_OVERFLOW, "instance end overflow" );
	ranges[0].nFirstInstance = 7;

	demands[0].nPageId = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing demand page" );
	demands[0].nPageId = 101;
	demands[0].nRequestEpoch = 0;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "missing request epoch" );
	demands[0].nRequestEpoch = 2;
	demands[0].szReserved[3] = 1;
	expectVG( plan, SIR_VG_PLAN_INVALID_ARGUMENT, "nonzero demand reserved field" );
	demands[0].szReserved[3] = 0;
	demands[0].nManifestGeneration = 3;
	expectVG( plan, SIR_VG_PLAN_STALE_SNAPSHOT, "stale demand snapshot" );
	demands[0].nManifestGeneration = 4;
	demands[1].nPageId = 101;
	expectVG( plan, SIR_VG_PLAN_DUPLICATE_DEMAND, "duplicate demand" );
	demands[1].nPageId = 100;
	expectVG( plan, SIR_VG_PLAN_DUPLICATE_DEMAND, "out of order demands" );
	demands[1].nPageId = 103;
	expectVG( plan, SIR_VG_PLAN_OK, "restored reference plan" );

	// A demand-only frame carries prefetch information without any draws.
	plan.nGroupCount = 0;
	plan.nRangeCount = 0;
	plan.pGroups = nullptr;
	plan.pRanges = nullptr;
	expectVG( plan, SIR_VG_PLAN_OK, "valid prefetch-only plan" );
	plan.nDemandCount = 0;
	plan.pDemands = nullptr;
	expectVG( plan, SIR_VG_PLAN_OK, "valid empty reference plan" );
	return 0;
}
