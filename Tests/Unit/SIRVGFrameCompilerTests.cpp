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
#include <Tools/GeometryCooker/SIRGeometryCooker.h>

#include <cstdio>
#include <limits>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Virtual geometry test failed at %s:%u: %s\n", __FILE__, __LINE__, #expression );                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirVGFixture_s
{
	std::vector<float> xyz;
	std::vector<uint32_t> indices;
	sirGeometryCookMaterial_t materials[2];
	sirGeometryCookOutput_t cooked;
	SIRMeshPackSchema schema;
	std::vector<sirVGPageSlot_t> pages;
	std::vector<sirVGClusterDrawLayout_t> layouts;
	std::vector<uint64_t> offsets;
	uint64_t nBackingBytes;
	SIRVGPageDirectory directory;
} sirVGFixture_t;

static bool makeFixture( sirVGFixture_t& fixture ) noexcept
{
	const uint32_t GRID = 9;
	for ( uint32_t y = 0; y < GRID; ++y ) {
		for ( uint32_t x = 0; x < GRID; ++x ) {
			fixture.xyz.push_back( ( float )x );
			fixture.xyz.push_back( ( float )y );
			fixture.xyz.push_back( ( float )( ( x * y ) % 7 ) * 0.13f );
		}
	}
	for ( uint32_t material = 0; material < 2; ++material ) {
		fixture.materials[material].nFirstIndex = ( uint32_t )fixture.indices.size();
		fixture.materials[material].nMaterialId = 100 + material;
		for ( uint32_t y = 0; y < GRID - 1; ++y ) {
			for ( uint32_t x = material * 4; x < material * 4 + 4; ++x ) {
				const uint32_t a = y * GRID + x;
				const uint32_t b = a + 1;
				const uint32_t c = a + GRID;
				const uint32_t d = c + 1;
				fixture.indices.insert( fixture.indices.end(), { a, b, c, b, d, c } );
			}
		}
		fixture.materials[material].nIndexCount = ( uint32_t )fixture.indices.size() - fixture.materials[material].nFirstIndex;
	}
	sirGeometryCookInput_t input = {};
	input.key = { 0x7531, 42, 3, 0 };
	input.nResourceTypeId = 103;
	input.nPackageId = 10;
	input.nGeneration = 1;
	input.pPositionsXYZ = fixture.xyz.data();
	input.nVertexCount = ( uint32_t )fixture.xyz.size() / 3;
	input.pIndices = fixture.indices.data();
	input.nIndexCount = ( uint32_t )fixture.indices.size();
	input.pMaterials = fixture.materials;
	input.nMaterialCount = 2;
	input.nMaxVerticesPerMeshlet = 32;
	input.nMaxTrianglesPerMeshlet = 16;
	input.nTargetRootTriangles = 24;
	input.fMaxSimplificationError = 0.5f;
	CHECK( SIRGeometryCooker::cook( input, &fixture.cooked ) == SIR_PACK_OK );
	CHECK( fixture.schema.open( fixture.cooked.manifest.data(), fixture.cooked.manifest.size() ) == SIR_MESH_PACK_OK );
	CHECK( fixture.schema.getPageCount() >= 4 && fixture.schema.getGroupCount() >= 5 );
	fixture.pages.resize( fixture.schema.getPageCount() );
	fixture.layouts.resize( fixture.schema.getClusterCount() );
	fixture.offsets.resize( fixture.schema.getPageCount() );
	uint64_t nCursor = 0;
	for ( uint32_t i = 0; i < fixture.cooked.pages.size(); ++i ) {
		fixture.offsets[i] = nCursor;
		uint32_t nVertices = 0, nIndices = 0;
		CHECK( fixture.schema.getDecodedPageCounts( i, &nVertices, &nIndices ) == SIR_MESH_PACK_OK );
		const uint64_t nNativeBytes = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t ) + ( uint64_t )nIndices * sizeof( uint32_t );
		nCursor += ( nNativeBytes + 15ULL ) & ~15ULL;
	}
	fixture.nBackingBytes = nCursor;
	const sirVGPageStorage_t storage = { fixture.pages.data(), fixture.layouts.data(), ( uint32_t )fixture.pages.size(),
	                                     ( uint32_t )fixture.layouts.size() };
	const sirVGPageArena_t arena = { 0x123456, nCursor, nCursor, 7, 1 };
	CHECK( fixture.directory.initialize( fixture.schema, storage, arena ) == SIR_VG_RESIDENCY_OK );
	return true;
}

