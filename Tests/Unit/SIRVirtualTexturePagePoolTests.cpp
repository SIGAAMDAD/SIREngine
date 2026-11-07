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

#include <Renderer/VirtualTexturing/SIRVirtualTexturePagePool.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

static sirGraphicsCompletionSet_t makeCompleted( uint64_t nDirect, uint64_t nCompute, uint64_t nCopy ) noexcept
{
	sirGraphicsCompletionSet_t done = {};
	done.nDeviceGeneration = 7;
	done.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = nDirect;
	done.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] = nCompute;
	done.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = nCopy;
	if ( nDirect != 0 ) {
		done.nQueueMask |= 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	}
	if ( nCompute != 0 ) {
		done.nQueueMask |= 1U << SIR_GRAPHICS_QUEUE_COMPUTE;
	}
	if ( nCopy != 0 ) {
		done.nQueueMask |= 1U << SIR_GRAPHICS_QUEUE_COPY;
	}
	return done;
}

static int testQueueSafeLifecycle( void )
{
	sirVirtualTexturePagePoolSlot_t slots[SIR_VT_PHYSICAL_COUNT * 2] = {};
	uint32_t virtualMap[16] = {};
	SIRVirtualTexturePagePool pool;
	SIR_TEST( pool.init( slots, SIR_VT_PHYSICAL_COUNT * 2, 2, virtualMap, 16, 7 ) );
	SIR_TEST( !pool.init( slots, SIR_VT_PHYSICAL_COUNT * 2, 2, virtualMap, 16, 7 ) );
	sirVirtualTexturePagePoolReservation_t a = {};
	sirVirtualTexturePagePoolReservation_t b = {};
	SIR_TEST( !pool.reserve( 16, SIR_VT_PHYSICAL_BC1, 12, &a ) );
	SIR_TEST( pool.reserve( 3, SIR_VT_PHYSICAL_BC1, 12, &a ) );
	SIR_TEST( a.nSlot == 0 && virtualMap[3] == UINT32_MAX );
	SIR_TEST( !pool.reserve( 3, SIR_VT_PHYSICAL_BC7, 12, &b ) );
	SIR_TEST( pool.reserve( 4, SIR_VT_PHYSICAL_BC1, 12, &b ) && b.nSlot == 1 );
	SIR_TEST( !pool.reserve( 5, SIR_VT_PHYSICAL_BC1, 12, &b ) );
	SIR_TEST( !pool.resetCold() );

	const sirGraphicsCompletionSet_t copy2 = makeCompleted( 0, 0, 2 );
	const sirGraphicsCompletionSet_t copy1 = makeCompleted( 0, 0, 1 );
	const sirGraphicsCompletionSet_t copy4 = makeCompleted( 0, 0, 4 );
	SIR_TEST( pool.commitUpload( a, copy2 ) );
	SIR_TEST( !pool.commitUpload( a, copy2 ) );
	uint32_t nSlot = 991;
	SIR_TEST( !pool.findResident( 3, 12, &nSlot ) && nSlot == 991 );
	SIR_TEST( pool.serviceCompletions( copy1 ) == 0 );
	SIR_TEST( pool.serviceCompletions( copy2 ) == 1 );
	SIR_TEST( pool.findResident( 3, 12, &nSlot ) && nSlot == a.nSlot );
	SIR_TEST( !pool.findResident( 3, 11, &nSlot ) );
	SIR_TEST( !pool.reserve( 3, SIR_VT_PHYSICAL_BC1, 13, &b ) );
	SIR_TEST( !pool.beginRetire( 3, makeCompleted( 0, 0, 0 ) ) );
	const sirGraphicsCompletionSet_t retiring = makeCompleted( 5, 0, 3 );
	SIR_TEST( pool.beginRetire( 3, retiring ) );
	SIR_TEST( !pool.findResident( 3, 12, &nSlot ) );
	SIR_TEST( !pool.resetCold() );
	SIR_TEST( !pool.reserve( 5, SIR_VT_PHYSICAL_BC1, 12, &a ) );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 4, 0, 3 ) ) == 0 );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 5, 0, 2 ) ) == 0 );
	SIR_TEST( pool.serviceCompletions( retiring ) == 1 );
	SIR_TEST( !pool.commitUpload( a, copy4 ) );
	SIR_TEST( pool.reserve( 5, SIR_VT_PHYSICAL_BC1, 13, &a ) && a.nSlot == 0 );
	SIR_TEST( pool.abortUpload( a, &copy4 ) );
	SIR_TEST( !pool.reserve( 6, SIR_VT_PHYSICAL_BC1, 13, &a ) );
	SIR_TEST( pool.serviceCompletions( copy2 ) == 0 );
	SIR_TEST( pool.serviceCompletions( copy4 ) == 1 );
	SIR_TEST( pool.reserve( 6, SIR_VT_PHYSICAL_BC1, 13, &a ) );
	SIR_TEST( pool.abortUpload( a, nullptr ) );
	SIR_TEST( pool.abortUpload( b, nullptr ) );
	SIR_TEST( pool.resetCold() );
	return 0;
}

