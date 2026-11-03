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

#ifndef __SIRENGINE_SIRSTRING_H__
#define __SIRENGINE_SIRSTRING_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <stdint.h>
#include <string.h>
#include <utility>

typedef struct sirStringView_s
{
	const char *pBytes;
	uint32_t nBytes;
} sirStringView_t;

// UTF-8 byte storage. Inline and heap payloads share a physical slot.
// Allocations, including shrinking, use only the supplied memory domain.
class SIRString
{
private:
	static constexpr uint32_t INLINE_CAPACITY = 24;

	const sirContainerAllocator_t *m_pAllocator;
	uint32_t m_nLength;
	uint32_t m_nCapacity;
	union
	{
		char m_szInline[INLINE_CAPACITY];
		char *m_pHeap;
	};

	bool isInline( void ) const noexcept
	{
		return m_nCapacity == INLINE_CAPACITY;
	}

	char *data( void ) noexcept
	{
		return isInline() ? m_szInline : m_pHeap;
	}

	const char *data( void ) const noexcept
	{
		return isInline() ? m_szInline : m_pHeap;
	}

	sirContainerResult_t tryReserveBytes( uint32_t nCapacity )
	{
		if ( nCapacity <= m_nCapacity ) {
			return SIR_CONTAINER_OK;
		}
		void *pStorage = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nCapacity, alignof( char ), &pStorage );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		memcpy( pStorage, data(), m_nLength + 1 );
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pHeap, m_nCapacity, alignof( char ) );
		}
		m_pHeap = ( char * )pStorage;
		m_nCapacity = nCapacity;
		return SIR_CONTAINER_OK;
	}

	void release( void ) noexcept
	{
		if ( !isInline() ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pHeap, m_nCapacity, alignof( char ) );
		}
	}

