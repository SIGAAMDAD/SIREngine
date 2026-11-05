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
#include <stdio.h>
#include <type_traits>

// The cache controls nonowning storage and sole source-pin retirement.
// Neither copying nor moving its lifecycle state is safe.
static_assert( !std::is_copy_constructible<SIRMeshCache>::value, "Mesh cache must not be copy-constructible" );
static_assert( !std::is_move_constructible<SIRMeshCache>::value, "Mesh cache must not be move-constructible" );
static_assert( !std::is_copy_assignable<SIRMeshCache>::value, "Mesh cache must not be copy-assignable" );
static_assert( !std::is_move_assignable<SIRMeshCache>::value, "Mesh cache must not be move-assignable" );

// Covers the CPU metadata-only contract; source bytes are externally owned.
typedef struct sirMeshTestOwner_s
{
	uint32_t nReleases;
} sirMeshTestOwner_t;

static void releaseSourcePin( void *pContext ) noexcept
{
	sirMeshTestOwner_t *pOwner = ( sirMeshTestOwner_t * )pContext;
	++pOwner->nReleases;
}

static bool require( bool bSuccess, const char *pszCase ) noexcept
{
	if ( !bSuccess ) {
		fprintf( stderr, "FAILED: %s\n", pszCase );
	}
	return bSuccess;
}

typedef struct sirMeshShutdownProbe_s
{
	SIRMeshCache *pCache;
	void *pStorage;
	uint64_t nStorageBytes;
	sirMeshGeometrySource_t replacement;
	sirMeshHandle_t original;
	uint32_t nReleaseCalls;
	sirMeshResult_t eInsert;
	sirMeshResult_t eInitialize;
	sirMeshResult_t eShutdown;
	sirMeshResult_t eAcquire;
} sirMeshShutdownProbe_t;

static void releaseSourcePinWithReentry( void *pContext ) noexcept
{
	sirMeshShutdownProbe_t *pProbe = ( sirMeshShutdownProbe_t * )pContext;
	++pProbe->nReleaseCalls;
	sirMeshHandle_t replacement = {};
	sirMeshGeometryRead_t read = {};
	pProbe->eInsert = pProbe->pCache->insert( &pProbe->replacement, &replacement );
	pProbe->eInitialize = pProbe->pCache->initialize( pProbe->pStorage, pProbe->nStorageBytes, 2 );
	pProbe->eAcquire = pProbe->pCache->acquire( pProbe->original, &read );
	pProbe->eShutdown = pProbe->pCache->shutdown();
}

static bool testShutdownReentry( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	SIRMeshCache cache;
	sirMeshShutdownProbe_t probe = {};
	sirMeshTestOwner_t secondary = {};
	probe.pCache = &cache;
	probe.pStorage = szStorage;
	probe.nStorageBytes = sizeof( szStorage );
	probe.replacement = { { 900, 1, 0, 0 }, szVertices, szIndices, 3, 3, &secondary, releaseSourcePin };
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "reentry: initialize" ) ) {
		return false;
	}
	sirMeshGeometrySource_t first = { { 901, 1, 0, 0 }, szVertices, szIndices, 3, 3, &probe, releaseSourcePinWithReentry };
	sirMeshGeometrySource_t second = { { 902, 1, 0, 0 }, szVertices, szIndices, 3, 3, &secondary, releaseSourcePin };
	sirMeshHandle_t other = {};
	if ( !require( cache.insert( &first, &probe.original ) == SIR_MESH_OK, "reentry: first mesh" ) ||
	     !require( cache.insert( &second, &other ) == SIR_MESH_OK, "reentry: second mesh" ) ) {
		cache.shutdown();
		return false;
	}
	if ( !require( cache.shutdown() == SIR_MESH_OK, "reentry: shutdown" ) ||
	     !require( probe.nReleaseCalls == 1 && secondary.nReleases == 1, "reentry: exact release counts" ) ||
	     !require( probe.eInsert == SIR_MESH_NOT_READY, "reentry: reject insert" ) ||
	     !require( probe.eInitialize == SIR_MESH_BUSY, "reentry: reject initialize" ) ||
	     !require( probe.eAcquire == SIR_MESH_NOT_READY, "reentry: reject acquire" ) ||
	     !require( probe.eShutdown == SIR_MESH_NOT_READY, "reentry: reject nested shutdown" ) ||
	     !require( cache.getCapacity() == 0 && cache.getResidentCount() == 0, "reentry: empty cache" ) ) {
		return false;
	}
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "reentry: allow later initialize" ) ) {
		return false;
	}
	return require( cache.shutdown() == SIR_MESH_OK, "reentry: later shutdown" );
}