static int testGenerationAndIndependentFamilies( void )
{
	sirVirtualTexturePagePoolSlot_t slots[SIR_VT_PHYSICAL_COUNT] = {};
	uint32_t virtualMap[64] = {};
	SIRVirtualTexturePagePool pool;
	SIR_TEST( !pool.init( slots, SIR_VT_PHYSICAL_COUNT, 1, virtualMap, 64, 0 ) );
	SIR_TEST( pool.init( slots, SIR_VT_PHYSICAL_COUNT, 1, virtualMap, 64, 7 ) );
	for ( uint32_t i = 0; i < SIR_VT_PHYSICAL_COUNT; ++i ) {
		sirVirtualTexturePagePoolReservation_t ticket = {};
		SIR_TEST( pool.reserve( i, ( sirVirtualTexturePhysicalFamily_t )i, 1, &ticket ) );
		SIR_TEST( ticket.nSlot == i );
		const sirGraphicsCompletionSet_t completion = makeCompleted( 3, 0, 2 );
		SIR_TEST( pool.commitUpload( ticket, completion ) );
	}
	sirVirtualTexturePagePoolStats_t stats = pool.getStats();
	SIR_TEST( stats.nUploading == SIR_VT_PHYSICAL_COUNT );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 3, 0, 1 ) ) == 0 );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 3, 0, 2 ) ) == SIR_VT_PHYSICAL_COUNT );
	stats = pool.getStats();
	SIR_TEST( stats.nResident == SIR_VT_PHYSICAL_COUNT );
	sirGraphicsCompletionSet_t bad = makeCompleted( 6, 0, 5 );
	bad.nDeviceGeneration = 8;
	SIR_TEST( !pool.beginRetire( 0, bad ) );
	SIR_TEST( pool.serviceCompletions( bad ) == 0 );
	for ( uint32_t i = 0; i < SIR_VT_PHYSICAL_COUNT; ++i ) {
		SIR_TEST( pool.beginRetire( i, makeCompleted( 6, 4, 5 ) ) );
	}
	SIR_TEST( pool.serviceCompletions( makeCompleted( 6, 3, 5 ) ) == 0 );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 6, 4, 5 ) ) == SIR_VT_PHYSICAL_COUNT );
	stats = pool.getStats();
	SIR_TEST( stats.nFree == SIR_VT_PHYSICAL_COUNT && stats.nRetiring == 0 );
	SIR_TEST( pool.resetCold() );
	return 0;
}

static int testAncestorFallback( void )
{
	sirVirtualTexturePagePoolSlot_t slots[SIR_VT_PHYSICAL_COUNT] = {};
	uint32_t virtualMap[2] = {};
	SIRVirtualTexturePagePool pool;
	SIR_TEST( pool.init( slots, SIR_VT_PHYSICAL_COUNT, 1, virtualMap, 2, 7 ) );
	sirVirtualTexturePackPageRecord_t pages[2] = {};
	pages[0].nVirtualPage = 0;
	pages[0].nParentPage = 1;
	pages[0].nGeneration = 3;
	pages[0].nMip = 0;
	pages[1].nVirtualPage = 1;
	pages[1].nParentPage = UINT32_MAX;
	pages[1].nGeneration = 3;
	pages[1].nMip = 1;
	sirVirtualTexturePackView_t view = {};
	view.header.nPageCount = 2;
	view.szDirectories[SIR_VT_PACK_PAGES].pData = ( const uint8_t * )pages;
	view.szDirectories[SIR_VT_PACK_PAGES].nBytes = sizeof( pages );
	sirVirtualTexturePagePoolReservation_t ticket = {};
	SIR_TEST( pool.reserve( 1, SIR_VT_PHYSICAL_BC1, 3, &ticket ) );
	SIR_TEST( pool.commitUpload( ticket, makeCompleted( 0, 0, 2 ) ) );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 0, 0, 2 ) ) == 1 );
	uint32_t nResolvedPage = 999;
	uint16_t nResolvedMip = 999;
	uint32_t nResolvedSlot = 999;
	SIR_TEST( pool.resolveResidentAncestor( view, 0, 3, &nResolvedPage, &nResolvedMip, &nResolvedSlot ) );
	SIR_TEST( nResolvedPage == 1 && nResolvedMip == 1 && nResolvedSlot == ticket.nSlot );
	SIR_TEST( !pool.resolveResidentAncestor( view, 0, 4, &nResolvedPage, &nResolvedMip, &nResolvedSlot ) );
	pages[0].nParentPage = 0;
	SIR_TEST( !pool.resolveResidentAncestor( view, 0, 3, &nResolvedPage, &nResolvedMip, &nResolvedSlot ) );
	pages[0].nParentPage = 1;
	SIR_TEST( pool.beginRetire( 1, makeCompleted( 5, 0, 3 ) ) );
	SIR_TEST( !pool.resolveResidentAncestor( view, 0, 3, &nResolvedPage, &nResolvedMip, &nResolvedSlot ) );
	SIR_TEST( pool.serviceCompletions( makeCompleted( 5, 0, 3 ) ) == 1 );
	SIR_TEST( pool.resetCold() );
	return 0;
}

int main( void )
{
	if ( testQueueSafeLifecycle() || testGenerationAndIndependentFamilies() || testAncestorFallback() ) {
		return 1;
	}
	printf( "Terrain3D V1 queue-safe format-family page pool PASS\n" );
	return 0;
}
