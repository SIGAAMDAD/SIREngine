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

#include <assert.h>
#include <mutex>
#include <math.h>
#include <new>
#include <stdint.h>

// The stand-alone generation-exhaustion fixture seeds near UINT32_MAX.
// Ordinary engine builds always start at zero.
#ifdef SIR_MESH_TEST_GENERATION_SEED
static constexpr uint32_t SIR_MESH_INITIAL_GENERATION = SIR_MESH_TEST_GENERATION_SEED;
#else
static constexpr uint32_t SIR_MESH_INITIAL_GENERATION = 0;
#endif

// Standalone boundary tests use a reduced limit to verify counter saturation.
// The shipping build always retains the full uint32_t borrow range.
#ifdef SIR_MESH_TEST_MAX_BORROWS
static constexpr uint32_t SIR_MESH_MAX_BORROWS = SIR_MESH_TEST_MAX_BORROWS;
#else
static constexpr uint32_t SIR_MESH_MAX_BORROWS = UINT32_MAX;
#endif

// Each cache initialization receives a unique, nonzero owner ID within this linked MeshCache implementation.
// Saturate rather than recycle IDs so a foreign/stale handle never aliases.
#ifdef SIR_MESH_TEST_OWNER_ID_SEED
static constexpr uint64_t SIR_MESH_INITIAL_OWNER_ID = SIR_MESH_TEST_OWNER_ID_SEED;
#else
static constexpr uint64_t SIR_MESH_INITIAL_OWNER_ID = 1;
#endif
static std::mutex s_MeshCacheOwnerIdMutex;
static uint64_t s_nNextMeshCacheOwnerId = SIR_MESH_INITIAL_OWNER_ID;

static uint64_t allocateMeshCacheOwnerId( void ) noexcept
{
	// This lock is only acquired on cache initialization, never during
	// insert, lookup, acquire, retire, release or shutdown.
	std::lock_guard<std::mutex> guard( s_MeshCacheOwnerIdMutex );
	if ( s_nNextMeshCacheOwnerId == UINT64_MAX ) {
		return 0;
	}
	return s_nNextMeshCacheOwnerId++;
}

typedef struct sirMeshCacheSlot_s
{
	sirMeshGeometrySource_t source;
	uint32_t nGeneration;
	uint32_t nBorrows;
	bool bOccupied;
	bool bRetiring;
} sirMeshCacheSlot_t;

static bool sameMeshKey( const sirMeshKey_t *pA, const sirMeshKey_t *pB ) noexcept
{
	return pA->nAssetId == pB->nAssetId && pA->nRevision == pB->nRevision && pA->nVariant == pB->nVariant && pA->nReserved == pB->nReserved;
}

static bool validateMeshSource( const sirMeshGeometrySource_t *pSource ) noexcept
{
	if ( pSource->key.nAssetId == 0 || pSource->key.nRevision == 0 || pSource->key.nReserved != 0 ) {
		return false;
	}
	if ( pSource->pPositionsXYZ == nullptr || pSource->pTriangleIndices == nullptr || pSource->pSourcePin == nullptr ||
	     pSource->pfnReleaseSourcePin == nullptr ) {
		return false;
	}
	if ( pSource->nVertexCount == 0 || pSource->nIndexCount == 0 || ( pSource->nIndexCount % 3U ) != 0 ) {
		return false;
	}
	if ( pSource->nVertexCount > UINT32_MAX / 3U ) {
		return false;
	}

	for ( uint32_t iIndex = 0; iIndex < pSource->nIndexCount; ++iIndex ) {
		if ( pSource->pTriangleIndices[iIndex] >= pSource->nVertexCount ) {
			return false;
		}
	}

	const uint32_t nPositionFloats = pSource->nVertexCount * 3U;
	for ( uint32_t iFloat = 0; iFloat < nPositionFloats; ++iFloat ) {
		if ( !isfinite( pSource->pPositionsXYZ[iFloat] ) ) {
			return false;
		}
	}

	return true;
}

SIRMeshCache::SIRMeshCache( void ) noexcept
    : m_pSlots( nullptr ), m_nCapacity( 0 ), m_nResidentCount( 0 ), m_nBorrowCount( 0 ), m_nNextGeneration( SIR_MESH_INITIAL_GENERATION ),
      m_nOwnerId( 0 ), m_bReady( false ), m_bShuttingDown( false )
{
}