static bool testMalformedGeometry( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	const uint32_t szInvalidIndices[3] = { 0, 1, 3 };
	float szNotFinite[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	sirMeshTestOwner_t owner = {};
	SIRMeshCache cache;
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 1 ) == SIR_MESH_OK, "malformed: initialize" ) ) {
		return false;
	}
	sirMeshGeometrySource_t source = { { 42, 7, 0, 0 }, szVertices, szIndices, 3, 3, &owner, releaseSourcePin };
	sirMeshHandle_t handle = {};
	source.key.nAssetId = 0;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: asset id" ) )
		return false;
	source.key.nAssetId = 42;
	source.key.nRevision = 0;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: revision" ) )
		return false;
	source.key.nRevision = 7;
	source.key.nReserved = 1;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: reserved" ) )
		return false;
	source.key.nReserved = 0;
	source.pSourcePin = nullptr;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: pin" ) )
		return false;
	source.pSourcePin = &owner;
	source.pPositionsXYZ = nullptr;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: positions" ) )
		return false;
	source.pPositionsXYZ = szVertices;
	source.pTriangleIndices = nullptr;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: indices pointer" ) )
		return false;
	source.pTriangleIndices = szIndices;
	source.nVertexCount = 0;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: empty vertices" ) )
		return false;
	source.nVertexCount = 3;
	source.nIndexCount = 1;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: topology" ) )
		return false;
	source.nIndexCount = 3;
	source.nVertexCount = UINT32_MAX;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: count overflow" ) )
		return false;
	source.nVertexCount = 3;
	source.pTriangleIndices = szInvalidIndices;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: index range" ) )
		return false;
	source.pTriangleIndices = szIndices;
	source.pPositionsXYZ = szNotFinite;
	szNotFinite[0] = std::numeric_limits<float>::quiet_NaN();
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: NaN" ) )
		return false;
	szNotFinite[0] = std::numeric_limits<float>::infinity();
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: infinity" ) )
		return false;
	if ( !require( owner.nReleases == 0 && cache.getResidentCount() == 0, "malformed: ownership unchanged" ) )
		return false;
	sirMeshHandle_t invalid = { UINT32_MAX, UINT32_MAX };
	sirMeshGeometryRead_t read = {};
	if ( !require( cache.find( nullptr, &handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: null lookup key" ) ||
	     !require( cache.find( &source.key, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "malformed: null lookup output" ) ||
	     !require( cache.acquire( invalid, &read ) == SIR_MESH_STALE_HANDLE, "malformed: stale acquire" ) ||
	     !require( cache.acquire( invalid, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "malformed: null borrow output" ) ||
	     !require( cache.release( invalid ) == SIR_MESH_STALE_HANDLE, "malformed: stale release" ) ||
	     !require( cache.retire( invalid ) == SIR_MESH_STALE_HANDLE, "malformed: stale retire" ) ) {
		return false;
	}
	source.pPositionsXYZ = szVertices;
	if ( !require( cache.insert( &source, &handle ) == SIR_MESH_OK, "malformed: accept valid mesh" ) )
		return false;
	if ( !require( cache.release( handle ) == SIR_MESH_INVALID_ARGUMENT, "malformed: release without borrow" ) ||
	     !require( cache.acquire( handle, &read ) == SIR_MESH_OK, "malformed: first borrow" ) ) {
		return false;
	}
	if ( !require( cache.retire( handle ) == SIR_MESH_OK, "malformed: retire valid mesh" ) )
		return false;
	if ( !require( cache.retire( handle ) == SIR_MESH_RETIRED, "malformed: duplicate retirement" ) ||
	     !require( cache.release( handle ) == SIR_MESH_OK, "malformed: final borrow release" ) ) {
		return false;
	}
	if ( !require( owner.nReleases == 1, "malformed: exactly once release" ) )
		return false;
	if ( !require( cache.shutdown() == SIR_MESH_OK, "malformed: shutdown" ) ||
	     !require( cache.find( &source.key, &handle ) == SIR_MESH_NOT_READY, "malformed: closed find" ) ||
	     !require( cache.release( handle ) == SIR_MESH_NOT_READY, "malformed: closed release" ) ||
	     !require( cache.retire( handle ) == SIR_MESH_NOT_READY, "malformed: closed retire" ) ) {
		return false;
	}
	return true;
}

int main( void )
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const float szVertices[9] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
	const uint32_t szIndices[3] = { 0, 1, 2 };
	sirMeshTestOwner_t ownerA = {};
	sirMeshTestOwner_t ownerB = {};
	sirMeshTestOwner_t ownerC = {};
	sirMeshGeometrySource_t source = { { 42, 7, 0, 0 }, szVertices, szIndices, 3, 3, &ownerA, releaseSourcePin };
	SIRMeshCache cache;
	sirMeshHandle_t hA = {};
	sirMeshHandle_t hB = {};
	sirMeshGeometryRead_t readA = {};
	sirMeshGeometryRead_t readB = {};

	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_NOT_READY, "reject uninitialized insertion" ) ) {
		return 1;
	}
	if ( !require( cache.requiredBytes( 2 ) <= sizeof( szStorage ), "storage size" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage + 1, sizeof( szStorage ) - 1, 2 ) == SIR_MESH_INVALID_ARGUMENT,
	               "reject unaligned storage" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage, cache.requiredBytes( 2 ) - 1, 2 ) == SIR_MESH_INVALID_ARGUMENT,
	               "reject undersized storage" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_OK, "initialize" ) ) {
		return 1;
	}
	if ( !require( cache.initialize( szStorage, sizeof( szStorage ), 2 ) == SIR_MESH_BUSY, "reject double initialization" ) ) {
		return 1;
	}
	if ( !require( cache.getCapacity() == 2 && cache.getResidentCount() == 0, "initial accounting" ) ) {
		return 1;
	}

	if ( !require( cache.insert( &source, nullptr ) == SIR_MESH_INVALID_ARGUMENT, "reject missing output" ) ) {
		return 1;
	}
	source.pfnReleaseSourcePin = nullptr;
	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_INVALID_ARGUMENT, "require source pin" ) ) {
		return 1;
	}
	source.pfnReleaseSourcePin = releaseSourcePin;
	const uint32_t szBadIndices[3] = { 0, 1, 3 };
	source.pTriangleIndices = szBadIndices;
	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_INVALID_ARGUMENT, "invalid triangle index" ) ) {
		return 1;
	}
	source.pTriangleIndices = szIndices;
	if ( !require( cache.insert( &source, &hA ) == SIR_MESH_OK, "insert mesh" ) ) {
		return 1;
	}
	if ( !require( cache.insert( &source, &hB ) == SIR_MESH_DUPLICATE, "deduplicate mesh key" ) ) {
		return 1;
	}
	if ( !require( cache.find( &source.key, &hB ) == SIR_MESH_OK && hB.nSlot == hA.nSlot, "lookup" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readA ) == SIR_MESH_OK, "physics borrow" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readB ) == SIR_MESH_OK, "renderer borrow" ) ) {
		return 1;
	}
	if ( !require( readA.pPositionsXYZ == readB.pPositionsXYZ && readA.pPositionsXYZ == szVertices, "physically same vertices" ) ) {
		return 1;
	}
	if ( !require( readA.pTriangleIndices == readB.pTriangleIndices && readA.pTriangleIndices == szIndices, "physically same topology" ) ) {
		return 1;
	}
	if ( !require( cache.getBorrowCount() == 2, "borrow accounting" ) ) {
		return 1;
	}

	if ( !require( cache.retire( hA ) == SIR_MESH_OK, "retire leased mesh" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readA ) == SIR_MESH_RETIRED, "no new borrow during retirement" ) ) {
		return 1;
	}
	if ( !require( cache.find( &source.key, &hB ) == SIR_MESH_NOT_FOUND, "retired not discoverable" ) ) {
		return 1;
	}
	if ( !require( ownerA.nReleases == 0, "pin alive with readers" ) ) {
		return 1;
	}
	if ( !require( cache.shutdown() == SIR_MESH_BUSY, "no shutdown while borrowed" ) ) {
		return 1;
	}
	if ( !require( cache.release( hA ) == SIR_MESH_OK && ownerA.nReleases == 0, "one reader still alive" ) ) {
		return 1;
	}
	if ( !require( cache.release( hA ) == SIR_MESH_OK && ownerA.nReleases == 1, "final reader releases pin" ) ) {
		return 1;
	}
	if ( !require( cache.release( hA ) == SIR_MESH_STALE_HANDLE, "stale release rejected" ) ) {
		return 1;
	}

	source.pSourcePin = &ownerB;
	if ( !require( cache.insert( &source, &hB ) == SIR_MESH_OK, "slot reuse" ) ) {
		return 1;
	}
	if ( !require( hB.nSlot == hA.nSlot && hB.nGeneration != hA.nGeneration, "generation increment" ) ) {
		return 1;
	}
	if ( !require( cache.acquire( hA, &readA ) == SIR_MESH_STALE_HANDLE, "ABA protection" ) ) {
		return 1;
	}
	source.key = { 43, 8, 1, 0 };
	source.pSourcePin = &ownerC;
	sirMeshHandle_t hC = {};
	if ( !require( cache.insert( &source, &hC ) == SIR_MESH_OK, "second resident mesh" ) ) {
		return 1;
	}
	source.key = { 44, 9, 0, 0 };
	if ( !require( cache.insert( &source, &hC ) == SIR_MESH_OUT_OF_CAPACITY, "capacity bound" ) ) {
		return 1;
	}
	if ( !require( cache.shutdown() == SIR_MESH_OK, "shutdown releases all idle pins" ) ) {
		return 1;
	}
	if ( !require( ownerA.nReleases == 1 && ownerB.nReleases == 1 && ownerC.nReleases == 1, "exactly once release" ) ) {
		return 1;
	}
	if ( !require( cache.getResidentCount() == 0 && cache.getCapacity() == 0, "shutdown accounting" ) ) {
		return 1;
	}
	if ( !require( cache.shutdown() == SIR_MESH_NOT_READY, "shutdown idempotence status" ) ) {
		return 1;
	}
	if ( !testShutdownReentry() || !testMalformedGeometry() ) {
		return 1;
	}
	return 0;
}
