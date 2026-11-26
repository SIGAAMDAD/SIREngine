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

#ifndef __SIR_BATCHMPMCQUEUE_H__
#define __SIR_BATCHMPMCQUEUE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>
#include <Core/Containers/SIRRawStorage.h>
#include <new>
#include <type_traits>

template <typename T>
class SIRBatchMpmcQueue
{
	static_assert( std::is_trivially_copyable<T>::value && std::is_trivially_default_constructible<T>::value,
	               "Batch queue requires trivially copyable default-constructible values" );

public:
	explicit SIRBatchMpmcQueue( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator )
	{
	}

	~SIRBatchMpmcQueue() noexcept
	{
		clear();
	}

	SIRBatchMpmcQueue( const SIRBatchMpmcQueue& ) = delete;
	SIRBatchMpmcQueue& operator=( const SIRBatchMpmcQueue& ) = delete;

	SIRContainerResult tryInitialize( uint32_t nCapacity )
	{
		if ( nCapacity < 2 || ( nCapacity & ( nCapacity - 1 ) ) != 0 || nCapacity > ( 1u << 30 ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		void *pMemory = nullptr;
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( Cell ), &nBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( Cell ), &pMemory );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		Cell *pCells = static_cast<Cell *>( pMemory );
		for ( uint32_t iIndex = 0; iIndex < nCapacity; ++iIndex ) {
			new ( pCells + iIndex ) Cell();
			pCells[iIndex].nSequence.store( iIndex, std::memory_order_relaxed );
		}
		clear();
		m_pCells = pCells;
		m_nCapacity = nCapacity;
		m_nEnqueue.store( 0 );
		m_nDequeue.store( 0 );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryEnqueueBatch( const T *pValues, uint32_t nCount ) noexcept
	{
		if ( pValues == nullptr || nCount == 0 || m_pCells == nullptr || nCount > m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nPosition = m_nEnqueue.load( std::memory_order_relaxed );
		for ( ;; ) {
			bool bFull = false;
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				const uint64_t nClaim = nPosition + iIndex;
				const uint64_t nSequence = m_pCells[nClaim & ( m_nCapacity - 1 )].nSequence.load( std::memory_order_acquire );
				if ( nSequence < nClaim ) {
					bFull = true;
					break;
				}
				if ( nSequence != nClaim ) {
					nPosition = m_nEnqueue.load( std::memory_order_relaxed );
					bFull = true;
					break;
				}
			}
			if ( bFull ) {
				// Try the current cursor once more to distinguish contention from capacity.
				const uint64_t nCurrent = m_nEnqueue.load( std::memory_order_relaxed );
				if ( nCurrent == nPosition ) {
					return SIRContainerResult::SIR_CONTAINER_FULL;
				}
				nPosition = nCurrent;
				continue;
			}
			uint64_t nExpected = nPosition;
			if ( m_nEnqueue.compare_exchange_weak( nExpected, nPosition + nCount, std::memory_order_acq_rel, std::memory_order_relaxed ) ) {
				break;
			}
			nPosition = nExpected;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			Cell& cell = m_pCells[( nPosition + iIndex ) & ( m_nCapacity - 1 )];
			cell.value = pValues[iIndex];
			cell.nSequence.store( nPosition + iIndex + 1, std::memory_order_release );
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryDequeueBatch( T *pValues, uint32_t nCount ) noexcept
	{
		if ( pValues == nullptr || nCount == 0 || m_pCells == nullptr || nCount > m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nPosition = m_nDequeue.load( std::memory_order_relaxed );
		for ( ;; ) {
			bool bEmpty = false;
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				const uint64_t nClaim = nPosition + iIndex;
				const uint64_t nSequence = m_pCells[nClaim & ( m_nCapacity - 1 )].nSequence.load( std::memory_order_acquire );
				if ( nSequence != nClaim + 1 ) {
					bEmpty = true;
					break;
				}
			}
			if ( bEmpty ) {
				const uint64_t nCurrent = m_nDequeue.load( std::memory_order_relaxed );
				if ( nCurrent == nPosition ) {
					return SIRContainerResult::SIR_CONTAINER_EMPTY;
				}
				nPosition = nCurrent;
				continue;
			}
			uint64_t nExpected = nPosition;
			if ( m_nDequeue.compare_exchange_weak( nExpected, nPosition + nCount, std::memory_order_acq_rel, std::memory_order_relaxed ) ) {
				break;
			}
			nPosition = nExpected;
		}
		for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
			Cell& cell = m_pCells[( nPosition + iIndex ) & ( m_nCapacity - 1 )];
			pValues[iIndex] = cell.value;
			cell.nSequence.store( nPosition + iIndex + m_nCapacity, std::memory_order_release );
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryEnqueue( const T& value ) noexcept
	{
		return tryEnqueueBatch( &value, 1 );
	}

	SIRContainerResult tryDequeue( T *pValue ) noexcept
	{
		return tryDequeueBatch( pValue, 1 );
	}

	void clear( void ) noexcept
	{
		for ( uint32_t iIndex = 0; iIndex < m_nCapacity; ++iIndex ) {
			m_pCells[iIndex].~Cell();
		}
		SIRRawStorage::deallocate( m_pAllocator, m_pCells, static_cast<size_t>( m_nCapacity * sizeof( Cell ) ), alignof( Cell ) );
		m_pCells = nullptr;
		m_nCapacity = 0;
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}

private:
	struct alignas( 64 ) Cell
	{
		std::atomic<uint64_t> nSequence{ 0 };
		T value{};
	};

	const sirContainerAllocator_t *m_pAllocator;
	Cell *m_pCells = nullptr;
	uint32_t m_nCapacity = 0;
	alignas( 64 ) std::atomic<uint64_t> m_nEnqueue{ 0 };
	alignas( 64 ) std::atomic<uint64_t> m_nDequeue{ 0 };
};
#endif /* __SIR_BATCHMPMCQUEUE_H__ */
