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

#include <Mesh/SIRMeshCache.h>
#include <Core/Platform/SIRAtomic.h>
#include <Core/Platform/SIRThread.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum sirMeshOwnerFixtureLimit_e : uint32_t
{
	SIR_MESH_OWNER_WORKERS = 8,
	SIR_MESH_OWNER_ITERATIONS = 48
} sirMeshOwnerFixtureLimit_t;

typedef struct sirMeshOwnerWorker_s
{
	SIRAtomic<uint32_t> *pReady;
	SIRAtomic<bool> *pStart;
	uint64_t szOwnerIds[SIR_MESH_OWNER_ITERATIONS];
	uint32_t nReleases;
	bool bPassed;
} sirMeshOwnerWorker_t;

static void releaseSourcePin( void *pPin ) noexcept
{
	uint32_t *pReleases = ( uint32_t * )pPin;
	++*pReleases;
}

typedef struct sirMeshReentryProbe_s
{
	SIRMeshCache *pCache;
	void *pStorage;
	uint64_t nStorageBytes;
	uint32_t *pReleases;
	sirMeshResult_t eReinitialize;
} sirMeshReentryProbe_t;

static void releaseSourcePinWithReentry( void *pContext ) noexcept
{
	sirMeshReentryProbe_t *pProbe = ( sirMeshReentryProbe_t * )pContext;
	++*pProbe->pReleases;
	pProbe->eReinitialize = pProbe->pCache->initialize( pProbe->pStorage, pProbe->nStorageBytes, 1 );
}

static void runOwnerWorker( void *pContext ) noexcept
{
	sirMeshOwnerWorker_t *pWorker = ( sirMeshOwnerWorker_t * )pContext;
	pWorker->bPassed = true;
	pWorker->pReady->fetchAdd( 1, SIR_MEMORY_ORDER_RELEASE );
	while ( !pWorker->pStart->load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
		// A bounded thread-count fixture; each worker starts together.
	}

	const float szPositions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	alignas( 64 ) uint8_t szStorage[4096] = {};

	for ( uint32_t i = 0; i < SIR_MESH_OWNER_ITERATIONS; ++i ) {
		SIRMeshCache cache;
		if ( i == 0 && cache.initialize( nullptr, sizeof( szStorage ), 1 ) != SIR_MESH_INVALID_ARGUMENT ) {
			pWorker->bPassed = false;
			return;
		}
		if ( i == 0 && cache.initialize( szStorage, 0, 1 ) != SIR_MESH_INVALID_ARGUMENT ) {
			pWorker->bPassed = false;
			return;
		}
		if ( cache.initialize( szStorage, sizeof( szStorage ), 1 ) != SIR_MESH_OK ) {
			pWorker->bPassed = false;
			return;
		}
		if ( i == 0 ) {
			sirMeshHandle_t missing = {};
			if ( cache.insert( nullptr, &missing ) != SIR_MESH_INVALID_ARGUMENT ) {
				pWorker->bPassed = false;
				return;
			}
		}
		sirMeshGeometrySource_t source = {};
		source.key = { 1, 1, 0, 0 };
		source.pPositionsXYZ = szPositions;
		source.pTriangleIndices = szIndices;
		source.nVertexCount = 3;
		source.nIndexCount = 3;
		sirMeshReentryProbe_t probe = {};
		probe.pCache = &cache;
		probe.pStorage = szStorage;
		probe.nStorageBytes = sizeof( szStorage );
		probe.pReleases = &pWorker->nReleases;
		source.pSourcePin = i == 0 ? ( void * )&probe : ( void * )&pWorker->nReleases;
		source.pfnReleaseSourcePin = i == 0 ? releaseSourcePinWithReentry : releaseSourcePin;
		if ( i == 0 ) {
			source.pTriangleIndices = nullptr;
			sirMeshHandle_t invalid = {};
			if ( cache.insert( &source, &invalid ) != SIR_MESH_INVALID_ARGUMENT ) {
				pWorker->bPassed = false;
				return;
			}
			source.pTriangleIndices = szIndices;
		}
		sirMeshHandle_t handle = {};
		if ( cache.insert( &source, &handle ) != SIR_MESH_OK || handle.nOwnerId == 0 ) {
			pWorker->bPassed = false;
			( void )cache.shutdown();
			return;
		}
		pWorker->szOwnerIds[i] = handle.nOwnerId;
		if ( i == 0 ) {
			sirMeshHandle_t staleSlot = handle;
			staleSlot.nSlot = 1;
			sirMeshGeometryRead_t read = {};
			if ( cache.acquire( staleSlot, &read ) != SIR_MESH_STALE_HANDLE ) {
				pWorker->bPassed = false;
				return;
			}
			sirMeshKey_t differentReserved = source.key;
			differentReserved.nReserved = 1;
			sirMeshHandle_t other = {};
			if ( cache.find( &differentReserved, &other ) != SIR_MESH_NOT_FOUND ) {
				pWorker->bPassed = false;
				return;
			}
		}
		if ( i == 0 ) {
			if ( cache.shutdown() != SIR_MESH_OK || probe.eReinitialize != SIR_MESH_BUSY || pWorker->nReleases != i + 1 ) {
				pWorker->bPassed = false;
				return;
			}
		} else if ( cache.retire( handle ) != SIR_MESH_OK || cache.shutdown() != SIR_MESH_OK || pWorker->nReleases != i + 1 ) {
			pWorker->bPassed = false;
			return;
		}
	}
}

