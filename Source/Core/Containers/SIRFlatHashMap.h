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

#ifndef __SIRENGINE_SIRFLATHASHMAP_H__
#define __SIRENGINE_SIRFLATHASHMAP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRHashProbe.h>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>, bool bCompactControl = false>
class SIRFlatHashMap
{
private:
	using Pair = std::pair<K, V>;
	static_assert( SIRLifetimeOps<Pair>::CAN_RELOCATE, "Hash table growth requires copy or nothrow move" );
	static_assert( std::is_nothrow_invocable_r<size_t, Hash, const K &>::value,
		"Hash must be noexcept to preserve strong rehash failure guarantee" );

	uint8_t *m_pControl = nullptr;
	Pair *m_pEntries = nullptr;
	uint32_t m_nSize = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_nDeleted = 0;
	const sirContainerAllocator_t *m_pAllocator;
	Hash m_hash;
	Equal m_equal;

	static bool calculateLayout( uint32_t nCapacity, size_t *pOffset, size_t *pBytes ) noexcept
	{
		const size_t nAlignment = alignof( Pair );
		size_t nOffset = 0;
		size_t nEntriesBytes = 0;
		if ( !SIRRawStorage::checkedAdd( SIRHashProbe::controlBytes<bCompactControl>( nCapacity ), nAlignment - 1, &nOffset ) ) {
			return false;
		}
		nOffset &= ~( nAlignment - 1 );
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( Pair ), &nEntriesBytes ) ) {
			return false;
		}
		*pOffset = nOffset;
		return SIRRawStorage::checkedAdd( nOffset, nEntriesBytes, pBytes );
	}

	sirContainerResult_t allocateTable( uint32_t nCapacity, void **ppBlock, uint8_t **ppControl, Pair **ppEntries ) const noexcept
	{
		size_t nOffset = 0;
		size_t nBytes = 0;
		if ( !calculateLayout( nCapacity, &nOffset, &nBytes ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pBlock = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( Pair ), &pBlock );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		memset( pBlock, SIRHashProbe::EMPTY, SIRHashProbe::controlBytes<bCompactControl>( nCapacity ) );
		*ppBlock = pBlock;
		*ppControl = ( uint8_t * )pBlock;
		*ppEntries = ( Pair * )( ( uint8_t * )pBlock + nOffset );
		return SIR_CONTAINER_OK;
	}

	void freeTable( void *pBlock, uint32_t nCapacity ) const noexcept
	{
		if ( pBlock == nullptr ) {
			return;
		}
		size_t nOffset = 0;
		size_t nBytes = 0;
		bool bValid = calculateLayout( nCapacity, &nOffset, &nBytes );
		( void )bValid;
		SIRRawStorage::deallocate( m_pAllocator, pBlock, nBytes, alignof( Pair ) );
	}

	bool ownsAddress( const void *pAddress ) const noexcept
	{
		if ( m_pControl == nullptr || pAddress == nullptr ) {
			return false;
		}
		size_t nOffset = 0;
		size_t nBytes = 0;
		calculateLayout( m_nCapacity, &nOffset, &nBytes );
		uintptr_t nAddress = ( uintptr_t )pAddress;
		uintptr_t nStart = ( uintptr_t )m_pControl;
		return nAddress >= nStart && nAddress - nStart < nBytes;
	}

	uint32_t locate( const K &key ) const
	{
		if ( m_nCapacity == 0 ) {
			return UINT32_MAX;
		}
		uint32_t iBucket = SIRHashProbe::firstIndex( m_hash( key ), m_nCapacity );
		for ( uint32_t nProbes = 0; nProbes < m_nCapacity; ++nProbes ) {
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iBucket ) == SIRHashProbe::EMPTY ) {
				return UINT32_MAX;
			}
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iBucket ) == SIRHashProbe::OCCUPIED && m_equal( m_pEntries[iBucket].first, key ) ) {
				return iBucket;
			}
			iBucket = ( iBucket + 1 ) & ( m_nCapacity - 1 );
		}
		return UINT32_MAX;
	}

	sirContainerResult_t rehash( uint32_t nNewCapacity )
	{
		void *pNewBlock = nullptr;
		uint8_t *pNewControl = nullptr;
		Pair *pNewEntries = nullptr;
		sirContainerResult_t eResult = allocateTable( nNewCapacity, &pNewBlock, &pNewControl, &pNewEntries );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		try {
			for ( uint32_t iOld = 0; iOld < m_nCapacity; ++iOld ) {
				if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iOld ) != SIRHashProbe::OCCUPIED ) {
					continue;
				}
				uint32_t iNew = SIRHashProbe::firstIndex( m_hash( m_pEntries[iOld].first ), nNewCapacity );
				while ( SIRHashProbe::getControl<bCompactControl>( pNewControl, iNew ) == SIRHashProbe::OCCUPIED ) {
					iNew = ( iNew + 1 ) & ( nNewCapacity - 1 );
				}
				new ( pNewEntries + iNew ) Pair( std::move_if_noexcept( m_pEntries[iOld] ) );
				SIRHashProbe::setControl<bCompactControl>( pNewControl, iNew, SIRHashProbe::OCCUPIED );
			}
		} catch ( ... ) {
			for ( uint32_t iIndex = 0; iIndex < nNewCapacity; ++iIndex ) {
				if ( SIRHashProbe::getControl<bCompactControl>( pNewControl, iIndex ) == SIRHashProbe::OCCUPIED ) {
					pNewEntries[iIndex].~Pair();
				}
			}
			// The allocated block is not published until construction has succeeded.
			freeTable( pNewBlock, nNewCapacity );
			throw;
		}
		uint32_t nOldSize = m_nSize;
		clear();
		freeTable( m_pControl, m_nCapacity );
		m_pControl = pNewControl;
		m_pEntries = pNewEntries;
		m_nCapacity = nNewCapacity;
		m_nSize = nOldSize;
		return SIR_CONTAINER_OK;
	}

	template <typename KeyArg, typename ValueArg>
	sirContainerResult_t insertInternal( KeyArg &&key, ValueArg &&value, bool *pInserted )
	{
		if ( pInserted != nullptr ) {
			*pInserted = false;
		}
		if ( locate( key ) != UINT32_MAX ) {
			return SIR_CONTAINER_OK;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( m_nCapacity == 0 || ( ( uint64_t )m_nSize + m_nDeleted + 1 ) * 8 > ( uint64_t )m_nCapacity * 7 ) {
			uint32_t nNeeded = 0;
			sirContainerResult_t eResult = SIRHashProbe::capacityForCount( m_nSize + 1, &nNeeded );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			// Rebuild tombstones at the current capacity, unless the live load requires growth.
			uint32_t nCapacity = nNeeded > m_nCapacity ? nNeeded : m_nCapacity;
			if ( ownsAddress( &key ) || ownsAddress( &value ) ) {
				if constexpr ( std::is_copy_constructible<K>::value && std::is_copy_constructible<V>::value ) {
					Pair staged( key, value );
					eResult = rehash( nCapacity );
					if ( eResult != SIR_CONTAINER_OK ) {
						return eResult;
					}
					return insertInternal( std::move( staged.first ), std::move( staged.second ), pInserted );
				}
				return SIR_CONTAINER_INVALID_ARGUMENT;
			}
			eResult = rehash( nCapacity );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		uint32_t iBucket = SIRHashProbe::firstIndex( m_hash( key ), m_nCapacity );
		uint32_t iDeleted = UINT32_MAX;
		while ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iBucket ) != SIRHashProbe::EMPTY ) {
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iBucket ) == SIRHashProbe::DELETED && iDeleted == UINT32_MAX ) {
				iDeleted = iBucket;
			}
			iBucket = ( iBucket + 1 ) & ( m_nCapacity - 1 );
		}
		if ( iDeleted != UINT32_MAX ) {
			iBucket = iDeleted;
		}
		new ( m_pEntries + iBucket ) Pair( std::forward<KeyArg>( key ), std::forward<ValueArg>( value ) );
		if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iBucket ) == SIRHashProbe::DELETED ) {
			--m_nDeleted;
		}
		SIRHashProbe::setControl<bCompactControl>( m_pControl, iBucket, SIRHashProbe::OCCUPIED );
		++m_nSize;
		if ( pInserted != nullptr ) {
			*pInserted = true;
		}
		return SIR_CONTAINER_OK;
	}