SIRMeshCache::~SIRMeshCache( void ) noexcept
{
	assert( !m_bReady && "SIRMeshCache requires explicit shutdown" );
}

uint32_t SIRMeshCache::getCapacity( void ) const noexcept
{
	return m_nCapacity;
}

uint32_t SIRMeshCache::getResidentCount( void ) const noexcept
{
	return m_nResidentCount;
}

uint32_t SIRMeshCache::getBorrowCount( void ) const noexcept
{
	return m_nBorrowCount;
}

uint64_t SIRMeshCache::requiredBytes( uint32_t nCapacity ) noexcept
{
	return ( uint64_t )nCapacity * sizeof( sirMeshCacheSlot_t );
}

uint32_t SIRMeshCache::requiredAlignment( void ) noexcept
{
	return ( uint32_t )alignof( sirMeshCacheSlot_t );
}

sirMeshResult_t SIRMeshCache::initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nCapacity ) noexcept
{
	if ( m_bReady || m_bShuttingDown ) {
		return SIR_MESH_BUSY;
	}
	if ( pStorage == nullptr || nCapacity == 0 || nStorageBytes < requiredBytes( nCapacity ) ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}
	if ( ( ( uintptr_t )pStorage % requiredAlignment() ) != 0 ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}

	const uint64_t nOwnerId = allocateMeshCacheOwnerId();
	if ( nOwnerId == 0 ) {
		return SIR_MESH_OUT_OF_CAPACITY;
	}

	m_pSlots = ( sirMeshCacheSlot_t * )pStorage;
	m_nOwnerId = nOwnerId;
	m_nCapacity = nCapacity;
	m_nResidentCount = 0;
	m_nBorrowCount = 0;
	for ( uint32_t iSlot = 0; iSlot < nCapacity; ++iSlot ) {
		new ( &m_pSlots[iSlot] ) sirMeshCacheSlot_t{};
	}
	m_bReady = true;
	return SIR_MESH_OK;
}

bool SIRMeshCache::isValidHandle( sirMeshHandle_t handle ) const noexcept
{
	// Only called after the public entrypoint has checked m_bReady.
	return handle.nOwnerId == m_nOwnerId && handle.nSlot < m_nCapacity && m_pSlots[handle.nSlot].bOccupied &&
	       m_pSlots[handle.nSlot].nGeneration == handle.nGeneration;
}

sirMeshResult_t SIRMeshCache::insert( const sirMeshGeometrySource_t *pSource, sirMeshHandle_t *pHandle ) noexcept
{
	if ( !m_bReady ) {
		return SIR_MESH_NOT_READY;
	}
	if ( pHandle == nullptr || pSource == nullptr || !validateMeshSource( pSource ) ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}

	uint32_t iFree = m_nCapacity;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirMeshCacheSlot_t *pSlot = &m_pSlots[iSlot];
		if ( pSlot->bOccupied && sameMeshKey( &pSlot->source.key, &pSource->key ) ) {
			return SIR_MESH_DUPLICATE;
		}
		if ( !pSlot->bOccupied && iFree == m_nCapacity ) {
			iFree = iSlot;
		}
	}
	if ( iFree == m_nCapacity ) {
		return SIR_MESH_OUT_OF_CAPACITY;
	}

	// Never recycle a generation: a previously returned handle must not
	// become valid again, even after repeated shutdown/reinitialize cycles.
	if ( m_nNextGeneration == UINT32_MAX ) {
		return SIR_MESH_OUT_OF_CAPACITY;
	}

	sirMeshCacheSlot_t *pSlot = &m_pSlots[iFree];
	++m_nNextGeneration;
	pSlot->nGeneration = m_nNextGeneration;
	pSlot->source = *pSource;
	pSlot->nBorrows = 0;
	pSlot->bRetiring = false;
	pSlot->bOccupied = true;
	++m_nResidentCount;
	*pHandle = { iFree, pSlot->nGeneration, m_nOwnerId };
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCache::find( const sirMeshKey_t *pKey, sirMeshHandle_t *pHandle ) const noexcept
{
	if ( !m_bReady ) {
		return SIR_MESH_NOT_READY;
	}
	if ( pHandle == nullptr || pKey == nullptr ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}
	*pHandle = {};

	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirMeshCacheSlot_t *pSlot = &m_pSlots[iSlot];
		if ( pSlot->bOccupied && !pSlot->bRetiring && sameMeshKey( &pSlot->source.key, pKey ) ) {
			*pHandle = { iSlot, pSlot->nGeneration, m_nOwnerId };
			return SIR_MESH_OK;
		}
	}
	return SIR_MESH_NOT_FOUND;
}

