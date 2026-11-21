/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRLOCKFREEVECTOR_H__
#define __SIRENGINE_SIRLOCKFREEVECTOR_H__

#include <Core/SIRPCH.h>

#include <Core/Containers/SIRRawStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <Core/Platform/SIRAtomic.h>
#include <new>
#include <stdint.h>
#include <type_traits>
#include <utility>

// Bounded MPMC append. Published elements remain immutable until quiescent reset.
// The owner must ensure there are no concurrent accesses during reset/destruction.
template <typename T>
class SIRLockFreeVector
{
private:
	static constexpr uint64_t SEALED = uint64_t( 1 ) << 63;
	static_assert( std::is_nothrow_destructible<T>::value, "Concurrent elements must be nothrow destructible" );
	const sirContainerAllocator_t *m_pAllocator;
	T *m_pValues = nullptr;
	SIRAtomic<uint8_t> *m_pReady = nullptr;
	uint32_t m_nCapacity = 0;
	SIRAtomic<uint64_t> m_nClaimAndSeal;
	SIRAtomic<uint32_t> m_nPublished;

public:
	explicit SIRLockFreeVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator ), m_nClaimAndSeal( 0 ), m_nPublished( 0 )
	{
	}
	~SIRLockFreeVector( void ) noexcept
	{
		if ( m_pReady != nullptr ) {
			clearAfterQuiescence();
			for ( uint32_t i = m_nCapacity; i != 0; --i ) {
				m_pReady[i - 1].~SIRAtomic<uint8_t>();
			}
		}
		SIRRawStorage::deallocate( m_pAllocator, m_pReady, ( size_t )m_nCapacity * sizeof( SIRAtomic<uint8_t> ),
		                           alignof( SIRAtomic<uint8_t> ) );
		SIRRawStorage::deallocate( m_pAllocator, m_pValues, ( size_t )m_nCapacity * sizeof( T ), alignof( T ) );
	}
	SIRLockFreeVector( const SIRLockFreeVector& ) = delete;
	SIRLockFreeVector& operator=( const SIRLockFreeVector& ) = delete;

	sirContainerResult_t tryInitialize( uint32_t nCapacity ) noexcept
	{
		if ( m_pReady != nullptr || nCapacity == 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nValueBytes = 0;
		size_t nStateBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nValueBytes ) ||
		     !SIRRawStorage::checkedMultiply( nCapacity, sizeof( SIRAtomic<uint8_t> ), &nStateBytes ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pValues = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nValueBytes, alignof( T ), &pValues );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		void *pReady = nullptr;
		eResult = SIRRawStorage::tryAllocate( m_pAllocator, nStateBytes, alignof( SIRAtomic<uint8_t> ), &pReady );
		if ( eResult != SIR_CONTAINER_OK ) {
			SIRRawStorage::deallocate( m_pAllocator, pValues, nValueBytes, alignof( T ) );
			return eResult;
		}
		m_pValues = ( T * )pValues;
		m_pReady = ( SIRAtomic<uint8_t> * )pReady;
		m_nCapacity = nCapacity;
		for ( uint32_t i = 0; i < nCapacity; ++i ) {
			new ( m_pReady + i ) SIRAtomic<uint8_t>( 0 );
		}
		return SIR_CONTAINER_OK;
	}

	template <typename... Args>
	sirContainerResult_t tryEmplaceBack( uint32_t *pIndex, Args&&...args ) noexcept
	{
		static_assert( std::is_nothrow_constructible<T, Args...>::value, "Concurrent append requires nonthrowing element construction" );
		if ( pIndex == nullptr || m_pReady == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nObserved = m_nClaimAndSeal.load( SIR_MEMORY_ORDER_RELAXED );
		for ( ;; ) {
			if ( ( nObserved & SEALED ) != 0 ) {
				return SIR_CONTAINER_CLOSED;
			}
			if ( nObserved >= m_nCapacity ) {
				return SIR_CONTAINER_FULL;
			}
			if ( m_nClaimAndSeal.compareExchangeWeak( nObserved, nObserved + 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE,
			                                          SIR_MEMORY_ORDER_RELAXED ) ) {
				break;
			}
		}
		const uint32_t iIndex = ( uint32_t )nObserved;
		new ( m_pValues + iIndex ) T( std::forward<Args>( args )... );
		m_pReady[iIndex].store( 1, SIR_MEMORY_ORDER_RELEASE );
		m_nPublished.fetchAdd( 1, SIR_MEMORY_ORDER_RELEASE );
		*pIndex = iIndex;
		return SIR_CONTAINER_OK;
	}
	sirContainerResult_t tryPushBack( const T& rValue, uint32_t *pIndex ) noexcept
	{
		return tryEmplaceBack( pIndex, rValue );
	}
	const T *tryGetPublished( uint32_t iIndex ) const noexcept
	{
		if ( iIndex >= getClaimedCount() ) {
			return nullptr;
		}
		return m_pReady[iIndex].load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ? m_pValues + iIndex : nullptr;
	}
	uint32_t getClaimedCount( void ) const noexcept
	{
		return ( uint32_t )( m_nClaimAndSeal.load( SIR_MEMORY_ORDER_ACQUIRE ) & ~SEALED );
	}
	uint32_t getPublishedCount( void ) const noexcept
	{
		return m_nPublished.load( SIR_MEMORY_ORDER_ACQUIRE );
	}
	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}
	sirContainerResult_t trySeal( void ) noexcept
	{
		if ( m_pReady == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nObserved = m_nClaimAndSeal.load( SIR_MEMORY_ORDER_ACQUIRE );
		while ( ( nObserved & SEALED ) == 0 ) {
			if ( m_nClaimAndSeal.compareExchangeWeak( nObserved, nObserved | SEALED, SIR_MEMORY_ORDER_ACQUIRE_RELEASE,
			                                          SIR_MEMORY_ORDER_ACQUIRE ) ) {
				break;
			}
		}
		return getClaimedCount() == getPublishedCount() ? SIR_CONTAINER_OK : SIR_CONTAINER_BUSY;
	}
	sirContainerResult_t tryGetSealedView( SIRReadOnlyVectorView<T> *pView ) const noexcept
	{
		if ( pView == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint64_t nState = m_nClaimAndSeal.load( SIR_MEMORY_ORDER_ACQUIRE );
		const uint32_t nClaimed = ( uint32_t )( nState & ~SEALED );
		if ( ( nState & SEALED ) == 0 || nClaimed != getPublishedCount() ) {
			return SIR_CONTAINER_BUSY;
		}
		*pView = { m_pValues, nClaimed };
		return SIR_CONTAINER_OK;
	}
	void clearAfterQuiescence( void ) noexcept
	{
		if ( m_pReady == nullptr ) {
			return;
		}
		const uint32_t nClaimed = getClaimedCount();
		for ( uint32_t i = 0; i < nClaimed; ++i ) {
			if ( m_pReady[i].load( SIR_MEMORY_ORDER_RELAXED ) != 0 ) {
				m_pValues[i].~T();
				m_pReady[i].store( 0, SIR_MEMORY_ORDER_RELAXED );
			}
		}
		m_nPublished.store( 0, SIR_MEMORY_ORDER_RELAXED );
		m_nClaimAndSeal.store( 0, SIR_MEMORY_ORDER_RELEASE );
	}
};

#endif // __SIRENGINE_SIRLOCKFREEVECTOR_H__