public:
	explicit SIRFlatHashMap( const sirContainerAllocator_t *pAllocator, Hash hash = Hash(), Equal equal = Equal() )
		: m_pAllocator( pAllocator ), m_hash( std::move( hash ) ), m_equal( std::move( equal ) )
	{
	}

	~SIRFlatHashMap( void ) noexcept
	{
		clear();
		freeTable( m_pControl, m_nCapacity );
	}

	SIRFlatHashMap( const SIRFlatHashMap & ) = delete;
	SIRFlatHashMap &operator=( const SIRFlatHashMap & ) = delete;
	SIRFlatHashMap( SIRFlatHashMap && ) = delete;
	SIRFlatHashMap &operator=( SIRFlatHashMap && ) = delete;

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}
	uint32_t getDeletedCount( void ) const noexcept
	{
		return m_nDeleted;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		size_t nOffset = 0;
		size_t nBytes = 0;
		if ( m_nCapacity != 0 ) {
			calculateLayout( m_nCapacity, &nOffset, &nBytes );
		}
		return { 0, nBytes, 0, 0, ( uint64_t )m_nSize * sizeof( Pair ) };
	}

	sirContainerResult_t tryReserve( uint32_t nElements )
	{
		if ( nElements == 0 || ( nElements <= m_nSize && m_nCapacity != 0 ) ) {
			return SIR_CONTAINER_OK;
		}
		uint32_t nRequired = 0;
		sirContainerResult_t eResult = SIRHashProbe::capacityForCount( nElements, &nRequired );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		return nRequired <= m_nCapacity ? SIR_CONTAINER_OK : rehash( nRequired );
	}

	// Explicitly reclaim oversized tables after removals. A failed rehash
	// preserves the original table and all live values.
	sirContainerResult_t tryShrinkToFit( void )
	{
		if ( m_nSize == 0 ) {
			clear();
			freeTable( m_pControl, m_nCapacity );
			m_pControl = nullptr;
			m_pEntries = nullptr;
			m_nCapacity = 0;
			return SIR_CONTAINER_OK;
		}
		uint32_t nRequired = 0;
		sirContainerResult_t eResult = SIRHashProbe::capacityForCount( m_nSize, &nRequired );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		if ( nRequired == m_nCapacity && m_nDeleted == 0 ) {
			return SIR_CONTAINER_OK;
		}
		return rehash( nRequired );
	}

	sirContainerResult_t tryInsert( const K &key, const V &value, bool *pInserted = nullptr )
	{
		return insertInternal( key, value, pInserted );
	}

	sirContainerResult_t tryInsert( K &&key, V &&value, bool *pInserted = nullptr )
	{
		return insertInternal( std::move( key ), std::move( value ), pInserted );
	}

	template <typename KeyArg, typename ValueArg>
	sirContainerResult_t tryEmplace( KeyArg &&key, ValueArg &&value, bool *pInserted = nullptr )
	{
		return insertInternal( std::forward<KeyArg>( key ), std::forward<ValueArg>( value ), pInserted );
	}

	V *find( const K &key )
	{
		uint32_t iIndex = locate( key );
		return iIndex == UINT32_MAX ? nullptr : &m_pEntries[iIndex].second;
	}

	const V *find( const K &key ) const
	{
		uint32_t iIndex = locate( key );
		return iIndex == UINT32_MAX ? nullptr : &m_pEntries[iIndex].second;
	}

	bool erase( const K &key )
	{
		uint32_t iIndex = locate( key );
		if ( iIndex == UINT32_MAX ) {
			return false;
		}
		m_pEntries[iIndex].~Pair();
		SIRHashProbe::setControl<bCompactControl>( m_pControl, iIndex, SIRHashProbe::DELETED );
		--m_nSize;
		++m_nDeleted;
		return true;
	}

	void clear( void ) noexcept
	{
		for ( uint32_t iIndex = 0; iIndex < m_nCapacity; ++iIndex ) {
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) == SIRHashProbe::OCCUPIED ) {
				m_pEntries[iIndex].~Pair();
			}
		}
		if ( m_pControl != nullptr ) {
			memset( m_pControl, SIRHashProbe::EMPTY, SIRHashProbe::controlBytes<bCompactControl>( m_nCapacity ) );
		}
		m_nSize = 0;
		m_nDeleted = 0;
	}
};

#endif // __SIRENGINE_SIRFLATHASHMAP_H__
