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

#ifndef __SIRENGINE_SIRPAGEDARRAY_H__
#define __SIRENGINE_SIRPAGEDARRAY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <new>
#include <utility>

// Pages never relocate; an append may grow the pointer directory.
template <typename T, uint32_t PAGE_ELEMENTS = 256>
class SIRPagedArray
{
private:
	static_assert( PAGE_ELEMENTS > 0, "Page capacity must be positive" );
	static_assert( PAGE_ELEMENTS <= UINT32_MAX / 2, "Page capacity must be bounded" );
	SIRVector<T *> m_pages;
	const sirContainerAllocator_t *m_pAllocator;
	uint32_t m_nSize = 0;

public:
	explicit SIRPagedArray( const sirContainerAllocator_t *pAllocator ) noexcept : m_pages( pAllocator ), m_pAllocator( pAllocator )
	{
	}
	~SIRPagedArray( void ) noexcept
	{
		clearAndRelease();
	}
	SIRPagedArray( const SIRPagedArray& ) = delete;
	SIRPagedArray& operator=( const SIRPagedArray& ) = delete;
	SIRPagedArray( SIRPagedArray&& ) = delete;
	SIRPagedArray& operator=( SIRPagedArray&& ) = delete;

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	uint32_t getPageCount( void ) const noexcept
	{
		return m_pages.getSize();
	}
	T *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? m_pages[iIndex / PAGE_ELEMENTS] + ( iIndex % PAGE_ELEMENTS ) : nullptr;
	}
	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? m_pages[iIndex / PAGE_ELEMENTS] + ( iIndex % PAGE_ELEMENTS ) : nullptr;
	}
	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		const sirContainerFootprint_t index = m_pages.getFootprint();
		return { 0, index.nLiveDomainBytes + ( uint64_t )m_pages.getSize() * PAGE_ELEMENTS * sizeof( T ), 0, 0,
		         ( uint64_t )m_nSize * sizeof( T ) };
	}

	template <typename... Args>
	sirContainerResult_t tryEmplaceBack( Args&&...args )
	{
		if ( m_nSize == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( m_nSize / PAGE_ELEMENTS == m_pages.getSize() ) {
			size_t nBytes = 0;
			if ( !SIRRawStorage::checkedMultiply( PAGE_ELEMENTS, sizeof( T ), &nBytes ) ) {
				return SIR_CONTAINER_SIZE_OVERFLOW;
			}
			void *pMemory = nullptr;
			sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( T ), &pMemory );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			eResult = m_pages.tryPushBack( ( T * )pMemory );
			if ( eResult != SIR_CONTAINER_OK ) {
				SIRRawStorage::deallocate( m_pAllocator, pMemory, nBytes, alignof( T ) );
				return eResult;
			}
		}
		T *pSlot = m_pages[m_nSize / PAGE_ELEMENTS] + m_nSize % PAGE_ELEMENTS;
		new ( pSlot ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIR_CONTAINER_EMPTY;
		}
		--m_nSize;
		m_pages[m_nSize / PAGE_ELEMENTS][m_nSize % PAGE_ELEMENTS].~T();
		return SIR_CONTAINER_OK;
	}
	// Free empty tail pages without relocating any live elements.
	// A failed directory shrink leaves an intact, smaller live page set.
	sirContainerResult_t tryShrinkToFit( void )
	{
		uint32_t nRequiredPages = m_nSize / PAGE_ELEMENTS;
		if ( m_nSize % PAGE_ELEMENTS != 0 ) {
			++nRequiredPages;
		}
		const size_t nPageBytes = ( size_t )PAGE_ELEMENTS * sizeof( T );
		while ( m_pages.getSize() > nRequiredPages ) {
			uint32_t iLast = m_pages.getSize() - 1;
			T *pUnused = m_pages[iLast];
			m_pages.tryPopBack();
			SIRRawStorage::deallocate( m_pAllocator, pUnused, nPageBytes, alignof( T ) );
		}
		return m_pages.tryShrinkToFit();
	}

	void clear( void ) noexcept
	{
		while ( m_nSize != 0 ) {
			tryPopBack();
		}
	}

	void clearAndRelease( void ) noexcept
	{
		clear();
		const size_t nPageBytes = ( size_t )PAGE_ELEMENTS * sizeof( T );
		for ( uint32_t iPage = 0; iPage < m_pages.getSize(); ++iPage ) {
			SIRRawStorage::deallocate( m_pAllocator, m_pages[iPage], nPageBytes, alignof( T ) );
		}
		m_pages = SIRVector<T *>( m_pAllocator );
	}
};

#endif // __SIRENGINE_SIRPAGEDARRAY_H__
