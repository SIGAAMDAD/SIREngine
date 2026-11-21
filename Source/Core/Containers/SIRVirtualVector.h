/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVIRTUALVECTOR_H__
#define __SIRENGINE_SIRVIRTUALVECTOR_H__

#include <Core/SIRPCH.h>

#include <Core/Containers/SIRRawStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <Core/Platform/SIRVirtualMemory.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <new>
#include <stdlib.h>
#include <stdint.h>
#include <utility>

// Fixed maximum address reservation. Not thread-safe. The caller is responsible
// for selecting an OS reservation policy permitted by its memory budget.
template <typename T>
class SIRVirtualVector
{
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

public:
	SIRVirtualVector( void ) noexcept = default;
	~SIRVirtualVector( void ) noexcept
	{
		release();
	}
	SIRVirtualVector( const SIRVirtualVector& ) = delete;
	SIRVirtualVector& operator=( const SIRVirtualVector& ) = delete;

	sirContainerResult_t tryInitialize( uint32_t nMaximum ) noexcept
	{
		if ( m_pData != nullptr || nMaximum == 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const size_t nPage = SIRVirtualMemory::getPageSize();
		if ( nPage == 0 || alignof( T ) > nPage ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nBytes = 0;
		size_t nRounded = 0;
		if ( !SIRRawStorage::checkedMultiply( nMaximum, sizeof( T ), &nBytes ) || !roundUp( nBytes, nPage, &nRounded ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pMemory = SIRVirtualMemory::reserve( nRounded );
		if ( pMemory == nullptr ) {
			return SIR_CONTAINER_OUT_OF_MEMORY;
		}
		m_pData = ( T * )pMemory;
		m_nMaximum = nMaximum;
		m_nPageBytes = nPage;
		m_nReservationBytes = nRounded;
		return SIR_CONTAINER_OK;
	}

	// The segment is exclusively owned for this vector's lifetime. GRDG and
	// other allocators must not commit into the same range.
	sirContainerResult_t tryInitialize( SIRMemorySegment& rSegment, uint32_t nMaximum ) noexcept
	{
		if ( m_pData != nullptr || nMaximum == 0 || rSegment.getBase() == nullptr || rSegment.getCommittedBytes() != 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const size_t nPage = ( size_t )rSegment.getPageSize();
		if ( nPage == 0 || alignof( T ) > nPage ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		size_t nBytes = 0;
		size_t nRounded = 0;
		if ( !SIRRawStorage::checkedMultiply( nMaximum, sizeof( T ), &nBytes ) || !roundUp( nBytes, nPage, &nRounded ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( nRounded > rSegment.getReservedBytes() || nRounded > rSegment.getCommitLimitBytes() ) {
			return SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		m_pData = ( T * )rSegment.getBase();
		m_pSegment = &rSegment;
		m_nMaximum = nMaximum;
		m_nPageBytes = nPage;
		m_nReservationBytes = nRounded;
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryReserve( uint32_t nCapacity ) noexcept
	{
		if ( m_pData == nullptr || nCapacity > m_nMaximum ) {
			return SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		size_t nBytes = 0;
		size_t nRounded = 0;
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( T ), &nBytes ) || !roundUp( nBytes, m_nPageBytes, &nRounded ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( nRounded <= m_nCommittedBytes ) {
			return SIR_CONTAINER_OK;
		}
		if ( m_pSegment != nullptr ? !m_pSegment->commit( m_nCommittedBytes, nRounded - m_nCommittedBytes )
		                           : !SIRVirtualMemory::commit( ( uint8_t * )m_pData + m_nCommittedBytes, nRounded - m_nCommittedBytes ) ) {
			return SIR_CONTAINER_OUT_OF_MEMORY;
		}
		m_nCommittedBytes = nRounded;
		return SIR_CONTAINER_OK;
	}
	template <typename... Args>
	sirContainerResult_t tryEmplaceBack( Args&&...args )
	{
		if ( m_nSize == m_nMaximum ) {
			return SIR_CONTAINER_FULL;
		}
		const sirContainerResult_t eResult = tryReserve( m_nSize + 1 );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		new ( m_pData + m_nSize ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIR_CONTAINER_OK;
	}
	sirContainerResult_t tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIR_CONTAINER_EMPTY;
		}
		m_pData[--m_nSize].~T();
		return SIR_CONTAINER_OK;
	}
	bool tryDecommitTail( void ) noexcept
	{
		if ( m_pData == nullptr ) {
			return false;
		}
		size_t nKeep = 0;
		if ( !roundUp( ( size_t )m_nSize * sizeof( T ), m_nPageBytes, &nKeep ) ) {
			return false;
		}
		if ( nKeep >= m_nCommittedBytes ) {
			return true;
		}
		if ( m_pSegment != nullptr ? !m_pSegment->decommit( nKeep, m_nCommittedBytes - nKeep )
		                           : !SIRVirtualMemory::decommit( ( uint8_t * )m_pData + nKeep, m_nCommittedBytes - nKeep ) ) {
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
		return ( uint32_t )( m_nCommittedBytes / sizeof( T ) );
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
};

#endif // __SIRENGINE_SIRVIRTUALVECTOR_H__
