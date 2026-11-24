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

#ifndef __SIR_VECTOR_H__
#define __SIR_VECTOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <Core/Containers/SIRWriteOnlyVectorView.h>
#include <assert.h>
#include <string.h>
#include <new>
#include <type_traits>
#include <utility>

template <typename T>
class SIRVectorAppendWriter;

template <typename T>
class SIRVector
{
private:
	friend class SIRVectorAppendWriter<T>;
	T *m_pData;
	uint32_t m_nSize;
	uint32_t m_nCapacity;
	const sirContainerAllocator_t *m_pAllocator;

	template <typename... Args>
	SIRContainerResult growAndEmplace( Args&&...args )
	{
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		static_assert( SIRLifetimeOps<T>::CAN_RELOCATE, "Growth requires copy or nothrow move construction" );
		uint32_t nGrowth = 0;
		SIRContainerResult eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, m_nSize + 1, &nGrowth );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nGrowth, sizeof( T ), &nBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pStorage = nullptr;
		eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pStorage );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		T *pNewData = static_cast<T *>(pStorage);
		// Construct before relocating: arguments may refer into the old array.
		try {
			new ( pNewData + m_nSize ) T( std::forward<Args>( args )... );
		} catch ( ... ) {
			SIRRawStorage::deallocate( m_pAllocator, pNewData, nBytes, alignof( T ) );
			throw;
		}
		try {
			SIRLifetimeOps<T>::relocate( pNewData, m_pData, m_nSize );
		} catch ( ... ) {
			pNewData[m_nSize].~T();
			SIRRawStorage::deallocate( m_pAllocator, pNewData, nBytes, alignof( T ) );
			throw;
		}
		SIRLifetimeOps<T>::destroySourceAfterRelocate( m_pData, m_nSize );
		SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), alignof( T ) );
		m_pData = pNewData;
		m_nCapacity = nGrowth;
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

