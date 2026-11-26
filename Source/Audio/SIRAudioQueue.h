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

#ifndef __SIR_AUDIOQUEUE_H__
#define __SIR_AUDIOQUEUE_H__

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

// Bounded MPMC queue; allocator, backing memory and producers must outlive
// their respective operations. No allocation or waiting in tryPush/tryPop.
// Initialization/shutdown are exclusive operations, never concurrent.
template <typename T>
class SIRAudioQueue
{
private:
	typedef struct sirSlot_s
	{
		std::atomic<uint64_t> nSequence;
		T value;
	} sirSlot_t;

	static_assert( std::is_trivially_copyable<T>::value, "Audio messages must be trivially copyable" );
	static_assert( std::is_trivially_default_constructible<T>::value, "Audio messages must be trivially default constructible" );

	sirSlot_t *m_pSlots = nullptr;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	uint32_t m_nCapacity = 0;
	std::atomic<uint64_t> m_nEnqueue = std::atomic<uint64_t>( 0 );
	std::atomic<uint64_t> m_nDequeue = std::atomic<uint64_t>( 0 );

public:
	SIRAudioQueue( void ) noexcept = default;
	~SIRAudioQueue( void ) noexcept;
	SIRAudioQueue( const SIRAudioQueue& ) = delete;
	SIRAudioQueue& operator=( const SIRAudioQueue& ) = delete;

	bool initialize( const sirContainerAllocator_t *pAllocator, uint32_t nCapacity ) noexcept;
	void shutdown( void ) noexcept;
	bool tryPush( const T& value ) noexcept;
	bool tryPop( T& value ) noexcept;
	uint32_t getCapacity( void ) const noexcept;
};

template <typename T>
SIRAudioQueue<T>::~SIRAudioQueue( void ) noexcept
{
	shutdown();
}

template <typename T>
bool SIRAudioQueue<T>::initialize( const sirContainerAllocator_t *pAllocator, uint32_t nCapacity ) noexcept
{
	if ( m_pSlots != nullptr || nCapacity < 2 || ( nCapacity & ( nCapacity - 1 ) ) != 0 || nCapacity > ( 1U << 30 ) ) {
		return false;
	}
	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( sirSlot_t ), &nBytes ) ) {
		return false;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBytes, alignof( sirSlot_t ), &pMemory ) != SIRContainerResult::SIR_CONTAINER_OK ) {
		return false;
	}
	m_pSlots = static_cast<sirSlot_t *>(pMemory);
	m_pAllocator = pAllocator;
	m_nCapacity = nCapacity;
	for ( uint32_t iSlot = 0; iSlot < nCapacity; ++iSlot ) {
		new ( m_pSlots + iSlot ) sirSlot_t();
		m_pSlots[iSlot].nSequence.store( iSlot, std::memory_order_relaxed );
	}
	m_nEnqueue.store( 0, std::memory_order_relaxed );
	m_nDequeue.store( 0, std::memory_order_relaxed );
	return true;
}

template <typename T>
void SIRAudioQueue<T>::shutdown( void ) noexcept
{
	if ( m_pSlots == nullptr ) {
		return;
	}
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		m_pSlots[iSlot].~sirSlot_t();
	}
	SIRRawStorage::deallocate( m_pAllocator, m_pSlots, static_cast<size_t>(m_nCapacity * sizeof(  sirSlot_t  )), alignof( sirSlot_t ) );
	m_pSlots = nullptr;
	m_pAllocator = nullptr;
	m_nCapacity = 0;
}

template <typename T>
bool SIRAudioQueue<T>::tryPush( const T& value ) noexcept
{
	if ( m_pSlots == nullptr ) {
		return false;
	}
	uint64_t nPosition = m_nEnqueue.load( std::memory_order_relaxed );
	for ( ;; ) {
		sirSlot_t& slot = m_pSlots[static_cast<uint32_t>(nPosition) & ( m_nCapacity - 1 )];
		const uint64_t nSequence = slot.nSequence.load( std::memory_order_acquire );
		const int64_t nDelta = static_cast<int64_t>( nSequence - nPosition );
		if ( nDelta == 0 ) {
			if ( m_nEnqueue.compare_exchange_weak( nPosition, nPosition + 1, std::memory_order_relaxed ) ) {
				slot.value = value;
				slot.nSequence.store( nPosition + 1, std::memory_order_release );
				return true;
			}
		} else if ( nDelta < 0 ) {
			return false;
		} else {
			nPosition = m_nEnqueue.load( std::memory_order_relaxed );
		}
	}
}

template <typename T>
bool SIRAudioQueue<T>::tryPop( T& value ) noexcept
{
	if ( m_pSlots == nullptr ) {
		return false;
	}
	uint64_t nPosition = m_nDequeue.load( std::memory_order_relaxed );
	for ( ;; ) {
		sirSlot_t& slot = m_pSlots[static_cast<uint32_t>(nPosition) & ( m_nCapacity - 1 )];
		const uint64_t nSequence = slot.nSequence.load( std::memory_order_acquire );
		const int64_t nDelta = static_cast<int64_t>(  nSequence - ( nPosition + 1 )  );
		if ( nDelta == 0 ) {
			if ( m_nDequeue.compare_exchange_weak( nPosition, nPosition + 1, std::memory_order_relaxed ) ) {
				value = slot.value;
				slot.nSequence.store( nPosition + m_nCapacity, std::memory_order_release );
				return true;
			}
		} else if ( nDelta < 0 ) {
			return false;
		} else {
			nPosition = m_nDequeue.load( std::memory_order_relaxed );
		}
	}
}

template <typename T>
uint32_t SIRAudioQueue<T>::getCapacity( void ) const noexcept
{
	return m_nCapacity;
}

#endif // __SIR_AUDIOQUEUE_H__
