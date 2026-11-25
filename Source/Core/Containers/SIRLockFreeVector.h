/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIR_LOCKFREEVECTOR_H__
#define __SIR_LOCKFREEVECTOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <atomic>
#include <new>
#include <stdint.h>
#include <type_traits>
#include <utility>

// Bounded MPMC append. Published elements remain immutable until quiescent reset.
// The owner must ensure there are no concurrent accesses during reset/destruction.
template <typename T>
class SIRLockFreeVector
{
public:
	explicit SIRLockFreeVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator ),
	      m_nClaimAndSeal( 0 ),
	      m_nPublished( 0 )
	{
	}

	~SIRLockFreeVector() noexcept
	{
		if ( m_pReady != nullptr ) {
			clearAfterQuiescence();
			for ( uint32_t i = m_nCapacity; i != 0; --i ) {
				m_pReady[i - 1].~atomic();
			}
		}
		SIRRawStorage::deallocate(
		    m_pAllocator, m_pReady, static_cast<size_t>( m_nCapacity * sizeof( std::atomic<uint8_t> ) ), alignof( std::atomic<uint8_t> ) );
		SIRRawStorage::deallocate( m_pAllocator, m_pValues, static_cast<size_t>( m_nCapacity * sizeof( T ) ), alignof( T ) );
	}

	SIRLockFreeVector( const SIRLockFreeVector& ) = delete;
	SIRLockFreeVector& operator=( const SIRLockFreeVector& ) = delete;

	SIRContainerResult tryInitialize( uint32_t nCapacity ) noexcept
	{
		if ( m_pReady != nullptr || nCapacity == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nValueBytes = 0;
		size_t nStateBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nValueBytes ) ||
		     !SIRRawStorage::checkedMultiply( nCapacity, sizeof( std::atomic<uint8_t> ), &nStateBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pValues = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nValueBytes, alignof( T ), &pValues );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		void *pReady = nullptr;
		eResult = SIRRawStorage::tryAllocate( m_pAllocator, nStateBytes, alignof( std::atomic<uint8_t> ), &pReady );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			SIRRawStorage::deallocate( m_pAllocator, pValues, nValueBytes, alignof( T ) );
			return eResult;
		}
		m_pValues = static_cast<T *>( pValues );
		m_pReady = ( std::atomic<uint8_t> * )pReady;
		m_nCapacity = nCapacity;
		for ( uint32_t i = 0; i < nCapacity; ++i ) {
			new ( m_pReady + i ) std::atomic<uint8_t>( 0 );
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceBack( uint32_t *pIndex, Args&&...args ) noexcept
	{
		static_assert( std::is_nothrow_constructible<T, Args...>::value, "Concurrent append requires nonthrowing element construction" );
		if ( pIndex == nullptr || m_pReady == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nObserved = m_nClaimAndSeal.load( std::memory_order_relaxed );
		for ( ;; ) {
			if ( ( nObserved & SEALED ) != 0 ) {
				return SIRContainerResult::SIR_CONTAINER_CLOSED;
			}
			if ( nObserved >= m_nCapacity ) {
				return SIRContainerResult::SIR_CONTAINER_FULL;
			}
			if ( m_nClaimAndSeal.compare_exchange_weak( nObserved, nObserved + 1, std::memory_order_acq_rel, std::memory_order_relaxed ) ) {
				break;
			}
		}
		const uint32_t iIndex = static_cast<uint32_t>( nObserved );
		new ( m_pValues + iIndex ) T( std::forward<Args>( args )... );
		m_pReady[iIndex].store( 1, std::memory_order_release );
		m_nPublished.fetch_add( 1, std::memory_order_release );
		*pIndex = iIndex;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPushBack( const T& rValue, uint32_t *pIndex ) noexcept
	{
		return tryEmplaceBack( pIndex, rValue );
	}

	const T *tryGetPublished( uint32_t iIndex ) const noexcept
	{
		if ( iIndex >= getClaimedCount() ) {
			return nullptr;
		}
		return m_pReady[iIndex].load( std::memory_order_acquire ) != 0 ? m_pValues + iIndex : nullptr;
	}

	uint32_t getClaimedCount( void ) const noexcept
	{
		return static_cast<uint32_t>( m_nClaimAndSeal.load( std::memory_order_acquire ) & ~SEALED );
	}

	uint32_t getPublishedCount( void ) const noexcept
	{
		return m_nPublished.load( std::memory_order_acquire );
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}

	SIRContainerResult trySeal( void ) noexcept
	{
		if ( m_pReady == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nObserved = m_nClaimAndSeal.load( std::memory_order_acquire );
		while ( ( nObserved & SEALED ) == 0 ) {
			if ( m_nClaimAndSeal.compare_exchange_weak(
			         nObserved, nObserved | SEALED, std::memory_order_acq_rel, std::memory_order_acquire ) ) {
				break;
			}
		}
		return getClaimedCount() == getPublishedCount() ? SIRContainerResult::SIR_CONTAINER_OK : SIRContainerResult::SIR_CONTAINER_BUSY;
	}

	SIRContainerResult tryGetSealedView( SIRReadOnlyVectorView<T> *pView ) const noexcept
	{
		if ( pView == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint64_t nState = m_nClaimAndSeal.load( std::memory_order_acquire );
		const uint32_t nClaimed = static_cast<uint32_t>( nState & ~SEALED );
		if ( ( nState & SEALED ) == 0 || nClaimed != getPublishedCount() ) {
			return SIRContainerResult::SIR_CONTAINER_BUSY;
		}
		*pView = { m_pValues, nClaimed };
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void clearAfterQuiescence( void ) noexcept
	{
		if ( m_pReady == nullptr ) {
			return;
		}
		const uint32_t nClaimed = getClaimedCount();
		for ( uint32_t i = 0; i < nClaimed; ++i ) {
			if ( m_pReady[i].load( std::memory_order_relaxed ) != 0 ) {
				m_pValues[i].~T();
				m_pReady[i].store( 0, std::memory_order_relaxed );
			}
		}
		m_nPublished.store( 0, std::memory_order_relaxed );
		m_nClaimAndSeal.store( 0, std::memory_order_release );
	}

private:
	static constexpr uint64_t SEALED = uint64_t( 1 ) << 63;
	static_assert( std::is_nothrow_destructible<T>::value, "Concurrent elements must be nothrow destructible" );
	const sirContainerAllocator_t *m_pAllocator;
	T *m_pValues = nullptr;
	std::atomic<uint8_t> *m_pReady = nullptr;
	uint32_t m_nCapacity = 0;
	std::atomic<uint64_t> m_nClaimAndSeal;
	std::atomic<uint32_t> m_nPublished;
};

#endif // __SIR_LOCKFREEVECTOR_H__
