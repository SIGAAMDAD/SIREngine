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

#ifndef __SIR_VIRTUALVECTOR_H__
#define __SIR_VIRTUALVECTOR_H__

#include <Core/SIRPCH.h>
#include <iterator>
#include <stddef.h>
#include <assert.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>
#include <Core/Memory/SIRMemorySegment.h>

// Fixed maximum address reservation. Not thread-safe. The caller is responsible
// for selecting an OS reservation policy permitted by its memory budget.
template <typename T>
class SIRVirtualVector
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

	T *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < getSize() ? data() + iIndex : nullptr;
	}

	T& operator[]( uint32_t iIndex ) noexcept
	{
		assert( iIndex < getSize() );
		return data()[iIndex];
	}

	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < getSize() ? data() + iIndex : nullptr;
	}

	const T& operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < getSize() );
		return data()[iIndex];
	}

	using value_type = T;
	using difference_type = ptrdiff_t;
	using const_reference = const T&;
	using const_pointer = const T *;
	using const_iterator = const T *;
	using reference = T&;
	using pointer = T *;
	using iterator = pointer;
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


	T *begin( void ) noexcept
	{
		return data();
	}

	T *end( void ) noexcept
	{
		return data() == nullptr ? nullptr : data() + getSize();
	}

	const T *begin( void ) const noexcept
	{
		return data();
	}

	const T *end( void ) const noexcept
	{
		return data() == nullptr ? nullptr : data() + getSize();
	}

	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	T *tryFront( void ) noexcept
	{
		return tryGet( 0 );
	}

	T *tryBack( void ) noexcept
	{
		return empty() ? nullptr : tryGet( getSize() - 1 );
	}

	T& front( void ) noexcept
	{
		assert( !empty() );
		return *tryFront();
	}

	T& back( void ) noexcept
	{
		assert( !empty() );
		return *tryBack();
	}

	const T *tryFront( void ) const noexcept
	{
		return tryGet( 0 );
	}

	const T *tryBack( void ) const noexcept
	{
		return empty() ? nullptr : tryGet( getSize() - 1 );
	}

	const T& front( void ) const noexcept
	{
		assert( !empty() );
		return *tryFront();
	}

	const T& back( void ) const noexcept
	{
		assert( !empty() );
		return *tryBack();
	}

	explicit SIRVirtualVector( void ) noexcept = default;

	~SIRVirtualVector() noexcept
	{
		release();
	}

	SIRVirtualVector( const SIRVirtualVector& ) = delete;
	SIRVirtualVector& operator=( const SIRVirtualVector& ) = delete;

	SIRContainerResult tryInitialize( uint32_t nMaximum ) noexcept
	{
		if ( m_pData != nullptr || nMaximum == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const size_t nPage = SIRVirtualMemory::getPageSize();
		if ( nPage == 0 || alignof( T ) > nPage ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nBytes = 0;
		size_t nRounded = 0;
		if ( !SIRRawStorage::checkedMultiply( nMaximum, sizeof( T ), &nBytes ) || !roundUp( nBytes, nPage, &nRounded ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pMemory = SIRVirtualMemory::reserve( nRounded );
		if ( pMemory == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_OUT_OF_MEMORY;
		}
		m_pData = static_cast<T *>( pMemory );
		m_nMaximum = nMaximum;
		m_nPageBytes = nPage;
		m_nReservationBytes = nRounded;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	// The segment is exclusively owned for this vector's lifetime. GRDG and
	// other allocators must not commit into the same range.
	SIRContainerResult tryInitialize( SIRMemorySegment& rSegment, uint32_t nMaximum ) noexcept
	{
		if ( m_pData != nullptr || nMaximum == 0 || rSegment.getBase() == nullptr || rSegment.getCommittedBytes() != 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const size_t nPage = static_cast<size_t>( rSegment.getPageSize() );
		if ( nPage == 0 || alignof( T ) > nPage ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nBytes = 0;
		size_t nRounded = 0;
		if ( !SIRRawStorage::checkedMultiply( nMaximum, sizeof( T ), &nBytes ) || !roundUp( nBytes, nPage, &nRounded ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( nRounded > rSegment.getReservedBytes() || nRounded > rSegment.getCommitLimitBytes() ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		m_pData = static_cast<T *>( rSegment.getBase() );
		m_pSegment = &rSegment;
		m_nMaximum = nMaximum;
		m_nPageBytes = nPage;
		m_nReservationBytes = nRounded;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryReserve( uint32_t nCapacity ) noexcept
	{
		if ( m_pData == nullptr || nCapacity > m_nMaximum ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		size_t nBytes = 0;
		size_t nRounded = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nBytes ) || !roundUp( nBytes, m_nPageBytes, &nRounded ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( nRounded <= m_nCommittedBytes ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( m_pSegment != nullptr
		         ? !m_pSegment->commit( m_nCommittedBytes, nRounded - m_nCommittedBytes )
		         : !SIRVirtualMemory::commit( reinterpret_cast<uint8_t *>( m_pData ) + m_nCommittedBytes, nRounded - m_nCommittedBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_OUT_OF_MEMORY;
		}
		m_nCommittedBytes = nRounded;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceBack( Args&&...args )
	{
		if ( m_nSize == m_nMaximum ) {
			return SIRContainerResult::SIR_CONTAINER_FULL;
		}
		const SIRContainerResult eResult = tryReserve( m_nSize + 1 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		new ( m_pData + m_nSize ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		m_pData[--m_nSize].~T();
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool tryDecommitTail( void ) noexcept
	{
		if ( m_pData == nullptr ) {
			return false;
		}
		size_t nKeep = 0;
		if ( !roundUp( static_cast<size_t>( m_nSize * sizeof( T ) ), m_nPageBytes, &nKeep ) ) {
			return false;
		}
		if ( nKeep >= m_nCommittedBytes ) {
			return true;
		}
		if ( m_pSegment != nullptr
		         ? !m_pSegment->decommit( nKeep, m_nCommittedBytes - nKeep )
		         : !SIRVirtualMemory::decommit( reinterpret_cast<uint8_t *>( m_pData ) + nKeep, m_nCommittedBytes - nKeep ) ) {
			return false;
		}
		m_nCommittedBytes = nKeep;
		return true;
	}

	void clear( void ) noexcept
	{
		while ( m_nSize != 0 ) {
			m_pData[--m_nSize].~T();
		}
	}

	void release( void ) noexcept
	{
		clear();
		if ( m_pData != nullptr ) {
			if ( m_pSegment != nullptr ) {
				if ( m_nCommittedBytes != 0 && !m_pSegment->decommit( 0, m_nCommittedBytes ) ) {
					abort(); // Unable to return exclusive pages to their memory manager.
				}
			} else {
				SIRVirtualMemory::release( m_pData, m_nReservationBytes );
			}
		}
		m_pData = nullptr;
		m_nReservationBytes = 0;
		m_nCommittedBytes = 0;
		m_nMaximum = 0;
		m_pSegment = nullptr;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint32_t getMaximum( void ) const noexcept
	{
		return m_nMaximum;
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return static_cast<uint32_t>( m_nCommittedBytes / sizeof( T ) );
	}

	size_t getReservedBytes( void ) const noexcept
	{
		return m_nReservationBytes;
	}

	size_t getCommittedBytes( void ) const noexcept
	{
		return m_nCommittedBytes;
	}

	const T *data( void ) const noexcept
	{
		return m_pData;
	}

	T *data( void ) noexcept
	{
		return m_pData;
	}

	SIRReadOnlyVectorView<T> getReadOnlyView( void ) const noexcept
	{
		return { m_pData, m_nSize };
	}

private:
	T *m_pData = nullptr;
	uint32_t m_nSize = 0;
	uint32_t m_nMaximum = 0;
	size_t m_nReservationBytes = 0;
	size_t m_nCommittedBytes = 0;
	size_t m_nPageBytes = 0;
	SIRMemorySegment *m_pSegment = nullptr;

	static bool roundUp( size_t nBytes, size_t nPage, size_t *pRounded ) noexcept
	{
		if ( nPage == 0 || nBytes > SIZE_MAX - nPage + 1 ) {
			return false;
		}
		*pRounded = ( nBytes + nPage - 1 ) / nPage * nPage;
		return true;
	}
};

#endif // __SIR_VIRTUALVECTOR_H__
