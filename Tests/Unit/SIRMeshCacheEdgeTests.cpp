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

#include <limits>
#include <new>
#include <stddef.h>
#include <type_traits>
#include <stdio.h>
#include <stdlib.h>

static_assert( sizeof( sirMeshHandle_t ) == 16, "Mesh handle ABI requires 128 bits" );
static_assert( offsetof( sirMeshHandle_t, nOwnerId ) == 8, "Mesh handle owner ID offset must be stable" );
static_assert( std::is_trivially_copyable<sirMeshHandle_t>::value, "Mesh handle must remain POD" );
static_assert( sizeof( sirMeshKey_t ) == 32, "Serialized mesh key must remain independent of runtime handle" );

typedef struct sirMeshEdgePin_s
{
	uint32_t nReleased;
} sirMeshEdgePin_t;

typedef struct sirMeshEdgeReentry_s
{
	SIRMeshCache *pCache;
	void *pStorage;
	uint64_t nStorageBytes;
	sirMeshHandle_t handle;
	sirMeshResult_t eInitialize;
	sirMeshResult_t eShutdown;
	sirMeshResult_t eFind;
	sirMeshResult_t eInsert;
	uint32_t nCallbacks;
} sirMeshEdgeReentry_t;

static const float s_szPositions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
static const uint32_t s_szIndices[3] = { 0, 1, 2 };

static void requireMesh( bool bPass, const char *pszDescription ) noexcept
{
	if ( !bPass ) {
		fprintf( stderr, "FAILED: %s\n", pszDescription );
		abort();
	}
}

static void onMeshPinRelease( void *pValue ) noexcept
{
	sirMeshEdgePin_t *pPin = ( sirMeshEdgePin_t * )pValue;
	++pPin->nReleased;
}

