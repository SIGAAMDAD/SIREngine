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

#include <World/HLOD/SIRWorldHLODGRDG.h>

#include <Resources/SIRResourceGraph.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <malloc.h>
#endif

#define SIR_HLOD_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "World HLOD GRDG failed: %s at line %d\n", #expression, __LINE__ );                                           \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static void *sirHLODTestAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nDomain;
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
#ifdef _WIN32
	return _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	void *pResult = nullptr;
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	return posix_memalign( &pResult, nActualAlignment, ( size_t )nBytes ) == 0 ? pResult : nullptr;
#endif
}

static void sirHLODTestFree( void *pContext, void *pMemory, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
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
	config.m_Allocator = { nullptr, sirHLODTestAllocate, sirHLODTestFree };
	config.m_nMetadataDomain = 0;
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 8;
	config.m_nMaxProviderPins = 8;
	config.m_nMaxCommittedBytes = 1024 * 1024;
	SIRResourceGraph graph( config );
	SIR_HLOD_CHECK( graph.isReady() );
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	SIR_HLOD_CHECK( pWriter != nullptr );

	sirWorldHLODNode_t szNodes[] = { { { 1, 3, 0 }, 77, 1, 2 }, { { 2, 3, 0 }, 77, 0, 0 }, { { 3, 3, 0 }, 77, 0, 0 } };
	sirWorldHLODProviderReady_t szProvider[] = { { 77, 1, 9, 12, true }, { 77, 1, 9, 12, true }, { 77, 1, 9, 12, true } };
	sirWorldHLODResidency_t szResident[3] = {};
	const sirWorldHLODGRDGOutput_t output = { szResident, 3 };
	SIRResourceReadScope scope( graph );
	SIR_HLOD_CHECK( scope.isActive() );
	sirWorldHLODGRDGInput_t input = { &graph, &scope, szNodes, szProvider, 3, 9, 12 };

	// GRDG source residency must be present, and wrong kinds never qualify.
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( !szResident[0].bReady && !szResident[1].bReady && !szResident[2].bReady );
	SIRResourcePublication root = { szNodes[0].keyGeometry, SIR_RESOURCE_KIND_GEOMETRY, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourcePublication child = { szNodes[1].keyGeometry, SIR_RESOURCE_KIND_GEOMETRY, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourceHandle hRoot = {};
	SIRResourceHandle hChild = {};
	SIR_HLOD_CHECK( pWriter->publish( root, &hRoot ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( pWriter->publish( child, &hChild ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( szResident[0].bReady && szResident[1].bReady && !szResident[2].bReady );

	float szErrors[] = { 100.0f, 0.0f, 0.0f };
	sirWorldHLODInput_t selection = { szNodes, szResident, szErrors, 3, 9, 10.0f };
	sirWorldHLODScratch_t szScratch[3] = {};
	uint32_t szSelected[3] = {};
	sirWorldHLODDemand_t szDemands[3] = {};
	uint32_t nSelected = 0;
	uint32_t nDemands = 0;
	sirWorldHLODOutput_t cut = { szScratch, 3, szSelected, 3, &nSelected, szDemands, 3, &nDemands };
	SIR_HLOD_CHECK( sirWorldHLODPlan( selection, cut ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 && nDemands == 1 && szDemands[0].nNodeIndex == 2 );

	SIR_HLOD_CHECK( pWriter->publish( root, &hRoot ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( !szResident[0].bReady && szResident[1].bReady );
	szProvider[0].nResourceRevision = 2;
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( szResident[0].bReady );

	szProvider[0].nDeviceGeneration = 11;
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( !szResident[0].bReady );
	szProvider[0].nDeviceGeneration = 12;
	szProvider[0].nSourceRevision = 76;
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( !szResident[0].bReady );
	szProvider[0].nSourceRevision = 77;
	szProvider[0].nWorldGeneration = 8;
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( !szResident[0].bReady );
	szProvider[0].nWorldGeneration = 9;
	szProvider[0].bReady = false;
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( !szResident[0].bReady );
	szProvider[0].bReady = true;

	// Invalid buffers/source metadata never overwrite the caller's output.
	output.pResidency[0] = { 123, 123, true };
	const sirWorldHLODGRDGOutput_t undersized = { szResident, 2 };
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, undersized ) == SIR_WORLD_HLOD_INVALID_ARGUMENT );
	SIR_HLOD_CHECK( szResident[0].nSourceRevision == 123 );
	szNodes[1].keyGeometry.m_nAssetId = 0;
	SIR_HLOD_CHECK( sirWorldHLODSnapshotGRDG( input, output ) == SIR_WORLD_HLOD_INVALID_HIERARCHY );
	SIR_HLOD_CHECK( szResident[0].nSourceRevision == 123 );
	return 0;
}
