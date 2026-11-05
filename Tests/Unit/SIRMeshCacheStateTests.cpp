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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static constexpr uint32_t SIR_MESH_TEST_CAPACITY = 6;
static constexpr uint32_t SIR_MESH_TEST_STEPS = 20000;

static const float s_szPositions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
static const uint32_t s_nIndices[3] = { 0, 1, 2 };

typedef struct sirMeshStatePin_s
{
	uint32_t nReleaseCount;
} sirMeshStatePin_t;

typedef struct sirMeshStateSlot_s
{
	sirMeshHandle_t handle;
	sirMeshKey_t key;
	sirMeshStatePin_t *pPin;
	uint32_t nBorrows;
	bool bOccupied;
	bool bRetiring;
} sirMeshStateSlot_t;

static void checkMeshState( bool bCondition, const char *pszMessage, uint32_t nStep ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "mesh state-model FAILED at step %u: %s\n", nStep, pszMessage );
		abort();
	}
}

static void retirePin( void *pContext ) noexcept
{
	sirMeshStatePin_t *pPin = ( sirMeshStatePin_t * )pContext;
	++pPin->nReleaseCount;
	checkMeshState( pPin->nReleaseCount == 1, "pin released more than once", 0 );
}

static uint32_t nextMeshRandom( uint32_t *pState ) noexcept
{
	uint32_t nValue = *pState;
	nValue ^= nValue << 13U;
	nValue ^= nValue >> 17U;
	nValue ^= nValue << 5U;
	*pState = nValue;
	return nValue;
}

static sirMeshGeometrySource_t makeMeshSource( const sirMeshKey_t& key, sirMeshStatePin_t *pPin ) noexcept
{
	return { key, s_szPositions, s_nIndices, 3, 3, pPin, retirePin };
}

static void checkCounters( SIRMeshCache& cache, const sirMeshStateSlot_t *pSlots, uint32_t nStep ) noexcept
{
	uint32_t nLive = 0;
	uint32_t nBorrows = 0;
	for ( uint32_t i = 0; i < SIR_MESH_TEST_CAPACITY; ++i ) {
		if ( pSlots[i].bOccupied ) {
			++nLive;
			nBorrows += pSlots[i].nBorrows;
			checkMeshState( pSlots[i].pPin->nReleaseCount == 0, "live pin prematurely released", nStep );
		}
	}
	checkMeshState( cache.getCapacity() == SIR_MESH_TEST_CAPACITY, "capacity changed", nStep );
	checkMeshState( cache.getResidentCount() == nLive, "resident count mismatch", nStep );
	checkMeshState( cache.getBorrowCount() == nBorrows, "borrow count mismatch", nStep );
}