static void onMeshReentryRelease( void *pValue ) noexcept
{
	sirMeshEdgeReentry_t *pProbe = ( sirMeshEdgeReentry_t * )pValue;
	++pProbe->nCallbacks;
	const float szPositions[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshEdgePin_t temporary = {};
	sirMeshGeometrySource_t source = { { 999, 1, 0, 0 }, szPositions, szIndices, 3, 3, &temporary, onMeshPinRelease };
	sirMeshHandle_t output = {};
	pProbe->eInitialize = pProbe->pCache->initialize( pProbe->pStorage, pProbe->nStorageBytes, 2 );
	pProbe->eShutdown = pProbe->pCache->shutdown();
	pProbe->eFind = pProbe->pCache->find( &source.key, &output );
	pProbe->eInsert = pProbe->pCache->insert( &source, &output );
	requireMesh( temporary.nReleased == 0, "reentrant insert did not take pin" );
}

static sirMeshGeometrySource_t makeMesh( sirMeshEdgePin_t *pPin, uint64_t nAssetId, uint32_t nVariant ) noexcept
{
	return { { nAssetId, 1, nVariant, 0 }, s_szPositions, s_szIndices, 3, 3, pPin, onMeshPinRelease };
}

static void testMeshInvalidSources( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRMeshCache cache;
	sirMeshHandle_t handle = {};
	sirMeshGeometryRead_t read = {};
	sirMeshEdgePin_t owner = {};
	sirMeshGeometrySource_t source = makeMesh( &owner, 7, 0 );
	requireMesh( cache.getCapacity() == 0 && cache.getBorrowCount() == 0, "initial accounting" );
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_NOT_READY, "insert before init" );
	requireMesh( cache.find( &source.key, &handle ) == SIR_MESH_NOT_READY, "find before init" );
	requireMesh( cache.acquire( handle, &read ) == SIR_MESH_NOT_READY, "acquire before init" );
	requireMesh( cache.release( handle ) == SIR_MESH_NOT_READY, "release before init" );
	requireMesh( cache.retire( handle ) == SIR_MESH_NOT_READY, "retire before init" );
	requireMesh( cache.shutdown() == SIR_MESH_NOT_READY, "shutdown before init" );
	requireMesh( cache.initialize( nullptr, sizeof( szStorage ), 2 ) == SIR_MESH_INVALID_ARGUMENT, "null storage" );
	requireMesh( cache.initialize( szStorage, sizeof( szStorage ), 0 ) == SIR_MESH_INVALID_ARGUMENT, "zero capacity" );
	requireMesh( cache.initialize( szStorage, 1, 2 ) == SIR_MESH_INVALID_ARGUMENT, "small storage" );
	requireMesh( cache.initialize( szStorage + 1, sizeof( szStorage ) - 1, 2 ) == SIR_MESH_INVALID_ARGUMENT, "misaligned storage" );
	requireMesh( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize valid" );
	requireMesh( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_BUSY, "already initialized" );
	requireMesh( cache.insert( nullptr, &handle ) == SIR_MESH_INVALID_ARGUMENT, "null source" );
	requireMesh( cache.insert( &source, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "null output handle" );
	const sirMeshGeometrySource_t original = source;
	source.key.nAssetId = 0;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "zero asset ID" );
	source = original;
	source.key.nRevision = 0;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "zero revision" );
	source = original;
	source.key.nReserved = 1;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "reserved key bytes" );
	source = original;
	source.pPositionsXYZ = nullptr;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "null positions" );
	source = original;
	source.pTriangleIndices = nullptr;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "null indices" );
	source = original;
	source.pSourcePin = nullptr;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "null pin" );
	source = original;
	source.pfnReleaseSourcePin = nullptr;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "null pin callback" );
	source = original;
	source.nVertexCount = 0;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "zero vertices" );
	source = original;
	source.nIndexCount = 0;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "zero indices" );
	source = original;
	source.nIndexCount = 2;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "index count not divisible by 3" );
	source = original;
	source.nVertexCount = UINT32_MAX;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "vertex count overflow" );
	source = original;
	const uint32_t szOutOfBounds[3] = { 0, 1, 3 };
	source.pTriangleIndices = szOutOfBounds;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "out-of-bounds triangle" );
	source = original;
	float szNonfinite[9] = {};
	for ( uint32_t i = 0; i < 9; ++i ) {
		szNonfinite[i] = s_szPositions[i];
	}
	szNonfinite[8] = std::numeric_limits<float>::quiet_NaN();
	source.pPositionsXYZ = szNonfinite;
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "NaN vertex" );
	szNonfinite[8] = std::numeric_limits<float>::infinity();
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "infinite vertex" );
	source = original;
	requireMesh( owner.nReleased == 0 && cache.getResidentCount() == 0, "failed source leaves ownership with caller" );
	requireMesh( cache.find( nullptr, &handle ) == SIR_MESH_INVALID_ARGUMENT, "null lookup key" );
	requireMesh( cache.find( &source.key, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "null lookup handle" );
	requireMesh( cache.find( &source.key, &handle ) == SIR_MESH_NOT_FOUND, "missing key" );
	requireMesh( handle.nGeneration == 0, "failed lookup clears handle" );
	requireMesh( cache.acquire( handle, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "null read output" );
	requireMesh( cache.acquire( handle, &read ) == SIR_MESH_STALE_HANDLE, "invalid handle" );
	requireMesh( read.pPositionsXYZ == nullptr, "failed borrow clears output" );
	requireMesh( cache.release( handle ) == SIR_MESH_STALE_HANDLE, "invalid release" );
	requireMesh( cache.retire( handle ) == SIR_MESH_STALE_HANDLE, "invalid retirement" );
	requireMesh( cache.shutdown() == SIR_MESH_OK, "shutdown empty cache" );
}

static void testMeshKeysAndLeases( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRMeshCache cache;
	sirMeshEdgePin_t szPins[3] = {};
	sirMeshGeometrySource_t source = makeMesh( &szPins[0], 3, 4 );
	sirMeshHandle_t handle = {};
	sirMeshHandle_t another = {};
	sirMeshGeometryRead_t read = {};
	requireMesh( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize key fixture" );
	requireMesh( cache.insert( &source, &handle ) == SIR_MESH_OK, "insert first" );
	requireMesh( cache.insert( &source, &another ) == SIR_MESH_DUPLICATE, "same key rejected" );
	requireMesh( cache.find( &source.key, &another ) == SIR_MESH_OK, "find first" );
	requireMesh( another.nGeneration == handle.nGeneration, "find generation" );
	sirMeshKey_t key = source.key;
	key.nAssetId = 4;
	requireMesh( cache.find( &key, &another ) == SIR_MESH_NOT_FOUND, "different asset" );
	key = source.key;
	key.nRevision = 2;
	requireMesh( cache.find( &key, &another ) == SIR_MESH_NOT_FOUND, "different revision" );
	key = source.key;
	key.nVariant = 5;
	requireMesh( cache.find( &key, &another ) == SIR_MESH_NOT_FOUND, "different variant" );
	requireMesh( cache.acquire( handle, &read ) == SIR_MESH_OK, "borrow first" );
	requireMesh( cache.acquire( handle, &read ) == SIR_MESH_OK, "borrow twice" );
	requireMesh( cache.getBorrowCount() == 2, "two readers" );
	requireMesh( cache.shutdown() == SIR_MESH_BUSY, "shutdown must retain leased pin" );
	requireMesh( cache.retire( handle ) == SIR_MESH_OK, "retire borrowed source" );
	requireMesh( cache.retire( handle ) == SIR_MESH_RETIRED, "double retire" );
	requireMesh( cache.acquire( handle, &read ) == SIR_MESH_RETIRED, "read retired source" );
	requireMesh( cache.find( &source.key, &another ) == SIR_MESH_NOT_FOUND, "retired source invisible" );
	requireMesh( szPins[0].nReleased == 0, "defer release while readers remain" );
	requireMesh( cache.release( handle ) == SIR_MESH_OK, "release first reader" );
	requireMesh( szPins[0].nReleased == 0, "first release does not retire" );
	requireMesh( cache.release( handle ) == SIR_MESH_OK, "release last reader" );
	requireMesh( szPins[0].nReleased == 1, "last reader releases pin" );
	requireMesh( cache.release( handle ) == SIR_MESH_STALE_HANDLE, "stale released handle" );
	source = makeMesh( &szPins[1], 3, 4 );
	requireMesh( cache.insert( &source, &another ) == SIR_MESH_OK, "reuse slot" );
	requireMesh( another.nGeneration != handle.nGeneration, "slot generation updated" );
	requireMesh( cache.acquire( handle, &read ) == SIR_MESH_STALE_HANDLE, "ABA rejection" );
	requireMesh( cache.release( another ) == SIR_MESH_INVALID_ARGUMENT, "release without acquire" );
	const sirMeshHandle_t outOfRange = { 800, 1, another.nOwnerId };
	requireMesh( cache.acquire( outOfRange, &read ) == SIR_MESH_STALE_HANDLE, "out-of-range slot" );
	const sirMeshHandle_t emptySlot = { 1, 999, another.nOwnerId };
	requireMesh( cache.acquire( emptySlot, &read ) == SIR_MESH_STALE_HANDLE, "empty slot" );
	const sirMeshHandle_t wrongGeneration = { another.nSlot, another.nGeneration + 1, another.nOwnerId };
	requireMesh( cache.acquire( wrongGeneration, &read ) == SIR_MESH_STALE_HANDLE, "wrong generation" );
	sirMeshGeometrySource_t second = makeMesh( &szPins[2], 3, 5 );
	requireMesh( cache.insert( &second, &handle ) == SIR_MESH_OK, "insert different variant" );
	requireMesh( cache.getResidentCount() == 2, "two resident sources" );
	const sirMeshKey_t missing = { 9000, 1, 0, 0 };
	requireMesh( cache.find( &missing, &handle ) == SIR_MESH_NOT_FOUND, "lookup all occupied" );
	sirMeshEdgePin_t unclaimed = {};
	sirMeshGeometrySource_t overflow = makeMesh( &unclaimed, 700, 0 );
	requireMesh( cache.insert( &overflow, &handle ) == SIR_MESH_OUT_OF_CAPACITY, "capacity exhausted" );
	requireMesh( unclaimed.nReleased == 0, "overflow keeps source pin with caller" );
	requireMesh( cache.retire( another ) == SIR_MESH_OK, "idle retirement" );
	requireMesh( szPins[1].nReleased == 1, "idle retirement releases pin" );
	requireMesh( cache.retire( another ) == SIR_MESH_STALE_HANDLE, "retire stale handle" );
	requireMesh( cache.shutdown() == SIR_MESH_OK, "shutdown active idle source" );
	requireMesh( szPins[2].nReleased == 1, "shutdown releases active pin" );
	requireMesh( cache.getBorrowCount() == 0 && cache.getResidentCount() == 0, "shutdown clears counters" );
}

static void testMeshReentrantShutdown( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRMeshCache cache;
	sirMeshEdgeReentry_t probe = {};
	sirMeshEdgePin_t owner = {};
	probe.pCache = &cache;
	probe.pStorage = szStorage;
	probe.nStorageBytes = sizeof( szStorage );
	requireMesh( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize reentry fixture" );
	sirMeshGeometrySource_t source = { { 8, 1, 0, 0 }, s_szPositions, s_szIndices, 3, 3, &probe, onMeshReentryRelease };
	sirMeshGeometrySource_t other = makeMesh( &owner, 9, 0 );
	sirMeshHandle_t another = {};
	requireMesh( cache.insert( &source, &probe.handle ) == SIR_MESH_OK, "insert callback mesh" );
	requireMesh( cache.insert( &other, &another ) == SIR_MESH_OK, "insert subsequent mesh" );
	requireMesh( cache.shutdown() == SIR_MESH_OK, "shutdown invokes callback" );
	requireMesh( probe.nCallbacks == 1 && owner.nReleased == 1, "release every pin once" );
	requireMesh( probe.eInitialize == SIR_MESH_BUSY && probe.eShutdown == SIR_MESH_NOT_READY && probe.eFind == SIR_MESH_NOT_READY &&
	                 probe.eInsert == SIR_MESH_NOT_READY,
	             "callback cannot modify shutdown cache" );
	requireMesh( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "reinitialization after callbacks" );
	requireMesh( cache.shutdown() == SIR_MESH_OK, "shutdown again" );
}

static void testCacheOwnerScope( void ) noexcept
{
	alignas( 64 ) uint8_t szStorageA[4096] = {};
	alignas( 64 ) uint8_t szStorageB[4096] = {};
	alignas( SIRMeshCache ) uint8_t szCacheStorage[sizeof( SIRMeshCache )] = {};
	sirMeshEdgePin_t szPins[3] = {};
	SIRMeshCache first;
	SIRMeshCache second;
	requireMesh( first.initialize( szStorageA, sizeof( szStorageA ), 1 ) == SIR_MESH_OK, "owner: init first" );
	requireMesh( second.initialize( szStorageB, sizeof( szStorageB ), 1 ) == SIR_MESH_OK, "owner: init second" );
	sirMeshGeometrySource_t sourceA = makeMesh( &szPins[0], 31, 0 );
	sirMeshGeometrySource_t sourceB = makeMesh( &szPins[1], 32, 0 );
	sirMeshHandle_t handleA = {};
	sirMeshHandle_t handleB = {};
	sirMeshGeometryRead_t read = {};
	requireMesh( first.insert( &sourceA, &handleA ) == SIR_MESH_OK, "owner: insert first" );
	requireMesh( second.insert( &sourceB, &handleB ) == SIR_MESH_OK, "owner: insert second" );
	requireMesh( handleA.nSlot == handleB.nSlot && handleA.nGeneration == handleB.nGeneration,
	             "owner: reproduce identical slot/generation" );
	requireMesh( handleA.nOwnerId != 0 && handleB.nOwnerId != 0 && handleA.nOwnerId != handleB.nOwnerId,
	             "owner: unique initialization epochs" );
	requireMesh( second.acquire( handleA, &read ) == SIR_MESH_STALE_HANDLE && read.pPositionsXYZ == nullptr,
	             "owner: reject foreign acquire" );
	requireMesh( second.release( handleA ) == SIR_MESH_STALE_HANDLE, "owner: reject foreign release" );
	requireMesh( second.retire( handleA ) == SIR_MESH_STALE_HANDLE, "owner: reject foreign retirement" );
	requireMesh( first.acquire( handleB, &read ) == SIR_MESH_STALE_HANDLE, "owner: reject reverse foreign acquire" );
	requireMesh( second.getResidentCount() == 1 && second.getBorrowCount() == 0 && szPins[1].nReleased == 0,
	             "owner: foreign handles did not mutate cache" );
	sirMeshHandle_t found = {};
	requireMesh( second.find( &sourceB.key, &found ) == SIR_MESH_OK && found.nOwnerId == handleB.nOwnerId,
	             "owner: lookup carries owner token" );
	requireMesh( first.shutdown() == SIR_MESH_OK && second.shutdown() == SIR_MESH_OK, "owner: shutdown both caches" );
	requireMesh( szPins[0].nReleased == 1 && szPins[1].nReleased == 1, "owner: exact pin releases" );
	requireMesh( first.initialize( szStorageA, sizeof( szStorageA ), 1 ) == SIR_MESH_OK, "owner: reinitialize first cache" );
	sirMeshGeometrySource_t sourceC = makeMesh( &szPins[2], 33, 0 );
	sirMeshHandle_t handleC = {};
	requireMesh( first.insert( &sourceC, &handleC ) == SIR_MESH_OK, "owner: insert after reinitialization" );
	requireMesh( handleC.nOwnerId != handleA.nOwnerId && first.acquire( handleA, &read ) == SIR_MESH_STALE_HANDLE,
	             "owner: old epoch cannot resolve after reinitialization" );
	requireMesh( first.shutdown() == SIR_MESH_OK && szPins[2].nReleased == 1, "owner: finish reinitialized cache" );

	// Placement-new at the identical address must not revive old handles.
	SIRMeshCache *pOriginal = new ( szCacheStorage ) SIRMeshCache();
	requireMesh( pOriginal->initialize( szStorageA, sizeof( szStorageA ), 1 ) == SIR_MESH_OK, "owner: placement cache initialize" );
	sirMeshEdgePin_t firstPlacementPin = {};
	sirMeshGeometrySource_t firstPlacementSource = makeMesh( &firstPlacementPin, 41, 0 );
	sirMeshHandle_t placementOld = {};
	requireMesh( pOriginal->insert( &firstPlacementSource, &placementOld ) == SIR_MESH_OK, "owner: placement cache insert" );
	requireMesh( pOriginal->shutdown() == SIR_MESH_OK, "owner: placement cache shutdown" );
	pOriginal->~SIRMeshCache();
	SIRMeshCache *pReplacement = new ( szCacheStorage ) SIRMeshCache();
	requireMesh( pReplacement->initialize( szStorageA, sizeof( szStorageA ), 1 ) == SIR_MESH_OK, "owner: replacement cache initialize" );
	sirMeshEdgePin_t replacementPin = {};
	sirMeshGeometrySource_t replacementSource = makeMesh( &replacementPin, 42, 0 );
	sirMeshHandle_t replacementHandle = {};
	requireMesh( pReplacement->insert( &replacementSource, &replacementHandle ) == SIR_MESH_OK, "owner: replacement cache insert" );
	requireMesh( replacementHandle.nSlot == placementOld.nSlot && replacementHandle.nGeneration == placementOld.nGeneration,
	             "owner: placement-new reproduces original collision" );
	requireMesh( replacementHandle.nOwnerId != placementOld.nOwnerId &&
	                 pReplacement->acquire( placementOld, &read ) == SIR_MESH_STALE_HANDLE,
	             "owner: old placement handle rejected" );
	requireMesh( pReplacement->shutdown() == SIR_MESH_OK, "owner: replacement shutdown" );
	pReplacement->~SIRMeshCache();
	requireMesh( firstPlacementPin.nReleased == 1 && replacementPin.nReleased == 1, "owner: placement pin releases" );
}

int main( void )
{
	testCacheOwnerScope();
	testMeshInvalidSources();
	testMeshKeysAndLeases();
	testMeshReentrantShutdown();
	return 0;
}