static sirVGPageAdmission_t admissionFor( const sirVGFixture_t& f, uint32_t i ) noexcept
{
	uint32_t nVertices = 0, nIndices = 0;
	( void )f.schema.getDecodedPageCounts( i, &nVertices, &nIndices );
	const uint64_t nNativeBytes = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t ) + ( uint64_t )nIndices * sizeof( uint32_t );
	return { f.cooked.pages[i].data(),
	         f.cooked.pages[i].size(),
	         ( nNativeBytes + 15ULL ) & ~15ULL,
	         f.directory.getBackingId(),
	         f.offsets[i],
	         100ULL + i * 10,
	         101ULL + i * 10,
	         102ULL + i * 10,
	         i,
	         f.directory.getDeviceGeneration(),
	         f.directory.getRequestEpoch() };
}

static sirVGPageCompletion_t completionFor( const sirVGPageAdmission_t& a ) noexcept
{
	return { a.nCopyFence, a.nComputeFence, a.nGraphicsFence, a.nPage, a.nDeviceGeneration, a.nRequestEpoch, true };
}

static bool installPage( sirVGFixture_t& f, uint32_t i ) noexcept
{
	const sirVGPageAdmission_t admission = admissionFor( f, i );
	CHECK( f.directory.admit( admission ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.getState( i ) == SIR_VG_PAGE_UPLOADING );
	CHECK( !f.directory.isResident( i ) );
	CHECK( f.directory.complete( completionFor( admission ) ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.isResident( i ) );
	return true;
}

typedef struct sirVGPlanScratch_s
{
	std::vector<sirVGSelectedGroup_t> groups;
	std::vector<sirVGDrawRange_t> ranges;
	std::vector<sirVGPageDemand_t> demands;
	std::vector<uint32_t> stack;
	std::vector<uint32_t> order;
	sirVGFrameCompileStorage_t storage( void ) noexcept
	{
		return { groups.data(),
		         ranges.data(),
		         demands.data(),
		         stack.data(),
		         order.data(),
		         ( uint32_t )groups.size(),
		         ( uint32_t )ranges.size(),
		         ( uint32_t )demands.size(),
		         ( uint32_t )stack.size(),
		         ( uint32_t )order.size() };
	}
} sirVGPlanScratch_t;

static sirVGPlanScratch_t makeScratch( const sirVGFixture_t& f, uint32_t nInstances )
{
	sirVGPlanScratch_t scratch;
	scratch.groups.resize( nInstances * f.schema.getGroupCount() );
	scratch.ranges.resize( nInstances * f.schema.getClusterCount() );
	scratch.demands.resize( f.schema.getPageCount() );
	scratch.stack.resize( f.schema.getGroupCount() );
	scratch.order.resize( scratch.ranges.size() );
	return scratch;
}

static sirVGFrameCompileInput_t makeInput( const sirVGFixture_t& f, const sirVGFrameInstance_t *pInstances, uint32_t nInstances ) noexcept
{
	sirVGFrameCompileInput_t input = {};
	input.pSchema = &f.schema;
	input.pDirectory = &f.directory;
	input.pInstances = pInstances;
	input.nInstanceCount = nInstances;
	input.view.szCameraXYZ[0] = 4.0f;
	input.view.szCameraXYZ[1] = 4.0f;
	input.view.szCameraXYZ[2] = 8.0f;
	input.view.fProjectionPixelsPerUnit = 1000.0f;
	input.view.fAllowedErrorPixels = 0.00001f;
	input.view.fNearDistance = 0.1f;
	input.view.nFrameId = 7;
	input.view.nDeadlineTick = 100;
	input.view.nPassId = 2;
	input.view.nRequestEpoch = f.directory.getRequestEpoch();
	input.view.nDemandPriority = 200;
	return input;
}

static bool testCompleteCut( void ) noexcept
{
	sirVGFixture_t f = {};
	CHECK( makeFixture( f ) );
	sirVGFrameInstance_t instance = { f.schema.getKey(), 0xAA, { 4.0f, 4.0f, 0.0f }, 1.0f, 1.0f, 3 };
	sirVGFrameCompileInput_t input = makeInput( f, &instance, 1 );
	sirVGPlanScratch_t scratch = makeScratch( f, 2 );
	sirVGFrameCompileStorage_t storage = scratch.storage();
	sirVGFramePlan_t plan = {};
	plan.nFrameId = 999;
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_STALE_SNAPSHOT );
	CHECK( plan.nFrameId == 999 );
	CHECK( f.directory.beginRetire( 0 ) == SIR_VG_RESIDENCY_INVALID_ARGUMENT );
	CHECK( f.directory.admit( admissionFor( f, 1 ) ) == SIR_VG_RESIDENCY_NOT_READY );
	CHECK( installPage( f, 0 ) );
	CHECK( f.directory.getCommittedBytes() == admissionFor( f, 0 ).nNativeBytes );
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount == 1 && plan.nRangeCount != 0 && plan.nDemandCount >= 1 );
	CHECK( plan.pGroups[0].nGroupId == 0 && plan.pGroups[0].nPageId == 1 );
	CHECK( sirVGValidateFramePlan( plan ) == SIR_VG_PLAN_OK );
	for ( uint32_t i = 1; i < plan.nDemandCount; ++i )
		CHECK( plan.pDemands[i].nPageId > plan.pDemands[i - 1].nPageId );
	for ( uint32_t i = 0; i < plan.nRangeCount; ++i )
		CHECK( plan.pRanges[i].nMaterialId == 100 || plan.pRanges[i].nMaterialId == 101 );

	// One missing sibling must keep the complete root, never a partial cut.
	CHECK( installPage( f, 1 ) );
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount == 1 && plan.pGroups[0].nGroupId == 0 );
	CHECK( installPage( f, 2 ) );
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount == 2 && plan.pGroups[0].nGroupId == 1 && plan.pGroups[1].nGroupId == 2 );
	CHECK( plan.nDemandCount >= 1 );

	for ( uint32_t i = 3; i < f.cooked.pages.size(); ++i )
		CHECK( installPage( f, i ) );
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount >= 2 && sirVGValidateFramePlan( plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nDemandCount == 0 );
	for ( uint32_t i = 1; i < plan.nRangeCount; ++i ) {
		const sirVGDrawRange_t& before = plan.pRanges[scratch.order[i - 1]];
		const sirVGDrawRange_t& after = plan.pRanges[scratch.order[i]];
		CHECK( before.nSortKey <= after.nSortKey );
	}
	const uint32_t nFineGroups = plan.nGroupCount;

	// Distance changes only the selected cut. It does not change canonical
	// collision identity or allocate a second physical geometry backing.
	input.view.szCameraXYZ[2] = 1000000.0f;
	input.view.fAllowedErrorPixels = 1000.0f;
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount == 1 && plan.pGroups[0].nGroupId == 0 );
	input.view.szCameraXYZ[2] = 8;
	input.view.fAllowedErrorPixels = 0.00001f;
	input.view.nFrustumPlaneCount = 1;
	input.view.szFrustumPlanes[0][0] = 1;
	input.view.szFrustumPlanes[0][3] = -100;
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount == 0 && plan.nRangeCount == 0 );
	input.view.nFrustumPlaneCount = 0;

	// The same cooked pages service multiple instances without extra backing.
	const sirVGFrameInstance_t instances[2] = { instance, { f.schema.getKey(), 0xBB, { 8, 4, 0 }, 1, 1, 4 } };
	input.pInstances = instances;
	input.nInstanceCount = 2;
	const uint64_t nBeforeBytes = f.directory.getCommittedBytes();
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount == 2 * nFineGroups && f.directory.getCommittedBytes() == nBeforeBytes );
	CHECK( sirVGValidateFramePlan( plan ) == SIR_VG_PLAN_OK );

	// Native device generations and retired page fences invalidate old plans.
	sirVGPagePin_t pin = {};
	const uint32_t nLeafPage = ( uint32_t )f.pages.size() - 1;
	CHECK( f.directory.acquire( nLeafPage, &pin ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.beginRetire( nLeafPage ) == SIR_VG_RESIDENCY_BUSY );
	CHECK( f.directory.markSubmitted( nLeafPage, 777 ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.release( pin ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.release( pin ) == SIR_VG_RESIDENCY_STALE );
	CHECK( f.directory.beginRetire( nLeafPage ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.finishRetire( nLeafPage, 776 ) == SIR_VG_RESIDENCY_BUSY );
	CHECK( f.directory.finishRetire( nLeafPage, 777 ) == SIR_VG_RESIDENCY_OK );
	CHECK( !f.directory.isResident( nLeafPage ) && f.directory.getCommittedBytes() < nBeforeBytes );
	input.nInstanceCount = 1;
	input.pInstances = &instance;
	CHECK( SIRVGFrameCompiler::compile( input, storage, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nGroupCount < nFineGroups && plan.nRangeCount != 0 );
	CHECK( plan.nDemandCount != 0 );
	CHECK( f.directory.getBackingId() == 0x123456 );
	CHECK( f.directory.shutdown() == SIR_VG_RESIDENCY_OK );
	return true;
}

static bool testBadCompletionAndDeviceReset( void ) noexcept
{
	sirVGFixture_t f = {};
	CHECK( makeFixture( f ) );
	sirVGPageAdmission_t root = admissionFor( f, 0 );
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_BUSY );
	sirVGPageCompletion_t completion = completionFor( root );
	completion.nCompletedCopyFence--;
	CHECK( f.directory.complete( completion ) == SIR_VG_RESIDENCY_NOT_READY );
	CHECK( !f.directory.isResident( 0 ) );
	completion = completionFor( root );
	completion.nCompletedComputeFence--;
	CHECK( f.directory.complete( completion ) == SIR_VG_RESIDENCY_NOT_READY );
	completion = completionFor( root );
	completion.nCompletedGraphicsFence--;
	CHECK( f.directory.complete( completion ) == SIR_VG_RESIDENCY_NOT_READY );
	CHECK( f.directory.complete( completionFor( root ) ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.complete( completionFor( root ) ) == SIR_VG_RESIDENCY_NOT_READY );
	root = admissionFor( f, 1 );
	root.nNativeOffset = 0;
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_OUT_OF_BUDGET );
	root = admissionFor( f, 1 );
	root.nNativeBytes = f.nBackingBytes + 1;
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_OUT_OF_BUDGET );
	root = admissionFor( f, 1 );
	root.nNativeBytes = 0;
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_INVALID_ARGUMENT );
	root = admissionFor( f, 1 );
	root.nPhysicalBackingId++;
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_INVALID_ARGUMENT );
	root = admissionFor( f, 1 );
	root.nRequestEpoch++;
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_STALE );
	root = admissionFor( f, 1 );
	std::vector<uint8_t> corrupt = f.cooked.pages[1];
	corrupt.back() ^= 1;
	root.pCookedBytes = corrupt.data();
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_BAD_SOURCE );
	CHECK( f.directory.getState( 1 ) == SIR_VG_PAGE_MISSING );
	root = admissionFor( f, 1 );
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.advanceEpoch( 2 ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.complete( completionFor( root ) ) == SIR_VG_RESIDENCY_STALE );
	CHECK( f.directory.getState( 1 ) == SIR_VG_PAGE_MISSING );
	CHECK( f.directory.isResident( 0 ) );
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_STALE );
	root = admissionFor( f, 1 );
	CHECK( f.directory.admit( root ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.complete( completionFor( root ) ) == SIR_VG_RESIDENCY_OK );
	// Reset must reject held readers and then discard every old physical page.
	sirVGPagePin_t reader = {};
	CHECK( f.directory.acquire( 1, &reader ) == SIR_VG_RESIDENCY_OK );
	const sirVGPageArena_t nextArena = { 0x123457, f.nBackingBytes, f.nBackingBytes, 8, 1 };
	CHECK( f.directory.resetDevice( nextArena ) == SIR_VG_RESIDENCY_BUSY );
	CHECK( f.directory.release( reader ) == SIR_VG_RESIDENCY_OK );
	const uint32_t oldSnapshot = f.directory.getSnapshotGeneration();
	CHECK( f.directory.resetDevice( nextArena ) == SIR_VG_RESIDENCY_OK );
	CHECK( !f.directory.isCurrentSnapshot( oldSnapshot ) );
	CHECK( f.directory.getCommittedBytes() == 0 && !f.directory.isResident( 0 ) );
	CHECK( f.directory.getBackingId() == 0x123457 && f.directory.getDeviceGeneration() == 8 );
	CHECK( f.directory.release( reader ) == SIR_VG_RESIDENCY_STALE );
	CHECK( f.directory.complete( completionFor( root ) ) == SIR_VG_RESIDENCY_NOT_READY );
	CHECK( f.directory.admit( admissionFor( f, 1 ) ) == SIR_VG_RESIDENCY_NOT_READY );
	CHECK( installPage( f, 0 ) );
	CHECK( installPage( f, 1 ) );
	CHECK( f.directory.resetDevice( nextArena ) == SIR_VG_RESIDENCY_INVALID_ARGUMENT );
	CHECK( f.directory.shutdown() == SIR_VG_RESIDENCY_OK );
	return true;
}

