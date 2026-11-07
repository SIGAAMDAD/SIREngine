/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureResourcePlan.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

static uint64_t referenceFamilyBytes( uint32_t nFamily, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	uint64_t nBytes = ( uint64_t )nWidth * nHeight;
	if ( nFamily < 3 ) {
		nBytes = ( ( uint64_t )nWidth + 3 ) / 4 * ( ( ( uint64_t )nHeight + 3 ) / 4 ) * ( nFamily == 0 ? 8 : 16 );
	}
	if ( nFamily == 4 ) {
		nBytes *= 2;
	}
	return nBytes;
}

static int testPhysicalLayouts( void )
{
	sirVirtualTextureResourceLimits_t limits = { 32768, 128, 1, 1024, 2048, 128, 32, 16 };
	sirVirtualTextureResourcePlan_t plan = {};
	for ( uint32_t nLayers = 1; nLayers <= 1024; nLayers *= 2 ) {
		limits.nPhysicalPageSets = nLayers;
		SIR_TEST( sirVTComputeResourcePlan( limits, &plan ) );
		SIR_TEST( plan.nArrayBanks == SIR_VT_PHYSICAL_COUNT && plan.nPhysicalPageSets == nLayers );
		SIR_TEST( plan.nAllocationBytes == 128ULL * 32 && plan.nPageTableBytes == 32768ULL * 16 );
		uint64_t nTextures = 0;
		for ( uint32_t iFamily = 0; iFamily < SIR_VT_PHYSICAL_COUNT; ++iFamily ) {
			const uint64_t nReferenceTileBytes = referenceFamilyBytes( iFamily, 136, 136 );
			SIR_TEST( plan.szPhysicalBytes[iFamily] == nReferenceTileBytes * nLayers );
			SIR_TEST( plan.szTailBytes[iFamily] == referenceFamilyBytes( iFamily, limits.nTailAtlasWidth, limits.nTailAtlasHeight ) );
			nTextures += plan.szPhysicalBytes[iFamily] + plan.szTailBytes[iFamily];
		}
		SIR_TEST( plan.nTotalTextureBytes == nTextures );
		SIR_TEST( plan.nTotalBytes == nTextures + plan.nTotalBufferBytes );
	}
	return 0;
}

static int testInvalidAndTransactional( void )
{
	sirVirtualTextureResourceLimits_t limits = { 100, 16, 4, 8, 128, 32, 32, 16 };
	sirVirtualTextureResourcePlan_t sentinel = {};
	sentinel.nTotalBytes = 0x12345678;
	sirVirtualTextureResourcePlan_t plan = sentinel;
	SIR_TEST( !sirVTComputeResourcePlan( limits, nullptr ) );
	const uint32_t nSavedPages = limits.nPhysicalPageSets;
	limits.nPhysicalPageSets = 9;
	SIR_TEST( !sirVTComputeResourcePlan( limits, &plan ) && plan.nTotalBytes == sentinel.nTotalBytes );
	limits.nPhysicalPageSets = nSavedPages;
	limits.nTailAtlasWidth = 0;
	SIR_TEST( !sirVTComputeResourcePlan( limits, &plan ) && plan.nTotalBytes == sentinel.nTotalBytes );
	limits.nTailAtlasWidth = 129;
	SIR_TEST( !sirVTComputeResourcePlan( limits, &plan ) && plan.nTotalBytes == sentinel.nTotalBytes );
	limits.nTailAtlasWidth = 128;
	limits.nRuntimePageTableBytes = 0;
	SIR_TEST( !sirVTComputeResourcePlan( limits, &plan ) && plan.nTotalBytes == sentinel.nTotalBytes );
	limits.nRuntimePageTableBytes = 16;
	limits.nVirtualPages = UINT32_MAX;
	limits.nAllocations = UINT32_MAX;
	limits.nTailAtlasWidth = UINT32_MAX - 3;
	limits.nTailAtlasHeight = UINT32_MAX - 3;
	limits.nRuntimePageTableBytes = UINT32_MAX;
	limits.nRuntimeAllocationBytes = UINT32_MAX;
	SIR_TEST( !sirVTComputeResourcePlan( limits, &plan ) && plan.nTotalBytes == sentinel.nTotalBytes );
	return 0;
}

static int testBudgetSelection( void )
{
	sirVirtualTextureResourceLimits_t limits = { 2048, 32, 512, 1024, 2048, 64, 32, 16 };
	sirVirtualTextureResourcePlan_t full = {};
	sirVirtualTextureResourcePlan_t minimum = {};
	sirVirtualTextureResourceLimits_t one = limits;
	one.nPhysicalPageSets = 1;
	SIR_TEST( sirVTComputeResourcePlan( limits, &full ) );
	SIR_TEST( sirVTComputeResourcePlan( one, &minimum ) );
	sirVirtualTextureResourcePlan_t result = {};
	sirVirtualTextureResourceLimits_t fitted = {};
	fitted.nPhysicalPageSets = 777;
	result.nTotalBytes = 123456;
	SIR_TEST( !sirVTFitResourcePlanToBudget( limits, minimum.nTotalBytes - 1, &fitted, &result ) );
	SIR_TEST( fitted.nPhysicalPageSets == 777 && result.nTotalBytes == 123456 );
	SIR_TEST( sirVTFitResourcePlanToBudget( limits, minimum.nTotalBytes, &fitted, &result ) );
	SIR_TEST( fitted.nPhysicalPageSets == 1 && result.nTotalBytes == minimum.nTotalBytes );
	SIR_TEST( sirVTFitResourcePlanToBudget( limits, full.nTotalBytes, &fitted, &result ) );
	SIR_TEST( fitted.nPhysicalPageSets == limits.nPhysicalPageSets && result.nTotalBytes == full.nTotalBytes );
	for ( uint32_t nCandidate = 1; nCandidate <= limits.nPhysicalPageSets; nCandidate += 7 ) {
		one.nPhysicalPageSets = nCandidate;
		SIR_TEST( sirVTComputeResourcePlan( one, &minimum ) );
		SIR_TEST( sirVTFitResourcePlanToBudget( limits, minimum.nTotalBytes, &fitted, &result ) );
		SIR_TEST( fitted.nPhysicalPageSets == nCandidate );
		SIR_TEST( result.nTotalBytes == minimum.nTotalBytes );
		SIR_TEST( result.nPageTableBytes == full.nPageTableBytes && result.nAllocationBytes == full.nAllocationBytes );
	}
	SIR_TEST( !sirVTFitResourcePlanToBudget( limits, full.nTotalBytes, nullptr, &result ) );
	SIR_TEST( !sirVTFitResourcePlanToBudget( limits, full.nTotalBytes, &fitted, nullptr ) );
	return 0;
}

int main( void )
{
	if ( testPhysicalLayouts() || testInvalidAndTransactional() || testBudgetSelection() ) {
		return 1;
	}
	printf( "Terrain3D Sparse V1 format-family GPU resource plan PASS\n" );
	return 0;
}
