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

#ifndef __SIR_RINGBUFFER_H__
#define __SIR_RINGBUFFER_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRIndexedContainerIterator.h>
#include <iterator>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <new>
#include <type_traits>
#include <utility>

// Bounded, single-threaded FIFO. Full insertion fails without overwriting live elements.
template <typename T>
class SIRRingBuffer
{
public:
	using size_type = uint32_t;

	uint32_t size( void ) const noexcept
	{
		return getSize();
	}

	bool empty( void ) const noexcept
	{
		return getSize() == 0;
	}

	bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	size_type capacity( void ) const noexcept
	{
		return getCapacity();
	}

	using value_type = T;
	using difference_type = ptrdiff_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T *;
	using const_pointer = const T *;
	using iterator = SIRIndexedContainerIterator<SIRRingBuffer<T>, false>;
	using const_iterator = SIRIndexedContainerIterator<SIRRingBuffer<T>, true>;

	iterator begin( void ) noexcept
	{
		return { this, 0 };
	}
	iterator end( void ) noexcept
	{
		return { this, size() };
	}
	const_iterator begin( void ) const noexcept
	{
		return { this, 0 };
	}
	const_iterator end( void ) const noexcept
	{
		return { this, size() };
	}
	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}
	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	reverse_iterator rbegin( void ) noexcept
	{
		return reverse_iterator( end() );
	}
	reverse_iterator rend( void ) noexcept
	{
		return reverse_iterator( begin() );
	}
	const_reverse_iterator rbegin( void ) const noexcept
	{
		return const_reverse_iterator( end() );
	}
	const_reverse_iterator rend( void ) const noexcept
	{
		return const_reverse_iterator( begin() );
	}
	const_reverse_iterator crbegin( void ) const noexcept
	{
		return rbegin();
	}
	const_reverse_iterator crend( void ) const noexcept
	{
		return rend();
	}

	T& operator[]( uint32_t iIndex ) noexcept
	{
		assert( iIndex < size() );
		return *tryGet( iIndex );
	}

	const T& operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < size() );
		return *tryGet( iIndex );
	}

	T *tryAt( uint32_t iIndex ) noexcept
	{
		return tryGet( iIndex );
	}
	const T *tryAt( uint32_t iIndex ) const noexcept
	{
		return tryGet( iIndex );
	}


	T *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < getSize() ? m_pData + wrapIndex( static_cast<uint64_t>( m_iHead ) + iIndex ) : nullptr;
	}

	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < getSize() ? m_pData + wrapIndex( static_cast<uint64_t>( m_iHead ) + iIndex ) : nullptr;
	}

	explicit SIRRingBuffer( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator )
	{
	}

	~SIRRingBuffer() noexcept
	{
		clearAndRelease();
	}

	SIRRingBuffer( const SIRRingBuffer& ) = delete;
	SIRRingBuffer& operator=( const SIRRingBuffer& ) = delete;
	SIRRingBuffer( SIRRingBuffer&& ) = delete;
	SIRRingBuffer& operator=( SIRRingBuffer&& ) = delete;

	SIRContainerResult tryInitialize( uint32_t nCapacity ) noexcept
	{
		if ( m_nCapacity != 0 || nCapacity == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pMemory = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pMemory );
		if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
			m_pData = static_cast<T *>( pMemory );
			m_nCapacity = nCapacity;
		}
		return eResult;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}

	bool isFull( void ) const noexcept
	{
		return m_nSize == m_nCapacity && m_nCapacity != 0;
	}

	T *front( void ) noexcept
	{
		return m_nSize == 0 ? nullptr : &m_pData[m_iHead];
	}

	const T *front( void ) const noexcept
	{
		return m_nSize == 0 ? nullptr : &m_pData[m_iHead];
	}

	T *back( void ) noexcept
	{
		return m_nSize == 0 ? nullptr : &m_pData[wrapIndex( static_cast<uint64_t>( m_iHead ) + m_nSize - 1 )];
	}

	const T *back( void ) const noexcept
	{
		return m_nSize == 0 ? nullptr : &m_pData[wrapIndex( static_cast<uint64_t>( m_iHead ) + m_nSize - 1 )];
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceFront( Args&&...args )
	{
		if ( m_nCapacity == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_FULL;
		}
		uint32_t iNewHead = m_iHead == 0 ? m_nCapacity - 1 : m_iHead - 1;
		new ( m_pData + iNewHead ) T( std::forward<Args>( args )... );
		m_iHead = iNewHead;
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPushFront( const T& value )
	{
		return tryEmplaceFront( value );
	}

	SIRContainerResult tryPushFront( T&& value )
	{
		return tryEmplaceFront( std::move( value ) );
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceBack( Args&&...args )
	{
		if ( m_nCapacity == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_FULL;
		}
		uint32_t iSlot = wrapIndex( static_cast<uint64_t>( m_iHead ) + m_nSize );
		new ( m_pData + iSlot ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPushBack( const T& value )
	{
		return tryEmplaceBack( value );
	}

	SIRContainerResult tryPushBack( T&& value )
	{
		return tryEmplaceBack( std::move( value ) );
	}

	// Batched API for trivially copyable elements. The entire operation either
	// succeeds or leaves the ring unchanged. Source/destination aliasing with
	// this ring's backing allocation is rejected before any modification.
	SIRContainerResult tryPushBackMany( const T *pItems, uint32_t nCount ) noexcept
	{
		static_assert( std::is_trivially_copyable<T>::value, "Bulk push requires trivially copyable elements" );
		if ( nCount == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( pItems == nullptr || m_nCapacity == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount > m_nCapacity - m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_FULL;
		}
		if ( overlapsStorage( pItems, nCount ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		const uint32_t iTail = wrapIndex( static_cast<uint64_t>( m_iHead ) + m_nSize );
		const uint32_t nFirst = nCount < m_nCapacity - iTail ? nCount : m_nCapacity - iTail;
		for ( uint32_t iIndex = 0; iIndex < nFirst; ++iIndex ) {
			new ( m_pData + iTail + iIndex ) T( pItems[iIndex] );
		}
		for ( uint32_t iIndex = nFirst; iIndex < nCount; ++iIndex ) {
			new ( m_pData + iIndex - nFirst ) T( pItems[iIndex] );
		}
		m_nSize += nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPopFrontMany( T *pOutput, uint32_t nCount ) noexcept
	{
		static_assert( std::is_trivially_copyable<T>::value, "Bulk pop requires trivially copyable elements" );
		if ( nCount == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( pOutput == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount > m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		if ( overlapsStorage( pOutput, nCount ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}

		const uint32_t nFirst = nCount < m_nCapacity - m_iHead ? nCount : m_nCapacity - m_iHead;
		for ( uint32_t iIndex = 0; iIndex < nFirst; ++iIndex ) {
			pOutput[iIndex] = m_pData[m_iHead + iIndex];
			m_pData[m_iHead + iIndex].~T();
		}
		for ( uint32_t iIndex = nFirst; iIndex < nCount; ++iIndex ) {
			pOutput[iIndex] = m_pData[iIndex - nFirst];
			m_pData[iIndex - nFirst].~T();
		}
		m_iHead = wrapIndex( static_cast<uint64_t>( m_iHead ) + nCount );
		m_nSize -= nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPopFront( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		m_pData[m_iHead].~T();
		m_iHead = wrapIndex( static_cast<uint64_t>( m_iHead ) + 1 );
		--m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		uint32_t iLast = wrapIndex( static_cast<uint64_t>( m_iHead ) + m_nSize - 1 );
		m_pData[iLast].~T();
		--m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		while ( m_nSize != 0 ) {
			tryPopFront();
		}
		m_iHead = 0;
	}

	void clearAndRelease( void ) noexcept
	{
		clear();
		SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>( m_nCapacity * sizeof( T ) ), alignof( T ) );
		m_pData = nullptr;
		m_nCapacity = 0;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { 0, static_cast<uint64_t>( m_nCapacity * sizeof( T ) ), 0, 0, static_cast<uint64_t>( m_nSize * sizeof( T ) ) };
	}

private:
	T *m_pData = nullptr;
	uint32_t m_nSize = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_iHead = 0;
	const sirContainerAllocator_t *m_pAllocator;

	bool overlapsStorage( const T *pItems, uint32_t nCount ) const noexcept
	{
		const uintptr_t nStorage = reinterpret_cast<uintptr_t>( m_pData );
		const uintptr_t nInput = reinterpret_cast<uintptr_t>( pItems );
		const uint64_t nStorageBytes = static_cast<uint64_t>( m_nCapacity * sizeof( T ) );
		const uint64_t nInputBytes = static_cast<uint64_t>( nCount * sizeof( T ) );
		if ( nInput >= nStorage ) {
			return nInput - nStorage < nStorageBytes;
		}
		return nStorage - nInput < nInputBytes;
	}

	uint32_t wrapIndex( uint64_t iIndex ) const noexcept
	{
		if ( ( m_nCapacity & ( m_nCapacity - 1 ) ) == 0 ) {
			return static_cast<uint32_t>( iIndex ) & ( m_nCapacity - 1 );
		}
		return static_cast<uint32_t>( iIndex >= m_nCapacity ? iIndex - m_nCapacity : iIndex );
	}
};

#endif // __SIR_RINGBUFFER_H__