static bool testNativeBudgetDistinctFromCookedBytes( void ) noexcept
{
	sirVGFixture_t f = {};
	CHECK( makeFixture( f ) );
	sirMeshPackPage_t root = {};
	CHECK( f.schema.pageAt( 0, &root ) == SIR_MESH_PACK_OK );
	uint32_t nVertices = 0, nIndices = 0;
	CHECK( f.schema.getDecodedPageCounts( 0, &nVertices, &nIndices ) == SIR_MESH_PACK_OK );
	const uint64_t nRootBytes = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t ) + ( uint64_t )nIndices * sizeof( uint32_t );
	CHECK( nRootBytes != root.nByteCount && nRootBytes != 0 );
	CHECK( f.directory.shutdown() == SIR_VG_RESIDENCY_OK );
	const sirVGPageStorage_t storage = { f.pages.data(), f.layouts.data(), ( uint32_t )f.pages.size(), ( uint32_t )f.layouts.size() };
	const sirVGPageArena_t tooSmall = { 0xFEDCBA, nRootBytes - 1, nRootBytes - 1, 9, 1 };
	CHECK( f.directory.initialize( f.schema, storage, tooSmall ) == SIR_VG_RESIDENCY_OUT_OF_BUDGET );
	const sirVGPageArena_t arena = { 0xFEDCBA, ( nRootBytes + 15ULL ) & ~15ULL, ( nRootBytes + 15ULL ) & ~15ULL, 9, 1 };
	CHECK( f.directory.initialize( f.schema, storage, arena ) == SIR_VG_RESIDENCY_OK );
	sirVGPageAdmission_t admission = admissionFor( f, 0 );
	admission.nNativeOffset = 0;
	admission.nNativeBytes = nRootBytes - 1;
	CHECK( f.directory.admit( admission ) == SIR_VG_RESIDENCY_BAD_SOURCE );
	admission.nNativeBytes = ( nRootBytes + 15ULL ) & ~15ULL;
	CHECK( f.directory.admit( admission ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.complete( completionFor( admission ) ) == SIR_VG_RESIDENCY_OK );
	CHECK( f.directory.getCommittedBytes() == admission.nNativeBytes && f.directory.isResident( 0 ) );
	CHECK( f.directory.shutdown() == SIR_VG_RESIDENCY_OK );
	return true;
}

int main( void )
{
	if ( !testCompleteCut() || !testBadCompletionAndDeviceReset() || !testNativeBudgetDistinctFromCookedBytes() )
		return 1;
	puts( "Virtual geometry complete-group LOD, sealed residency and native-generation checks passed" );
	return 0;
}