int main( void )
{
	SIRAtomic<uint32_t> ready( 0 );
	SIRAtomic<bool> start( false );
	SIRThread szThreads[SIR_MESH_OWNER_WORKERS];
	sirMeshOwnerWorker_t szWorkers[SIR_MESH_OWNER_WORKERS] = {};
	uint32_t nStarted = 0;
	for ( uint32_t i = 0; i < SIR_MESH_OWNER_WORKERS; ++i ) {
		szWorkers[i].pReady = &ready;
		szWorkers[i].pStart = &start;
		if ( !szThreads[i].start( runOwnerWorker, &szWorkers[i] ) ) {
			break;
		}
		++nStarted;
	}

	while ( ready.load( SIR_MEMORY_ORDER_ACQUIRE ) != nStarted ) {
	}
	start.store( true, SIR_MEMORY_ORDER_RELEASE );
	for ( uint32_t i = 0; i < nStarted; ++i ) {
		if ( !szThreads[i].join() ) {
			fputs( "mesh owner worker join failed\n", stderr );
			abort();
		}
	}
	if ( nStarted != SIR_MESH_OWNER_WORKERS ) {
		fputs( "mesh owner worker creation failed\n", stderr );
		return 1;
	}

	for ( uint32_t i = 0; i < SIR_MESH_OWNER_WORKERS; ++i ) {
		if ( !szWorkers[i].bPassed || szWorkers[i].nReleases != SIR_MESH_OWNER_ITERATIONS ) {
			fputs( "mesh owner worker failed lifecycle\n", stderr );
			return 1;
		}
		for ( uint32_t iEpoch = 0; iEpoch < SIR_MESH_OWNER_ITERATIONS; ++iEpoch ) {
			const uint64_t nOwnerId = szWorkers[i].szOwnerIds[iEpoch];
			for ( uint32_t iPriorWorker = 0; iPriorWorker <= i; ++iPriorWorker ) {
				const uint32_t nLimit = iPriorWorker == i ? iEpoch : SIR_MESH_OWNER_ITERATIONS;
				for ( uint32_t iPriorEpoch = 0; iPriorEpoch < nLimit; ++iPriorEpoch ) {
					if ( nOwnerId == szWorkers[iPriorWorker].szOwnerIds[iPriorEpoch] ) {
						fputs( "mesh owner ID reused across threads or epochs\n", stderr );
						return 1;
					}
				}
			}
		}
	}
	return 0;
}
