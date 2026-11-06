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

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool s_bMeasureAllocations = false;
static uint32_t s_nAllocationCalls = 0;

// Catch both C++ allocations and direct C heap usage made by the cache.
// --wrap is enabled for this test on ELF/Linux; C++ hooks remain portable.
#if defined( __linux__ )
extern "C" void *__real_malloc( size_t nBytes ) noexcept;
extern "C" void *__real_calloc( size_t nCount, size_t nBytes ) noexcept;
extern "C" void *__real_realloc( void *pStorage, size_t nBytes ) noexcept;

extern "C" void *__wrap_malloc( size_t nBytes ) noexcept
{
	if ( s_bMeasureAllocations ) {
		++s_nAllocationCalls;
	}
	return __real_malloc( nBytes );
}

extern "C" void *__wrap_calloc( size_t nCount, size_t nBytes ) noexcept
{
	if ( s_bMeasureAllocations ) {
		++s_nAllocationCalls;
	}
	return __real_calloc( nCount, nBytes );
}

extern "C" void *__wrap_realloc( void *pStorage, size_t nBytes ) noexcept
{
	if ( s_bMeasureAllocations ) {
		++s_nAllocationCalls;
	}
	return __real_realloc( pStorage, nBytes );
}
#endif

void *operator new( size_t nBytes )
{
	if ( s_bMeasureAllocations ) {
		++s_nAllocationCalls;
	}
	void *pResult = malloc( nBytes );
	if ( pResult == nullptr ) {
		abort();
	}
	return pResult;
}

void *operator new[]( size_t nBytes )
{
	if ( s_bMeasureAllocations ) {
		++s_nAllocationCalls;
	}
	void *pResult = malloc( nBytes );
	if ( pResult == nullptr ) {
		abort();
	}
	return pResult;
}

static void ( *volatile s_pFreeMemory )( void * ) = free;

void operator delete( void *pStorage ) noexcept
{
	s_pFreeMemory( pStorage );
}

void operator delete[]( void *pStorage ) noexcept
{
	s_pFreeMemory( pStorage );
}

void operator delete( void *pStorage, size_t ) noexcept
{
	s_pFreeMemory( pStorage );
}

void operator delete[]( void *pStorage, size_t ) noexcept
{
	s_pFreeMemory( pStorage );
}

typedef struct sirMeshAllocationPin_s
{
	uint32_t nReleaseCount;
} sirMeshAllocationPin_t;

static void onSourcePinRelease( void *pSourcePin ) noexcept
{
	++( ( sirMeshAllocationPin_t * )pSourcePin )->nReleaseCount;
}

static void checkMeshAllocation( bool bCondition, const char *pszDescription ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "mesh allocation FAILED: %s\n", pszDescription );
		abort();
	}
}

static void beginAllocationMeasurement( void ) noexcept
{
	s_nAllocationCalls = 0;
	s_bMeasureAllocations = true;
}

static uint32_t endAllocationMeasurement( void ) noexcept
{
	s_bMeasureAllocations = false;
	return s_nAllocationCalls;
}

int main( void )
{
	// Confirm the test hooks detect allocations rather than returning zero
	// because they are disconnected from the actual link path.
	beginAllocationMeasurement();
	void *pProbe = ::operator new( 64 );
	const uint32_t nProbeCount = endAllocationMeasurement();
	::operator delete( pProbe );
	checkMeshAllocation( nProbeCount != 0, "C++ allocation hook is active" );

	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshAllocationPin_t szPins[3] = {};
	sirMeshGeometrySource_t first = { { 42, 1, 0, 0 }, szVertices, szIndices, 3, 3, &szPins[0], onSourcePinRelease };
	sirMeshGeometrySource_t second = { { 43, 1, 0, 0 }, szVertices, szIndices, 3, 3, &szPins[1], onSourcePinRelease };
	sirMeshGeometrySource_t third = { { 44, 1, 0, 0 }, szVertices, szIndices, 3, 3, &szPins[2], onSourcePinRelease };
	SIRMeshCache cache;
	sirMeshHandle_t firstHandle = {};
	sirMeshHandle_t secondHandle = {};
	sirMeshHandle_t unusedHandle = {};
	sirMeshGeometryRead_t read = {};

	beginAllocationMeasurement();
	const sirMeshResult_t eInit = cache.initialize( szStorage, sizeof( szStorage ), 2 );
	const sirMeshResult_t eFirst = cache.insert( &first, &firstHandle );
	const sirMeshResult_t eDuplicate = cache.insert( &first, &unusedHandle );
	const sirMeshResult_t eSecond = cache.insert( &second, &secondHandle );
	const sirMeshResult_t eFull = cache.insert( &third, &unusedHandle );
	const sirMeshResult_t eFind = cache.find( &first.key, &unusedHandle );
	const sirMeshResult_t eAcquire = cache.acquire( firstHandle, &read );
	const sirMeshResult_t eRetire = cache.retire( firstHandle );
	const sirMeshResult_t eBusy = cache.shutdown();
	const sirMeshResult_t eRelease = cache.release( firstHandle );
	const sirMeshResult_t eShutdown = cache.shutdown();
	const sirMeshResult_t eReinitialize = cache.initialize( szStorage, sizeof( szStorage ), 2 );
	const sirMeshResult_t eReuse = cache.insert( &third, &unusedHandle );
	const sirMeshResult_t eFinalShutdown = cache.shutdown();
	const uint32_t nAllocations = endAllocationMeasurement();

	checkMeshAllocation( eInit == SIR_MESH_OK && eFirst == SIR_MESH_OK && eSecond == SIR_MESH_OK && eFind == SIR_MESH_OK,
	                     "valid initialize/insert/find" );
	checkMeshAllocation( eDuplicate == SIR_MESH_DUPLICATE && eFull == SIR_MESH_OUT_OF_CAPACITY, "duplicate/capacity" );
	checkMeshAllocation( eAcquire == SIR_MESH_OK && eRetire == SIR_MESH_OK && eBusy == SIR_MESH_BUSY && eRelease == SIR_MESH_OK,
	                     "borrow and deferred retirement" );
	checkMeshAllocation( eShutdown == SIR_MESH_OK && eReinitialize == SIR_MESH_OK && eReuse == SIR_MESH_OK && eFinalShutdown == SIR_MESH_OK,
	                     "shutdown and reinitialization" );
	checkMeshAllocation( szPins[0].nReleaseCount == 1 && szPins[1].nReleaseCount == 1 && szPins[2].nReleaseCount == 1,
	                     "pin release exactly once" );
	checkMeshAllocation( nAllocations == 0, "cache performed a heap allocation" );
	return 0;
}
