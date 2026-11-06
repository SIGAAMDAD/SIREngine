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

#include <World/HLOD/SIRWorldHLODPins.h>
#include <Resources/SIRResourceGraph.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <malloc.h>
#endif

#define SIR_HLOD_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "HLOD pin failure: %s at line %d\n", #expression, __LINE__ );                                                 \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static void *sirHLODPinAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nDomain;
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
#ifdef _WIN32
	return _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	void *pMemory = nullptr;
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	return posix_memalign( &pMemory, nActualAlignment, ( size_t )nBytes ) == 0 ? pMemory : nullptr;
#endif
}

static void sirHLODPinFree( void *pContext, void *pMemory, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
#ifdef _WIN32
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

int main( void )
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, sirHLODPinAllocate, sirHLODPinFree };
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 8;
	config.m_nMaxProviderPins = 2;
	config.m_nMaxCommittedBytes = 1024 * 1024;

	SIRResourceGraph graph( config );
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	SIRResourceReadScope scope( graph );
	SIR_HLOD_CHECK( graph.isReady() && pWriter != nullptr && scope.isActive() );

	sirWorldHLODNode_t szNodes[] = {
	    { { 11, 3, 0 }, 77, 1, 2 },
	    { { 12, 3, 0 }, 78, 0, 0 },
	    { { 13, 3, 0 }, 79, 0, 0 },
	};
	sirWorldHLODProviderReady_t szReady[] = {
	    { 77, 1, 9, 13, true },
	    { 78, 1, 9, 13, true },
	    { 79, 1, 9, 13, true },
	};
	SIRResourcePublication root = { szNodes[0].keyGeometry, SIR_RESOURCE_KIND_GEOMETRY, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourcePublication first = { szNodes[1].keyGeometry, SIR_RESOURCE_KIND_GEOMETRY, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourcePublication second = { szNodes[2].keyGeometry, SIR_RESOURCE_KIND_GEOMETRY, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourceHandle hResource = {};
	SIR_HLOD_CHECK( pWriter->publish( root, &hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( pWriter->publish( first, &hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( pWriter->publish( second, &hResource ) == SIR_RESOURCE_RESULT_SUCCESS );

	// Produce the selected cut from actual GRDG readiness, rather than a hardcoded list.
	const sirWorldHLODGRDGInput_t snapshot = { &graph, &scope, szNodes, szReady, 3, 9, 13 };
	sirWorldHLODResidency_t szResidency[3] = {};
	const sirWorldHLODGRDGOutput_t ready = { szResidency, 3 };
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( snapshot, ready ) == SIR_WORLD_HLOD_OK );
	float szErrors[] = { 50.0f, 0.0f, 0.0f };
	sirWorldHLODScratch_t szCutScratch[3] = {};
	uint32_t szSelected[3] = {};
	sirWorldHLODDemand_t szDemands[3] = {};
	uint32_t nSelected = 0;
	uint32_t nDemands = 0;
	const sirWorldHLODInput_t cutInput = { szNodes, szResidency, szErrors, 3, 9, 10.0f };
	sirWorldHLODOutput_t cut = { szCutScratch, 3, szSelected, 3, &nSelected, szDemands, 3, &nDemands };
	SIR_HLOD_CHECK( sirWorldHLODPlan( cutInput, cut ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 2 && szSelected[0] == 1 && szSelected[1] == 2 && nDemands == 0 );

	SIRResourcePinHandle szPins[2] = { { 99, 99 }, { 99, 99 } };
	SIRResourcePinHandle szScratch[2] = {};
	uint32_t nPinned = 123;
	sirWorldHLODPinInput_t input = { snapshot, szSelected, nSelected, 17 };
	sirWorldHLODPinOutput_t output = { szPins, szScratch, 2, &nPinned };

	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_OK );
	SIR_HLOD_CHECK( nPinned == 2 && szPins[0].m_nSlot != szPins[1].m_nSlot );
	SIR_HLOD_CHECK( graph.releaseProviderPin( szPins[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( graph.releaseProviderPin( szPins[1] ) == SIR_RESOURCE_RESULT_SUCCESS );

	// The second pin failing must release the first, with output untouched.
	SIRResourceHandle hFirst = {};
	SIRResourceView firstView;
	SIRResourcePinHandle hOccupied = {};
	SIR_HLOD_CHECK( graph.find( scope, szNodes[1].keyGeometry, &hFirst ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( graph.resolve( scope, hFirst, &firstView ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( graph.pinForProvider( scope, firstView, input.nProviderId, &hOccupied ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_PROVIDER_FAILURE );
	SIR_HLOD_CHECK( nPinned == 2 );
	SIR_HLOD_CHECK( graph.releaseProviderPin( hOccupied ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_OK );
	SIR_HLOD_CHECK( graph.releaseProviderPin( szPins[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( graph.releaseProviderPin( szPins[1] ) == SIR_RESOURCE_RESULT_SUCCESS );

	// Stale second resource after first pin must unwind, never expose one pin.
	SIR_HLOD_CHECK( pWriter->publish( second, &hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
	nPinned = 123;
	szPins[0] = { 99, 99 };
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_STALE_RESIDENCY );
	SIR_HLOD_CHECK( nPinned == 123 && szPins[0].m_nSlot == 99 );
	szReady[2].nResourceRevision = 2;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_OK );
	SIR_HLOD_CHECK( graph.releaseProviderPin( szPins[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( graph.releaseProviderPin( szPins[1] ) == SIR_RESOURCE_RESULT_SUCCESS );

	// World/device epoch and source fingerprint mismatches reject selection.
	szReady[2].nWorldGeneration = 8;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_STALE_RESIDENCY );
	szReady[2].nWorldGeneration = 9;
	szReady[1].nDeviceGeneration = 14;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_STALE_RESIDENCY );
	szReady[1].nDeviceGeneration = 13;
	szReady[2].nSourceRevision = 100;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_STALE_RESIDENCY );
	szReady[2].nSourceRevision = 79;

	// Invalid cuts and insufficient space cannot touch output or acquire pins.
	szSelected[1] = 1;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_INVALID_CUT );
	szSelected[1] = 0;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_INVALID_CUT );
	szSelected[1] = 3;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_INVALID_CUT );
	szSelected[1] = 2;
	output.nCapacity = 1;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_CAPACITY );
	output.nCapacity = 2;
	input.nProviderId = 0;
	SIR_HLOD_CHECK( sirWorldHLODPinCut( input, output ) == SIR_WORLD_HLOD_PIN_INVALID_ARGUMENT );
	SIR_HLOD_CHECK( nPinned == 2 );
	return 0;
}