sirMeshResult_t SIRMeshCache::acquire( sirMeshHandle_t handle, sirMeshGeometryRead_t *pRead ) noexcept
{
	if ( !m_bReady ) {
		return SIR_MESH_NOT_READY;
	}
	if ( pRead == nullptr ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}
	*pRead = {};
	if ( !isValidHandle( handle ) ) {
		return SIR_MESH_STALE_HANDLE;
	}

	sirMeshCacheSlot_t *pSlot = &m_pSlots[handle.nSlot];
	if ( pSlot->bRetiring ) {
		return SIR_MESH_RETIRED;
	}
	if ( pSlot->nBorrows == SIR_MESH_MAX_BORROWS || m_nBorrowCount == SIR_MESH_MAX_BORROWS ) {
		return SIR_MESH_OUT_OF_CAPACITY;
	}

	++pSlot->nBorrows;
	++m_nBorrowCount;
	*pRead = { pSlot->source.pPositionsXYZ, pSlot->source.pTriangleIndices, pSlot->source.nVertexCount, pSlot->source.nIndexCount,
	           pSlot->source.key };
	return SIR_MESH_OK;
}

void SIRMeshCache::finalizeSlot( uint32_t iSlot ) noexcept
{
	sirMeshCacheSlot_t *pSlot = &m_pSlots[iSlot];
	assert( pSlot->bOccupied && pSlot->nBorrows == 0 );

	void *pPin = pSlot->source.pSourcePin;
	void ( *pfnRelease )( void * ) noexcept = pSlot->source.pfnReleaseSourcePin;
	*pSlot = {};
	--m_nResidentCount;
	// Source ownership is released only after the slot cannot be resolved.
	pfnRelease( pPin );
}

sirMeshResult_t SIRMeshCache::release( sirMeshHandle_t handle ) noexcept
{
	if ( !m_bReady ) {
		return SIR_MESH_NOT_READY;
	}
	if ( !isValidHandle( handle ) ) {
		return SIR_MESH_STALE_HANDLE;
	}

	sirMeshCacheSlot_t *pSlot = &m_pSlots[handle.nSlot];
	if ( pSlot->nBorrows == 0 ) {
		return SIR_MESH_INVALID_ARGUMENT;
	}

	--pSlot->nBorrows;
	--m_nBorrowCount;
	if ( pSlot->nBorrows == 0 && pSlot->bRetiring ) {
		finalizeSlot( handle.nSlot );
	}
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCache::retire( sirMeshHandle_t handle ) noexcept
{
	if ( !m_bReady ) {
		return SIR_MESH_NOT_READY;
	}
	if ( !isValidHandle( handle ) ) {
		return SIR_MESH_STALE_HANDLE;
	}

	sirMeshCacheSlot_t *pSlot = &m_pSlots[handle.nSlot];
	if ( pSlot->bRetiring ) {
		return SIR_MESH_RETIRED;
	}

	pSlot->bRetiring = true;
	if ( pSlot->nBorrows == 0 ) {
		finalizeSlot( handle.nSlot );
	}
	return SIR_MESH_OK;
}

sirMeshResult_t SIRMeshCache::shutdown( void ) noexcept
{
	if ( !m_bReady ) {
		return SIR_MESH_NOT_READY;
	}
	if ( m_nBorrowCount != 0 ) {
		return SIR_MESH_BUSY;
	}

	// Callbacks may re-enter this cache. Block new reads/writes and prevent
	// reinitialization until all existing source pins have been released.
	m_bShuttingDown = true;
	m_bReady = false;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		if ( m_pSlots[iSlot].bOccupied ) {
			finalizeSlot( iSlot );
		}
	}
	m_pSlots = nullptr;
	m_nCapacity = 0;
	m_nOwnerId = 0;
	m_bShuttingDown = false;
	return SIR_MESH_OK;
}