public:
	explicit SIRString( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator ), m_nLength( 0 ), m_nCapacity( INLINE_CAPACITY )
	{
		m_szInline[0] = '\0';
	}

	~SIRString( void ) noexcept
	{
		release();
	}

	SIRString( const SIRString & ) = delete;
	SIRString &operator=( const SIRString & ) = delete;

	SIRString( SIRString &&other ) noexcept
	    : m_pAllocator( other.m_pAllocator ), m_nLength( other.m_nLength ), m_nCapacity( other.m_nCapacity )
	{
		if ( other.isInline() ) {
			if ( m_nLength < INLINE_CAPACITY ) {
				memcpy( m_szInline, other.m_szInline, m_nLength + 1 );
			} else {
				m_nLength = 0;
				m_szInline[0] = '\0';
			}
		} else {
			m_pHeap = other.m_pHeap;
		}
		other.m_nCapacity = INLINE_CAPACITY;
		other.m_nLength = 0;
		other.m_szInline[0] = '\0';
	}

	SIRString &operator=( SIRString &&other ) noexcept
	{
		if ( this != &other ) {
			release();
			m_pAllocator = other.m_pAllocator;
			m_nLength = other.m_nLength;
			m_nCapacity = other.m_nCapacity;
			if ( other.isInline() ) {
				if ( m_nLength < INLINE_CAPACITY ) {
					memcpy( m_szInline, other.m_szInline, m_nLength + 1 );
				} else {
					m_nLength = 0;
					m_szInline[0] = '\0';
				}
			} else {
				m_pHeap = other.m_pHeap;
			}
			other.m_nCapacity = INLINE_CAPACITY;
			other.m_nLength = 0;
			other.m_szInline[0] = '\0';
		}
		return *this;
	}

	uint32_t getByteLength( void ) const noexcept
	{
		return m_nLength;
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}

	const char *cStr( void ) const noexcept
	{
		return data();
	}

	sirStringView_t getView( void ) const noexcept
	{
		return { data(), m_nLength };
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { INLINE_CAPACITY, isInline() ? 0u : m_nCapacity, 0, 0, ( uint64_t )m_nLength + 1 };
	}

	sirContainerResult_t tryAppendBytes( const char *pBytes, uint32_t nBytes )
	{
		if ( nBytes == 0 ) {
			return SIR_CONTAINER_OK;
		}
		if ( pBytes == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( ( uint64_t )m_nLength + nBytes + 1 > UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		uintptr_t nStart = ( uintptr_t )data();
		uintptr_t nSource = ( uintptr_t )pBytes;
		bool bAliased = nSource >= nStart && nSource - nStart <= m_nLength;
		uint32_t iSource = bAliased ? ( uint32_t )( nSource - nStart ) : 0;
		if ( bAliased && ( uint64_t )iSource + nBytes > ( uint64_t )m_nLength + 1 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t nRequired = m_nLength + nBytes + 1;
		if ( nRequired > m_nCapacity ) {
			uint32_t nGrowth = 0;
			sirContainerResult_t eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, nRequired, &nGrowth );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			eResult = tryReserveBytes( nGrowth );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		char *pDestination = data() + m_nLength;
		if ( bAliased ) {
			memmove( pDestination, data() + iSource, nBytes );
		} else {
			memcpy( pDestination, pBytes, nBytes );
		}
		m_nLength += nBytes;
		if ( isInline() && m_nLength < INLINE_CAPACITY ) {
			m_szInline[m_nLength] = '\0';
		} else if ( !isInline() ) {
			m_pHeap[m_nLength] = '\0';
		} else {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryAssignBytes( const char *pBytes, uint32_t nBytes )
	{
		if ( pBytes == nullptr && nBytes != 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nBytes == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		uintptr_t nStart = ( uintptr_t )data();
		uintptr_t nSource = ( uintptr_t )pBytes;
		bool bAliased = pBytes != nullptr && nSource >= nStart && nSource - nStart <= m_nLength;
		uint32_t iSource = bAliased ? ( uint32_t )( nSource - nStart ) : 0;
		if ( bAliased && ( uint64_t )iSource + nBytes > ( uint64_t )m_nLength + 1 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nBytes + 1 > m_nCapacity ) {
			uint32_t nGrowth = 0;
			sirContainerResult_t eResult = SIRVectorStorage::calculateGrowth( m_nCapacity, nBytes + 1, &nGrowth );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			eResult = tryReserveBytes( nGrowth );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		if ( nBytes != 0 ) {
			if ( bAliased ) {
				memmove( data(), data() + iSource, nBytes );
			} else {
				memcpy( data(), pBytes, nBytes );
			}
		}
		m_nLength = nBytes;
		if ( isInline() && nBytes < INLINE_CAPACITY ) {
			m_szInline[nBytes] = '\0';
		} else if ( !isInline() ) {
			m_pHeap[nBytes] = '\0';
		} else {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryShrinkToFit( void )
	{
		if ( isInline() || m_nCapacity == m_nLength + 1 ) {
			return SIR_CONTAINER_OK;
		}
		if ( m_nLength + 1 <= INLINE_CAPACITY ) {
			char *pOld = m_pHeap;
			uint32_t nOldCapacity = m_nCapacity;
			memcpy( m_szInline, pOld, m_nLength + 1 );
			SIRRawStorage::deallocate( m_pAllocator, pOld, nOldCapacity, alignof( char ) );
			m_nCapacity = INLINE_CAPACITY;
			return SIR_CONTAINER_OK;
		}
		void *pMemory = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, m_nLength + 1, alignof( char ), &pMemory );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		memcpy( pMemory, m_pHeap, m_nLength + 1 );
		SIRRawStorage::deallocate( m_pAllocator, m_pHeap, m_nCapacity, alignof( char ) );
		m_pHeap = ( char * )pMemory;
		m_nCapacity = m_nLength + 1;
		return SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		m_nLength = 0;
		data()[0] = '\0';
	}
};

#endif // __SIRENGINE_SIRSTRING_H__