static void testRandomizedMeshLifecycle( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	sirMeshStatePin_t szPins[SIR_MESH_TEST_STEPS] = {};
	sirMeshStateSlot_t szSlots[SIR_MESH_TEST_CAPACITY] = {};
	sirMeshHandle_t szPrevious[SIR_MESH_TEST_CAPACITY] = {};
	uint32_t szPreviousGeneration[SIR_MESH_TEST_CAPACITY] = {};
	uint32_t nSeed = 0xD15EA5E5U;
	uint32_t nSuccessfulInsertions = 0;
	uint32_t nRetiringWithBorrow = 0;
	uint32_t nEmptyRetirements = 0;
	uint32_t nBusyShutdowns = 0;
	uint32_t nSuccessfulShutdowns = 0;
	uint32_t nStaleProbes = 0;
	uint32_t nNextPin = 0;
	uint64_t nNextAssetId = 1;
	SIRMeshCache cache;
	checkMeshState( cache.initialize( szStorage, sizeof( szStorage ), SIR_MESH_TEST_CAPACITY ) == SIR_MESH_OK, "initialization", 0 );

	for ( uint32_t nStep = 0; nStep < SIR_MESH_TEST_STEPS; ++nStep ) {
		const uint32_t nRandom = nextMeshRandom( &nSeed );
		const uint32_t nSlot = ( nRandom >> 8U ) % SIR_MESH_TEST_CAPACITY;
		sirMeshStateSlot_t *pModel = &szSlots[nSlot];
		sirMeshGeometryRead_t read = {};
		sirMeshHandle_t output = {};
		const uint32_t nAction = nRandom % 8U;
		if ( nAction == 0U ) {
			checkMeshState( nNextPin < SIR_MESH_TEST_STEPS, "pin array exhausted", nStep );
			sirMeshKey_t key = { nNextAssetId++, 1, nStep & 7U, 0 };
			sirMeshStatePin_t *pPin = &szPins[nNextPin++];
			sirMeshGeometrySource_t source = makeMeshSource( key, pPin );
			const sirMeshResult_t eResult = cache.insert( &source, &output );
			uint32_t nLive = 0;
			for ( uint32_t i = 0; i < SIR_MESH_TEST_CAPACITY; ++i ) {
				nLive += szSlots[i].bOccupied ? 1U : 0U;
			}
			if ( nLive == SIR_MESH_TEST_CAPACITY ) {
				checkMeshState( eResult == SIR_MESH_OUT_OF_CAPACITY, "full insertion must fail", nStep );
			} else {
				checkMeshState( eResult == SIR_MESH_OK, "insertion must succeed", nStep );
				checkMeshState( output.nSlot < SIR_MESH_TEST_CAPACITY, "returned slot outside capacity", nStep );
				checkMeshState( !szSlots[output.nSlot].bOccupied, "overwrote occupied slot", nStep );
				checkMeshState( output.nGeneration != 0 && output.nGeneration > szPreviousGeneration[output.nSlot],
				                "generation not monotonic after slot reuse", nStep );
				szPreviousGeneration[output.nSlot] = output.nGeneration;
				szSlots[output.nSlot] = { output, key, pPin, 0, true, false };
				++nSuccessfulInsertions;
			}
			checkMeshState( pPin->nReleaseCount == 0, "failed insert took source pin", nStep );
		} else if ( nAction == 1U && pModel->bOccupied && !pModel->bRetiring ) {
			checkMeshState( cache.acquire( pModel->handle, &read ) == SIR_MESH_OK, "acquire", nStep );
			checkMeshState( read.pPositionsXYZ == s_szPositions && read.pTriangleIndices == s_nIndices, "borrowed wrong bytes", nStep );
			checkMeshState( read.key.nAssetId == pModel->key.nAssetId, "borrowed wrong revision", nStep );
			++pModel->nBorrows;
		} else if ( nAction == 2U && pModel->bOccupied && pModel->nBorrows != 0 ) {
			checkMeshState( cache.release( pModel->handle ) == SIR_MESH_OK, "release", nStep );
			--pModel->nBorrows;
			if ( pModel->nBorrows == 0 && pModel->bRetiring ) {
				checkMeshState( pModel->pPin->nReleaseCount == 1, "last release did not retire pin", nStep );
				szPrevious[nSlot] = pModel->handle;
				*pModel = {};
			}
		} else if ( nAction == 3U && pModel->bOccupied && !pModel->bRetiring ) {
			checkMeshState( cache.retire( pModel->handle ) == SIR_MESH_OK, "retire", nStep );
			if ( pModel->nBorrows == 0 ) {
				checkMeshState( pModel->pPin->nReleaseCount == 1, "idle retire did not release pin", nStep );
				szPrevious[nSlot] = pModel->handle;
				*pModel = {};
				++nEmptyRetirements;
			} else {
				checkMeshState( pModel->pPin->nReleaseCount == 0, "retiring pin freed during borrow", nStep );
				pModel->bRetiring = true;
				++nRetiringWithBorrow;
			}
		} else if ( nAction == 4U && pModel->bOccupied ) {
			const sirMeshResult_t eResult = cache.find( &pModel->key, &output );
			checkMeshState( eResult == ( pModel->bRetiring ? SIR_MESH_NOT_FOUND : SIR_MESH_OK ), "lookup status", nStep );
			if ( !pModel->bRetiring ) {
				checkMeshState( output.nSlot == pModel->handle.nSlot && output.nGeneration == pModel->handle.nGeneration,
				                "lookup returned wrong generation", nStep );
			} else {
				checkMeshState( cache.acquire( pModel->handle, &read ) == SIR_MESH_RETIRED, "retired borrow", nStep );
				checkMeshState( read.pPositionsXYZ == nullptr, "retired borrow exposed geometry", nStep );
			}
		} else if ( nAction == 5U ) {
			sirMeshHandle_t stale = { UINT32_MAX, UINT32_MAX };
			if ( szPrevious[nSlot].nGeneration != 0 ) {
				stale = szPrevious[nSlot];
			}
			checkMeshState( cache.acquire( stale, &read ) == SIR_MESH_STALE_HANDLE, "stale acquire", nStep );
			checkMeshState( cache.retire( stale ) == SIR_MESH_STALE_HANDLE, "stale retirement", nStep );
			++nStaleProbes;
		} else if ( nAction == 6U ) {
			uint32_t nBorrowed = 0;
			for ( uint32_t i = 0; i < SIR_MESH_TEST_CAPACITY; ++i ) {
				nBorrowed += szSlots[i].nBorrows;
			}
			if ( nBorrowed != 0 ) {
				checkMeshState( cache.shutdown() == SIR_MESH_BUSY, "shutdown during borrows", nStep );
				++nBusyShutdowns;
			} else {
				checkMeshState( cache.shutdown() == SIR_MESH_OK, "unborrowed shutdown", nStep );
				for ( uint32_t i = 0; i < SIR_MESH_TEST_CAPACITY; ++i ) {
					if ( szSlots[i].bOccupied ) {
						checkMeshState( szSlots[i].pPin->nReleaseCount == 1, "shutdown did not retire pin", nStep );
						szPrevious[i] = szSlots[i].handle;
						szSlots[i] = {};
					}
				}
				checkMeshState( cache.initialize( szStorage, sizeof( szStorage ), SIR_MESH_TEST_CAPACITY ) == SIR_MESH_OK,
				                "reinitialize after shutdown", nStep );
				++nSuccessfulShutdowns;
			}
		} else if ( nAction == 7U && pModel->bOccupied ) {
			sirMeshGeometrySource_t source = makeMeshSource( pModel->key, pModel->pPin );
			checkMeshState( cache.insert( &source, &output ) == SIR_MESH_DUPLICATE, "duplicate key accepted", nStep );
		}
		checkCounters( cache, szSlots, nStep );
	}

	for ( uint32_t i = 0; i < SIR_MESH_TEST_CAPACITY; ++i ) {
		while ( szSlots[i].bOccupied && szSlots[i].nBorrows != 0 ) {
			checkMeshState( cache.release( szSlots[i].handle ) == SIR_MESH_OK, "drain final borrows", SIR_MESH_TEST_STEPS );
			--szSlots[i].nBorrows;
			if ( szSlots[i].bRetiring && szSlots[i].nBorrows == 0 ) {
				checkMeshState( szSlots[i].pPin->nReleaseCount == 1, "final drain pin", SIR_MESH_TEST_STEPS );
				szSlots[i] = {};
			}
		}
	}
	checkMeshState( cache.shutdown() == SIR_MESH_OK, "final shutdown", SIR_MESH_TEST_STEPS );
	for ( uint32_t i = 0; i < SIR_MESH_TEST_CAPACITY; ++i ) {
		if ( szSlots[i].bOccupied ) {
			checkMeshState( szSlots[i].pPin->nReleaseCount == 1, "final pin release", SIR_MESH_TEST_STEPS );
		}
	}
	checkMeshState( nSuccessfulInsertions > 100 && nRetiringWithBorrow > 0 && nEmptyRetirements > 0 && nBusyShutdowns > 0 &&
	                    nSuccessfulShutdowns > 0 && nStaleProbes > 0,
	                "insufficient random-path coverage", SIR_MESH_TEST_STEPS );
	fprintf( stdout, "mesh model: %u inserts, %u borrowed retirements, %u idle retirements, %u busy shutdowns, %u shutdowns\n",
	         nSuccessfulInsertions, nRetiringWithBorrow, nEmptyRetirements, nBusyShutdowns, nSuccessfulShutdowns );
}

int main( void )
{
	testRandomizedMeshLifecycle();
	return 0;
}
