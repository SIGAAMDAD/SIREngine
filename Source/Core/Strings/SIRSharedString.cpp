/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Strings/SIRSharedString.h>
#include <Core/Strings/SIRStringOps.h>
#include <Core/Containers/SIRRawStorage.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>

struct sirSharedStringHeader_s
{
	std::atomic<uint32_t> nRefs{ 1 };
	uint32_t nBytes{};
};

struct sirSharedStringBlock_s
{
	sirSharedStringHeader_t header;
	const sirContainerAllocator_t *pAllocator{};
};

struct sirSharedStringSlabHeader_s
{
	const sirContainerAllocator_t *pAllocator{};
	uint32_t nSlotBytes{};
	uint32_t nPayloadCapacity{};
	uint32_t iFree{};
	uint32_t nLive{};
	bool bRetired : 1;
	SIRSpinLock lock;
};

SIRSharedString::SIRSharedString( SIRSharedString&& other ) noexcept
    : m_nStorage( other.m_nStorage )
{
	other.m_nStorage = 0;
}

SIRSharedString::~SIRSharedString() noexcept
{
	reset();
}

SIRSharedString& SIRSharedString::operator=( SIRSharedString&& other ) noexcept
{
	if ( this != &other ) {
		reset();
		m_nStorage = other.m_nStorage;
		other.m_nStorage = 0;
	}
	return *this;
}

sirStringView_t SIRSharedString::getView( void ) const noexcept
{
	if ( isStatic() ) {
		return *reinterpret_cast<const sirStringView_t *>( m_nStorage & ~uintptr_t{ 1 } );
	}
	const sirSharedStringHeader_t *pHeader = getHeader();
	if ( pHeader == nullptr ) {
		return { "", 0 };
	}
	const char *pBytes = isSlab() ? reinterpret_cast<const char *>( pHeader + 1 ) : reinterpret_cast<const char *>( getBlock() + 1 );
	return { pBytes, pHeader->nBytes };
}

const char *SIRSharedString::cStr( void ) const noexcept
{
	return getView().pBytes;
}

