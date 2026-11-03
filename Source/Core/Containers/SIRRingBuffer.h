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

#ifndef __SIRENGINE_SIRRINGBUFFER_H__
#define __SIRENGINE_SIRRINGBUFFER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <new>
#include <utility>

// Bounded, single-threaded FIFO. Full insertion fails without overwriting live elements.
template <typename T>
class SIRRingBuffer
{
private:
	T *m_pData = nullptr;
	uint32_t m_nSize = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_iHead = 0;
	const sirContainerAllocator_t *m_pAllocator;

public:
	explicit SIRRingBuffer( const sirContainerAllocator_t *pAllocator ) noexcept : m_pAllocator( pAllocator )
	{
	}
	~SIRRingBuffer( void ) noexcept
	{
		clear();
		SIRRawStorage::deallocate( m_pAllocator, m_pData, ( size_t )m_nCapacity * sizeof( T ), alignof( T ) );
	}
	SIRRingBuffer( const SIRRingBuffer & ) = delete;
	SIRRingBuffer &operator=( const SIRRingBuffer & ) = delete;
	SIRRingBuffer( SIRRingBuffer && ) = delete;
	SIRRingBuffer &operator=( SIRRingBuffer && ) = delete;

	sirContainerResult_t tryInitialize( uint32_t nCapacity ) noexcept
	{
		if ( m_nCapacity != 0 || nCapacity == 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nBytes = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nBytes ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pMemory = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pMemory );
		if ( eResult == SIR_CONTAINER_OK ) {
			m_pData = ( T * )pMemory;
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
		return m_nSize == 0 ? nullptr : &m_pData[( ( uint64_t )m_iHead + m_nSize - 1 ) % m_nCapacity];
	}

	const T *back( void ) const noexcept
	{
		return m_nSize == 0 ? nullptr : &m_pData[( ( uint64_t )m_iHead + m_nSize - 1 ) % m_nCapacity];
	}

	template <typename... Args>
	sirContainerResult_t tryEmplaceFront( Args &&...args )
	{
		if ( m_nCapacity == 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == m_nCapacity ) {
			return SIR_CONTAINER_FULL;
		}
		uint32_t iNewHead = m_iHead == 0 ? m_nCapacity - 1 : m_iHead - 1;
		new ( m_pData + iNewHead ) T( std::forward<Args>( args )... );
		m_iHead = iNewHead;
		++m_nSize;
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryPushFront( const T &value )
	{
		return tryEmplaceFront( value );
	}
	sirContainerResult_t tryPushFront( T &&value )
	{
		return tryEmplaceFront( std::move( value ) );
	}

	template <typename... Args>
	sirContainerResult_t tryEmplaceBack( Args &&...args )
	{
		if ( m_nCapacity == 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == m_nCapacity ) {
			return SIR_CONTAINER_FULL;
		}
		uint32_t iSlot = ( ( uint64_t )m_iHead + m_nSize ) % m_nCapacity;
		new ( m_pData + iSlot ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryPushBack( const T &value )
	{
		return tryEmplaceBack( value );
	}
	sirContainerResult_t tryPushBack( T &&value )
	{
		return tryEmplaceBack( std::move( value ) );
	}

	sirContainerResult_t tryPopFront( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIR_CONTAINER_EMPTY;
		}
		m_pData[m_iHead].~T();
		m_iHead = ( m_iHead + 1 ) % m_nCapacity;
		--m_nSize;
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIR_CONTAINER_EMPTY;
		}
		uint32_t iLast = ( ( uint64_t )m_iHead + m_nSize - 1 ) % m_nCapacity;
		m_pData[iLast].~T();
		--m_nSize;
		return SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		while ( m_nSize != 0 ) {
			tryPopFront();
		}
		m_iHead = 0;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { 0, ( uint64_t )m_nCapacity * sizeof( T ), 0, 0, ( uint64_t )m_nSize * sizeof( T ) };
	}
};

#endif // __SIRENGINE_SIRRINGBUFFER_H__