public:
	explicit SIRVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pData( nullptr ), m_nSize( 0 ), m_nCapacity( 0 ), m_pAllocator( pAllocator )
	{
	}

	~SIRVector( void ) noexcept
	{
		clear();
		SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), alignof( T ) );
	}

	SIRVector( const SIRVector& ) = delete;
	SIRVector& operator=( const SIRVector& ) = delete;

	SIRVector( SIRVector&& other ) noexcept
	    : m_pData( other.m_pData ), m_nSize( other.m_nSize ), m_nCapacity( other.m_nCapacity ), m_pAllocator( other.m_pAllocator )
	{
		other.m_pData = nullptr;
		other.m_nSize = 0;
		other.m_nCapacity = 0;
	}

	SIRVector& operator=( SIRVector&& other ) noexcept
	{
		if ( this != &other ) {
			clear();
			SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), alignof( T ) );
			m_pData = other.m_pData;
			m_nSize = other.m_nSize;
			m_nCapacity = other.m_nCapacity;
			m_pAllocator = other.m_pAllocator;
			other.m_pData = nullptr;
			other.m_nSize = 0;
			other.m_nCapacity = 0;
		}
		return *this;
	}

	SIRReadOnlyVectorView<T> getReadOnlyView( void ) const noexcept
	{
		return { m_pData, m_nSize };
	}
	SIRWriteOnlyVectorView<T> getWriteOnlyView( void ) noexcept
	{
		return { m_pData, m_nSize };
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}
	const sirContainerAllocator_t *getAllocator( void ) const noexcept
	{
		return m_pAllocator;
	}
	bool isEmpty( void ) const noexcept
	{
		return m_nSize == 0;
	}
	T *data( void ) noexcept
	{
		return m_pData;
	}
	const T *data( void ) const noexcept
	{
		return m_pData;
	}
	T *begin( void ) noexcept
	{
		return m_pData;
	}
	const T *begin( void ) const noexcept
	{
		return m_pData;
	}
	T *end( void ) noexcept
	{
		return m_pData == nullptr ? nullptr : m_pData + m_nSize;
	}
	const T *end( void ) const noexcept
	{
		return m_pData == nullptr ? nullptr : m_pData + m_nSize;
	}
	T *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? m_pData + iIndex : nullptr;
	}
	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? m_pData + iIndex : nullptr;
	}
	T& operator[]( uint32_t iIndex ) noexcept
	{
		assert( iIndex < m_nSize );
		return m_pData[iIndex];
	}
	const T& operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < m_nSize );
		return m_pData[iIndex];
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { 0, static_cast<uint64_t>(m_nCapacity * sizeof(  T  )), 0, 0, static_cast<uint64_t>(m_nSize * sizeof(  T  )) };
	}

	SIRContainerResult tryReserve( uint32_t nCapacity )
	{
		if ( nCapacity <= m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		static_assert( SIRLifetimeOps<T>::CAN_RELOCATE, "SIRVector growth requires copy or nothrow move construction" );
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pNewStorage = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pNewStorage );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		T *pNew = static_cast<T *>(pNewStorage);
		try {
			SIRLifetimeOps<T>::relocate( pNew, m_pData, m_nSize );
		} catch ( ... ) {
			SIRRawStorage::deallocate( m_pAllocator, pNew, nBytes, alignof( T ) );
			throw;
		}
		SIRLifetimeOps<T>::destroySourceAfterRelocate( m_pData, m_nSize );
		SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), alignof( T ) );
		m_pData = pNew;
		m_nCapacity = nCapacity;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	// Reclaim reserved domain memory without changing the logical contents.
	// An allocation or copy failure preserves the original storage.
	SIRContainerResult tryShrinkToFit( void )
	{
		if ( m_nSize == m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( m_nSize == 0 ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), alignof( T ) );
			m_pData = nullptr;
			m_nCapacity = 0;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		static_assert( SIRLifetimeOps<T>::CAN_RELOCATE, "Shrinking requires copy or nothrow move construction" );
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( m_nSize, sizeof( T ), &nBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pMemory = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pMemory );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		T *pNew = static_cast<T *>(pMemory);
		try {
			SIRLifetimeOps<T>::relocate( pNew, m_pData, m_nSize );
		} catch ( ... ) {
			SIRRawStorage::deallocate( m_pAllocator, pNew, nBytes, alignof( T ) );
			throw;
		}
		SIRLifetimeOps<T>::destroySourceAfterRelocate( m_pData, m_nSize );
		SIRRawStorage::deallocate( m_pAllocator, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), alignof( T ) );
		m_pData = pNew;
		m_nCapacity = m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceBack( Args&&...args )
	{
		if ( m_nSize < m_nCapacity ) {
			new ( m_pData + m_nSize ) T( std::forward<Args>( args )... );
			++m_nSize;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		return growAndEmplace( std::forward<Args>( args )... );
	}

	SIRContainerResult tryPushBack( const T& value )
	{
		if ( m_nSize == m_nCapacity && m_pData != nullptr ) {
			uintptr_t nAddress = reinterpret_cast<uintptr_t>(&value);
			uintptr_t nStart = reinterpret_cast<uintptr_t>(m_pData);
			if ( nAddress >= nStart && nAddress - nStart < static_cast<size_t>(m_nSize * sizeof(  T  )) ) {
				T staged( value );
				return tryEmplaceBack( std::move( staged ) );
			}
		}
		return tryEmplaceBack( value );
	}

	SIRContainerResult tryPushBack( T&& value )
	{
		if ( m_nSize == m_nCapacity && m_pData != nullptr ) {
			uintptr_t nAddress = reinterpret_cast<uintptr_t>(&value);
			uintptr_t nStart = reinterpret_cast<uintptr_t>(m_pData);
			if ( nAddress >= nStart && nAddress - nStart < static_cast<size_t>(m_nSize * sizeof(  T  )) ) {
				if constexpr ( std::is_copy_constructible<T>::value ) {
					T staged( value );
					return tryEmplaceBack( std::move( staged ) );
				}
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
		}
		return tryEmplaceBack( std::move( value ) );
	}

	// Optional allocator in-place expansion, avoiding element relocation.
	// The callback must preserve pointer identity and update allocator accounting.
	SIRContainerResult tryReserveExtended( uint32_t nCapacity, const sirContainerAllocatorCapabilities_t *pCapabilities )
	{
		if ( nCapacity <= m_nCapacity ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		size_t nNewBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nNewBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		size_t nActual = 0;
		if ( SIRRawStorage::tryExpandInPlace( pCapabilities, m_pData, static_cast<size_t>(m_nCapacity * sizeof(  T  )), nNewBytes, alignof( T ),
		                                      &nActual ) ) {
			// Never exceed the exact usable size provided by the allocator.
			m_nCapacity = nCapacity;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		return tryReserve( nCapacity );
	}

	SIRContainerResult tryAppendRange( const T *pValues, uint32_t nCount )
	{
		if ( nCount == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( pValues == nullptr || nCount > UINT32_MAX - m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		// Alias rejection is checked before any reallocation.
		if ( m_pData != nullptr ) {
			const uintptr_t nPointer = reinterpret_cast<uintptr_t>(pValues);
			const uintptr_t nStart = reinterpret_cast<uintptr_t>(m_pData);
			if ( nPointer >= nStart && nPointer - nStart < static_cast<size_t>(m_nSize * sizeof(  T  )) ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
		}
		const uint32_t nRequired = m_nSize + nCount;
		if ( nRequired > m_nCapacity ) {
			uint32_t nGrowth = 0;
			SIRContainerResult eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, nRequired, &nGrowth );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			eResult = tryReserve( nGrowth );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		SIRLifetimeOps<T>::copyConstructRange( m_pData + m_nSize, pValues, nCount );
		m_nSize = nRequired;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	// Transactional insertion: the original array remains valid on any copy or
	// allocation failure, including when the source range aliases this vector.
	// Copy-only semantics are explicit; move-only insertion uses emplace.
	SIRContainerResult tryInsertRange( uint32_t iIndex, const T *pValues, uint32_t nCount )
	{
		static_assert( std::is_copy_constructible<T>::value, "Range insertion requires copy construction" );
		if ( iIndex > m_nSize || ( nCount != 0 && pValues == nullptr ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( nCount > UINT32_MAX - m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const uint32_t nRequired = m_nSize + nCount;
		uint32_t nTarget = m_nCapacity;
		if ( nRequired > nTarget ) {
			const SIRContainerResult eResult = SIRVectorStorage::calculateGrowth( nTarget, nRequired, &nTarget );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		SIRVector<T> replacement( m_pAllocator );
		SIRContainerResult eResult = replacement.tryReserve( nTarget );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t i = 0; i < iIndex; ++i ) {
			eResult = replacement.tryEmplaceBack( m_pData[i] );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		for ( uint32_t i = 0; i < nCount; ++i ) {
			eResult = replacement.tryEmplaceBack( pValues[i] );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		for ( uint32_t i = iIndex; i < m_nSize; ++i ) {
			eResult = replacement.tryEmplaceBack( m_pData[i] );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		*this = std::move( replacement );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	// Transactional range assignment preserves the former contents on copy
	// failure. The input range may refer to this vector's live elements.
	SIRContainerResult tryAssignRange( const T *pValues, uint32_t nCount )
	{
		static_assert( std::is_copy_constructible<T>::value, "Range assignment requires copy construction" );
		if ( nCount != 0 && pValues == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<T> replacement( m_pAllocator );
		SIRContainerResult eResult = replacement.tryReserve( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = replacement.tryAppendRange( pValues, nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		*this = std::move( replacement );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryEraseRange( uint32_t iFirst, uint32_t nCount ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Range erase requires nothrow move assignment" );
		if ( iFirst > m_nSize || nCount > m_nSize - iFirst ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		const uint32_t nRemaining = m_nSize - iFirst - nCount;
		if constexpr ( std::is_trivially_copyable<T>::value ) {
			if ( nRemaining != 0 ) {
				memmove( m_pData + iFirst, m_pData + iFirst + nCount, static_cast<size_t>(nRemaining * sizeof(  T  )) );
			}
		} else {
			for ( uint32_t iIndex = 0; iIndex < nRemaining; ++iIndex ) {
				m_pData[iFirst + iIndex] = std::move( m_pData[iFirst + nCount + iIndex] );
			}
		}
		SIRLifetimeOps<T>::destroy( m_pData + m_nSize - nCount, nCount );
		m_nSize -= nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryResize( uint32_t nSize )
	{
		if ( nSize < m_nSize ) {
			for ( uint32_t iIndex = m_nSize; iIndex > nSize; --iIndex ) {
				m_pData[iIndex - 1].~T();
			}
			m_nSize = nSize;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( nSize == m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( nSize > m_nCapacity ) {
			uint32_t nGrowth = 0;
			SIRContainerResult eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, nSize, &nGrowth );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			eResult = tryReserve( nGrowth );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		uint32_t nConstructed = m_nSize;
		try {
			for ( ; nConstructed < nSize; ++nConstructed ) {
				new ( m_pData + nConstructed ) T();
			}
		} catch ( ... ) {
			for ( uint32_t iIndex = nConstructed; iIndex > m_nSize; --iIndex ) {
				m_pData[iIndex - 1].~T();
			}
			throw;
		}
		m_nSize = nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryEraseSwap( uint32_t iIndex ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Swap erase requires nothrow move assignment" );
		if ( iIndex >= m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t iLast = m_nSize - 1;
		if ( iIndex != iLast ) {
			m_pData[iIndex] = std::move( m_pData[iLast] );
		}
		return tryPopBack();
	}

	SIRContainerResult tryEraseOrdered( uint32_t iIndex ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Ordered erase requires nothrow move assignment" );
		if ( iIndex >= m_nSize ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
#if defined( __clang__ )
		if constexpr ( std::is_trivially_copyable<T>::value && sizeof( T ) >= 16 ) {
			const uint32_t nTrailing = m_nSize - iIndex - 1;
			if ( nTrailing != 0 ) {
				memmove( m_pData + iIndex, m_pData + iIndex + 1, static_cast<size_t>(nTrailing * sizeof(  T  )) );
			}
			return tryPopBack();
		}
#endif
		for ( uint32_t iShift = iIndex + 1; iShift < m_nSize; ++iShift ) {
			m_pData[iShift - 1] = std::move( m_pData[iShift] );
		}
		return tryPopBack();
	}

	SIRContainerResult tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		--m_nSize;
		m_pData[m_nSize].~T();
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		SIRLifetimeOps<T>::destroy( m_pData, m_nSize );
		m_nSize = 0;
	}
};

#endif // __SIR_VECTOR_H__