SIRContainerResult SIRSharedString::tryAssign( const sirContainerAllocator_t *pAllocator, sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes == 0 ) {
		reset();
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	const sirStringView_t current = getView();
	const sirSharedStringBlock_t *pCurrent = getBlock();
	if ( pCurrent != nullptr && pCurrent->pAllocator == pAllocator && current.nBytes == view.nBytes ) {
		if ( memcmp( current.pBytes, view.pBytes, view.nBytes ) == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
	}
	size_t nAllocation = 0;
	if ( !SIRRawStorage::checkedAdd( sizeof( sirSharedStringBlock_t ) + 1, view.nBytes, &nAllocation ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	void *pMemory = nullptr;
	const SIRContainerResult eResult = SIRRawStorage::tryAllocate( pAllocator, nAllocation, alignof( sirSharedStringBlock_t ), &pMemory );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	sirSharedStringBlock_t *pBlock = new ( pMemory ) sirSharedStringBlock_t{};
	pBlock->header.nBytes = view.nBytes;
	pBlock->pAllocator = pAllocator;
	char *pszData = reinterpret_cast<char *>( pBlock + 1 );
	memcpy( pszData, view.pBytes, view.nBytes );
	pszData[view.nBytes] = '\0';
	reset();
	m_nStorage = reinterpret_cast<uintptr_t>( pBlock );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRSharedString::tryShare( const SIRSharedString& other ) noexcept
{
	sirSharedStringHeader_t *pBlock = other.getHeader();
	if ( other.m_nStorage == m_nStorage ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( pBlock != nullptr ) {
		// The source handle already pins the immutable block. Refcount increment needs no data ordering.
		uint32_t nRefs = pBlock->nRefs.load( std::memory_order_relaxed );
		do {
			if ( nRefs == UINT32_MAX ) {
				return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
			}
		} while ( !pBlock->nRefs.compare_exchange_weak( nRefs, nRefs + 1, std::memory_order_relaxed, std::memory_order_relaxed ) );
	}
	reset();
	m_nStorage = other.m_nStorage;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRSharedString::reset( void ) noexcept
{
	sirSharedStringHeader_t *pHeader = getHeader();
	sirSharedStringBlock_t *pBlock = getBlock();
	const bool bSlab = isSlab();
	m_nStorage = 0;
	if ( pHeader == nullptr ) { return; }
	if ( pHeader->nRefs.fetch_sub( 1, std::memory_order_release ) != 1 ) { return; }
	std::atomic_thread_fence( std::memory_order_acquire );
	if ( bSlab ) {
		SIRSharedStringSlab::release( pHeader );
		return;
	}
	const size_t nAllocation = sizeof( sirSharedStringBlock_t ) + static_cast<size_t>( pHeader->nBytes ) + 1;
	const sirContainerAllocator_t *pAllocator = pBlock->pAllocator;
	pBlock->~sirSharedStringBlock_t();
	SIRRawStorage::deallocate( pAllocator, pBlock, nAllocation, alignof( sirSharedStringBlock_t ) );
}

bool SIRSharedString::isStatic( void ) const noexcept
{
	return ( m_nStorage & uintptr_t{ 3 } ) == 1;
}

bool SIRSharedString::isSlab( void ) const noexcept
{
	return ( m_nStorage & uintptr_t{ 3 } ) == 2;
}

sirSharedStringHeader_t *SIRSharedString::getHeader( void ) const noexcept
{
	if ( isSlab() ) {
		return reinterpret_cast<sirSharedStringHeader_t *>( m_nStorage & ~uintptr_t{ 3 } );
	}
	sirSharedStringBlock_t *pBlock = getBlock();
	return pBlock == nullptr ? nullptr : &pBlock->header;
}

sirSharedStringBlock_t *SIRSharedString::getBlock( void ) const noexcept
{
	return ( m_nStorage & uintptr_t{ 3 } ) != 0 ? nullptr : reinterpret_cast<sirSharedStringBlock_t *>( m_nStorage );
}

SIRContainerResult SIRSharedString::tryBindStatic( const sirStringView_t& staticView ) noexcept
{
	// Caller guarantees both the descriptor and the terminated character range are static.
	if ( staticView.pBytes == nullptr || staticView.nBytes == UINT32_MAX ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( staticView.pBytes[staticView.nBytes] != '\0' ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	static_assert( alignof( sirStringView_t ) >= 4 && alignof( sirSharedStringBlock_t ) >= 4, "Pointer tag needs alignment" );
	const uintptr_t nAddress = reinterpret_cast<uintptr_t>( &staticView );
	reset();
	m_nStorage = nAddress | uintptr_t{ 1 };
	return SIRContainerResult::SIR_CONTAINER_OK;
}


SIRSharedStringSlab::~SIRSharedStringSlab() noexcept
{
	reset();
}

SIRContainerResult SIRSharedStringSlab::tryInitialize( const sirContainerAllocator_t *pAllocator, uint32_t nPayloadCapacity ) noexcept
{
	if ( m_pSlab != nullptr ) { return SIRContainerResult::SIR_CONTAINER_BUSY; }
	if ( nPayloadCapacity >= SLAB_BYTES - sizeof( sirSharedStringHeader_t ) ) { return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED; }
	static constexpr uint32_t ALIGNMENT = 8;
	const uint32_t nFirst = static_cast<uint32_t>( ( sizeof( sirSharedStringSlabHeader_t ) + ALIGNMENT - 1 ) & ~( static_cast<size_t>( ALIGNMENT ) - 1 ) );
	const uint32_t nSlot = ( nPayloadCapacity + static_cast<uint32_t>( sizeof( sirSharedStringHeader_t ) ) + ALIGNMENT ) & ~( ALIGNMENT - 1 );
	if ( nSlot > SLAB_BYTES - nFirst ) { return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED; }
	void *pMemory = nullptr;
	const SIRContainerResult eResult = SIRRawStorage::tryAllocate( pAllocator, SLAB_BYTES, SLAB_BYTES, &pMemory );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) { return eResult; }
	m_pSlab = new ( pMemory ) sirSharedStringSlabHeader_t;
	m_pSlab->pAllocator = pAllocator;
	m_pSlab->nSlotBytes = nSlot;
	m_pSlab->nPayloadCapacity = nPayloadCapacity;
	m_pSlab->iFree = nFirst;
	m_pSlab->bRetired = false;
	char *pBytes = static_cast<char *>( pMemory );
	for ( uint32_t i = nFirst; i <= SLAB_BYTES - nSlot; i += nSlot ) {
		const uint32_t iNext = i + nSlot <= SLAB_BYTES - nSlot ? i + nSlot : 0;
		memcpy( pBytes + i, &iNext, sizeof( iNext ) );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

uint32_t SIRSharedStringSlab::getPayloadCapacity( void ) const noexcept
{
	return m_pSlab == nullptr ? 0 : m_pSlab->nPayloadCapacity;
}

SIRContainerResult SIRSharedStringSlab::allocate( sirStringView_t view, sirSharedStringHeader_t *& pHeader ) noexcept
{
	if ( m_pSlab == nullptr ) { return SIRContainerResult::SIR_CONTAINER_CLOSED; }
	if ( view.nBytes > m_pSlab->nPayloadCapacity ) { return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED; }
	m_pSlab->lock.lock();
	const uint32_t iSlot = m_pSlab->iFree;
	if ( iSlot == 0 ) { m_pSlab->lock.unlock(); return SIRContainerResult::SIR_CONTAINER_FULL; }
	char *pSlot = reinterpret_cast<char *>( m_pSlab ) + iSlot;
	memcpy( &m_pSlab->iFree, pSlot, sizeof( uint32_t ) );
	++m_pSlab->nLive;
	m_pSlab->lock.unlock();
	pHeader = new ( pSlot ) sirSharedStringHeader_t{};
	pHeader->nBytes = view.nBytes;
	char *pText = reinterpret_cast<char *>( pHeader + 1 );
	memcpy( pText, view.pBytes, view.nBytes );
	pText[view.nBytes] = 0;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

sirSharedStringSlabHeader_t *SIRSharedStringSlab::ownerOf( const sirSharedStringHeader_t *pHeader ) noexcept
{
	// Slab alignment and slot addresses are part of this allocator's contract.
	return reinterpret_cast<sirSharedStringSlabHeader_t *>( reinterpret_cast<uintptr_t>( pHeader ) & ~( static_cast<uintptr_t>( SLAB_BYTES ) - 1 ) );
}

void SIRSharedStringSlab::destroy( sirSharedStringSlabHeader_t *pSlab ) noexcept
{
	const sirContainerAllocator_t *pAllocator = pSlab->pAllocator;
	pSlab->~sirSharedStringSlabHeader_t();
	SIRRawStorage::deallocate( pAllocator, pSlab, SLAB_BYTES, SLAB_BYTES );
}

void SIRSharedStringSlab::release( sirSharedStringHeader_t *pHeader ) noexcept
{
	sirSharedStringSlabHeader_t *pSlab = ownerOf( pHeader );
	const uint32_t iSlot = static_cast<uint32_t>( reinterpret_cast<uintptr_t>( pHeader ) - reinterpret_cast<uintptr_t>( pSlab ) );
	pHeader->~sirSharedStringHeader_t();
	pSlab->lock.lock();
	memcpy( reinterpret_cast<char *>( pSlab ) + iSlot, &pSlab->iFree, sizeof( uint32_t ) );
	pSlab->iFree = iSlot;
	--pSlab->nLive;
	const bool bDestroy = pSlab->bRetired && pSlab->nLive == 0;
	pSlab->lock.unlock();
	if ( bDestroy ) { destroy( pSlab ); }
}

void SIRSharedStringSlab::reset( void ) noexcept
{
	sirSharedStringSlabHeader_t *pSlab = m_pSlab;
	m_pSlab = nullptr;
	if ( pSlab == nullptr ) { return; }
	pSlab->lock.lock();
	pSlab->bRetired = true;
	const bool bDestroy = pSlab->nLive == 0;
	pSlab->lock.unlock();
	if ( bDestroy ) { destroy( pSlab ); }
}

SIRContainerResult SIRSharedString::tryAssign( SIRSharedStringSlab& slab, sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) { return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT; }
	if ( view.nBytes == 0 ) { reset(); return SIRContainerResult::SIR_CONTAINER_OK; }
	if ( isSlab() && SIRSharedStringSlab::ownerOf( getHeader() ) == slab.m_pSlab && SIRStringOps::equals( getView(), view ) ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	sirSharedStringHeader_t *pHeader = nullptr;
	const SIRContainerResult eResult = slab.allocate( view, pHeader );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) { return eResult; }
	reset();
	m_nStorage = reinterpret_cast<uintptr_t>( pHeader ) | uintptr_t{ 2 };
	return SIRContainerResult::SIR_CONTAINER_OK;
}
