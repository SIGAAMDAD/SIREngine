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

#ifndef __SIRENGINE_SIRINLINEVECTOR_H__
#define __SIRENGINE_SIRINLINEVECTOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <assert.h>
#include <new>
#include <type_traits>
#include <utility>

template <typename T, uint32_t N>
class SIRInlineVector
{
private:
	static_assert( N > 0, "Inline capacity must be positive" );
	alignas( T ) unsigned char m_szInlineStorage[sizeof( T ) * N];
	T *m_pData;
	uint32_t m_nSize;
	uint32_t m_nCapacity;
	const sirContainerAllocator_t *m_pAllocator;

	T *inlineData( void ) noexcept
	{
		return ( T * )m_szInlineStorage;
	}
	const T *inlineData( void ) const noexcept
	{
		return ( const T * )m_szInlineStorage;
	}
	bool isInline( void ) const noexcept
	{
		return m_pData == inlineData();
	}

	template <typename... Args>
	sirContainerResult_t growAndEmplace( Args &&...args )
	{
		if ( m_nSize == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		static_assert( SIRLifetimeOps<T>::CAN_RELOCATE, "Growth requires copy or nothrow move construction" );
		uint32_t nGrowth = 0;
		sirContainerResult_t eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, m_nSize + 1, &nGrowth );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nGrowth, sizeof( T ), &nBytes ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pStorage = nullptr;
		eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pStorage );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		T *pNewData = ( T * )pStorage;
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
		SIRLifetimeOps<T>::destroy( m_pData, m_nSize );
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pData, ( size_t )m_nCapacity * sizeof( T ), alignof( T ) );
		}
		m_pData = pNewData;
		m_nCapacity = nGrowth;
		++m_nSize;
		return SIR_CONTAINER_OK;
	}


public:
	explicit SIRInlineVector( const sirContainerAllocator_t *pAllocator ) noexcept
		: m_pData( inlineData() ), m_nSize( 0 ), m_nCapacity( N ), m_pAllocator( pAllocator )
	{
	}

	~SIRInlineVector( void ) noexcept
	{
		clear();
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pData, ( size_t )m_nCapacity * sizeof( T ), alignof( T ) );
		}
	}

	SIRInlineVector( const SIRInlineVector & ) = delete;
	SIRInlineVector &operator=( const SIRInlineVector & ) = delete;
	SIRInlineVector( SIRInlineVector && ) = delete;
	SIRInlineVector &operator=( SIRInlineVector && ) = delete;

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}
	bool usesInlineStorage( void ) const noexcept
	{
		return isInline();
	}
	T *data( void ) noexcept
	{
		return m_pData;
	}
	const T *data( void ) const noexcept
	{
		return m_pData;
	}
	T *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? m_pData + iIndex : nullptr;
	}
	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? m_pData + iIndex : nullptr;
	}
	T &operator[]( uint32_t iIndex ) noexcept
	{
		assert( iIndex < m_nSize );
		return m_pData[iIndex];
	}
	const T &operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < m_nSize );
		return m_pData[iIndex];
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { ( uint64_t )N * sizeof( T ), isInline() ? 0u : ( uint64_t )m_nCapacity * sizeof( T ), 0, 0,
			( uint64_t )m_nSize * sizeof( T ) };
	}

	sirContainerResult_t tryReserve( uint32_t nCapacity )
	{
		if ( nCapacity <= m_nCapacity ) {
			return SIR_CONTAINER_OK;
		}
		static_assert( SIRLifetimeOps<T>::CAN_RELOCATE, "Inline vector growth requires copy or nothrow move" );
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nBytes ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pMemory = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pMemory );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		T *pNew = ( T * )pMemory;
		try {
			SIRLifetimeOps<T>::relocate( pNew, m_pData, m_nSize );
		} catch ( ... ) {
			SIRRawStorage::deallocate( m_pAllocator, pNew, nBytes, alignof( T ) );
			throw;
		}
		SIRLifetimeOps<T>::destroy( m_pData, m_nSize );
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pData, ( size_t )m_nCapacity * sizeof( T ), alignof( T ) );
		}
		m_pData = pNew;
		m_nCapacity = nCapacity;
		return SIR_CONTAINER_OK;
	}

	template <typename... Args>
	sirContainerResult_t tryEmplaceBack( Args &&...args )
	{
		if ( m_nSize < m_nCapacity ) {
			new ( m_pData + m_nSize ) T( std::forward<Args>( args )... );
			++m_nSize;
			return SIR_CONTAINER_OK;
		}
		return growAndEmplace( std::forward<Args>( args )... );
	}

	sirContainerResult_t tryPushBack( const T &value )
	{
		if ( m_nSize == m_nCapacity ) {
			uintptr_t nAddress = ( uintptr_t )&value;
			uintptr_t nStart = ( uintptr_t )m_pData;
			if ( nAddress >= nStart && nAddress - nStart < ( size_t )m_nSize * sizeof( T ) ) {
				T staged( value );
				return tryEmplaceBack( std::move( staged ) );
			}
		}
		return tryEmplaceBack( value );
	}

	sirContainerResult_t tryPushBack( T &&value )
	{
		if ( m_nSize == m_nCapacity && m_pData != nullptr ) {
			uintptr_t nAddress = ( uintptr_t )&value;
			uintptr_t nStart = ( uintptr_t )m_pData;
			if ( nAddress >= nStart && nAddress - nStart < ( size_t )m_nSize * sizeof( T ) ) {
				if constexpr ( std::is_copy_constructible<T>::value ) {
					T staged( value );
					return tryEmplaceBack( std::move( staged ) );
				}
				return SIR_CONTAINER_INVALID_ARGUMENT;
			}
		}
		return tryEmplaceBack( std::move( value ) );
	}

	sirContainerResult_t tryResize( uint32_t nSize )
	{
		if ( nSize < m_nSize ) {
			for ( uint32_t iIndex = m_nSize; iIndex > nSize; --iIndex ) {
				m_pData[iIndex - 1].~T();
			}
			m_nSize = nSize;
			return SIR_CONTAINER_OK;
		}
		if ( nSize == m_nSize ) {
			return SIR_CONTAINER_OK;
		}
		if ( nSize > m_nCapacity ) {
			uint32_t nGrowth = 0;
			sirContainerResult_t eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, nSize, &nGrowth );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			eResult = tryReserve( nGrowth );
			if ( eResult != SIR_CONTAINER_OK ) {
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
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryEraseSwap( uint32_t iIndex ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Swap erase requires nothrow move assignment" );
		if ( iIndex >= m_nSize ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t iLast = m_nSize - 1;
		if ( iIndex != iLast ) {
			m_pData[iIndex] = std::move( m_pData[iLast] );
		}
		return tryPopBack();
	}

	sirContainerResult_t tryEraseOrdered( uint32_t iIndex ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Ordered erase requires nothrow move assignment" );
		if ( iIndex >= m_nSize ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		for ( uint32_t iShift = iIndex + 1; iShift < m_nSize; ++iShift ) {
			m_pData[iShift - 1] = std::move( m_pData[iShift] );
		}
		return tryPopBack();
	}

	sirContainerResult_t tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIR_CONTAINER_EMPTY;
		}
		--m_nSize;
		m_pData[m_nSize].~T();
		return SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		SIRLifetimeOps<T>::destroy( m_pData, m_nSize );
		m_nSize = 0;
	}
};

#endif // __SIRENGINE_SIRINLINEVECTOR_H__
