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

#ifndef __SIRENGINE_SIRFLATHASHSET_H__
#define __SIRENGINE_SIRFLATHASHSET_H__

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

template <typename K, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRFlatHashSet
{
private:
	static_assert( SIRLifetimeOps<K>::CAN_RELOCATE, "Hash set growth requires copy or nothrow move" );
	static_assert( std::is_nothrow_invocable_r<size_t, Hash, const K &>::value, "Hash must be noexcept" );
	void *m_pBlock = nullptr;
	uint8_t *m_pControl = nullptr;
	K *m_pKeys = nullptr;
	uint32_t m_nSize = 0;
	uint32_t m_nCapacity = 0;
	uint32_t m_nDeleted = 0;
	const sirContainerAllocator_t *m_pAllocator;
	Hash m_hash;
	Equal m_equal;

	static bool layout( uint32_t nCapacity, size_t *pOffset, size_t *pBytes ) noexcept
	{
		size_t nOffset = 0;
		size_t nEntries = 0;
		if ( !SIRRawStorage::checkedAdd( nCapacity, alignof( K ) - 1, &nOffset ) ) {
			return false;
		}
		nOffset &= ~( alignof( K ) - 1 );
		if ( !SIRRawStorage::checkedMultiply( nCapacity, sizeof( K ), &nEntries ) ) {
			return false;
		}
		*pOffset = nOffset;
		return SIRRawStorage::checkedAdd( nOffset, nEntries, pBytes );
	}

	void release( void *pBlock, uint32_t nCapacity ) const noexcept
	{
		if ( pBlock != nullptr ) {
			size_t nOffset = 0;
			size_t nBytes = 0;
			layout( nCapacity, &nOffset, &nBytes );
			SIRRawStorage::deallocate( m_pAllocator, pBlock, nBytes, alignof( K ) );
		}
	}

	uint32_t locate( const K &key ) const
	{
		if ( m_nCapacity == 0 ) {
			return UINT32_MAX;
		}
		uint32_t iIndex = SIRHashProbe::firstIndex( m_hash( key ), m_nCapacity );
		for ( uint32_t nProbe = 0; nProbe < m_nCapacity; ++nProbe ) {
			if ( m_pControl[iIndex] == SIRHashProbe::EMPTY ) {
				return UINT32_MAX;
			}
			if ( m_pControl[iIndex] == SIRHashProbe::OCCUPIED && m_equal( m_pKeys[iIndex], key ) ) {
				return iIndex;
			}
			iIndex = ( iIndex + 1 ) & ( m_nCapacity - 1 );
		}
		return UINT32_MAX;
	}

	sirContainerResult_t rehash( uint32_t nCapacity )
	{
		size_t nOffset = 0;
		size_t nBytes = 0;
		if ( !layout( nCapacity, &nOffset, &nBytes ) ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pBlock = nullptr;
		sirContainerResult_t eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( K ), &pBlock );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint8_t *pControl = ( uint8_t * )pBlock;
		K *pKeys = ( K * )( pControl + nOffset );
		memset( pControl, SIRHashProbe::EMPTY, nCapacity );
		try {
			for ( uint32_t iOld = 0; iOld < m_nCapacity; ++iOld ) {
				if ( m_pControl[iOld] != SIRHashProbe::OCCUPIED ) {
					continue;
				}
				uint32_t iNew = SIRHashProbe::firstIndex( m_hash( m_pKeys[iOld] ), nCapacity );
				while ( pControl[iNew] == SIRHashProbe::OCCUPIED ) {
					iNew = ( iNew + 1 ) & ( nCapacity - 1 );
				}
				new ( pKeys + iNew ) K( std::move_if_noexcept( m_pKeys[iOld] ) );
				pControl[iNew] = SIRHashProbe::OCCUPIED;
			}
		} catch ( ... ) {
			for ( uint32_t iIndex = 0; iIndex < nCapacity; ++iIndex ) {
				if ( pControl[iIndex] == SIRHashProbe::OCCUPIED ) {
					pKeys[iIndex].~K();
				}
			}
			release( pBlock, nCapacity );
			throw;
		}
		uint32_t nOldSize = m_nSize;
		clear();
		release( m_pBlock, m_nCapacity );
		m_pBlock = pBlock;
		m_pControl = pControl;
		m_pKeys = pKeys;
		m_nSize = nOldSize;
		m_nCapacity = nCapacity;
		return SIR_CONTAINER_OK;
	}

	template <typename KeyArg>
	sirContainerResult_t insertInternal( KeyArg &&key, bool *pInserted )
	{
		if ( pInserted != nullptr ) {
			*pInserted = false;
		}
		if ( contains( key ) ) {
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
			eResult = rehash( nNeeded > m_nCapacity ? nNeeded : m_nCapacity );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		uint32_t iIndex = SIRHashProbe::firstIndex( m_hash( key ), m_nCapacity );
		uint32_t iDeleted = UINT32_MAX;
		while ( m_pControl[iIndex] != SIRHashProbe::EMPTY ) {
			if ( m_pControl[iIndex] == SIRHashProbe::DELETED && iDeleted == UINT32_MAX ) {
				iDeleted = iIndex;
			}
			iIndex = ( iIndex + 1 ) & ( m_nCapacity - 1 );
		}
		if ( iDeleted != UINT32_MAX ) {
			iIndex = iDeleted;
		}
		new ( m_pKeys + iIndex ) K( std::forward<KeyArg>( key ) );
		if ( m_pControl[iIndex] == SIRHashProbe::DELETED ) {
			--m_nDeleted;
		}
		m_pControl[iIndex] = SIRHashProbe::OCCUPIED;
		++m_nSize;
		if ( pInserted != nullptr ) {
			*pInserted = true;
		}
		return SIR_CONTAINER_OK;
	}

public:
	explicit SIRFlatHashSet( const sirContainerAllocator_t *pAllocator, Hash hash = Hash(), Equal equal = Equal() )
		: m_pAllocator( pAllocator ), m_hash( std::move( hash ) ), m_equal( std::move( equal ) )
	{
	}

	~SIRFlatHashSet( void ) noexcept
	{
		clear();
		release( m_pBlock, m_nCapacity );
	}
	SIRFlatHashSet( const SIRFlatHashSet & ) = delete;
	SIRFlatHashSet &operator=( const SIRFlatHashSet & ) = delete;
	SIRFlatHashSet( SIRFlatHashSet && ) = delete;
	SIRFlatHashSet &operator=( SIRFlatHashSet && ) = delete;

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}
	bool contains( const K &key ) const
	{
		return locate( key ) != UINT32_MAX;
	}

	sirContainerResult_t tryReserve( uint32_t nElements )
	{
		if ( nElements == 0 ) {
			return SIR_CONTAINER_OK;
		}
		uint32_t nRequired = 0;
		sirContainerResult_t eResult = SIRHashProbe::capacityForCount( nElements > m_nSize ? nElements : m_nSize, &nRequired );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		return nRequired <= m_nCapacity ? SIR_CONTAINER_OK : rehash( nRequired );
	}

	sirContainerResult_t tryInsert( const K &key, bool *pInserted = nullptr )
	{
		return insertInternal( key, pInserted );
	}
	sirContainerResult_t tryInsert( K &&key, bool *pInserted = nullptr )
	{
		return insertInternal( std::move( key ), pInserted );
	}

	template <typename KeyArg>
	sirContainerResult_t tryEmplace( KeyArg &&key, bool *pInserted = nullptr )
	{
		return insertInternal( std::forward<KeyArg>( key ), pInserted );
	}

	bool erase( const K &key )
	{
		uint32_t iIndex = locate( key );
		if ( iIndex == UINT32_MAX ) {
			return false;
		}
		m_pKeys[iIndex].~K();
		m_pControl[iIndex] = SIRHashProbe::DELETED;
		--m_nSize;
		++m_nDeleted;
		return true;
	}

	void clear( void ) noexcept
	{
		for ( uint32_t iIndex = 0; iIndex < m_nCapacity; ++iIndex ) {
			if ( m_pControl[iIndex] == SIRHashProbe::OCCUPIED ) {
				m_pKeys[iIndex].~K();
			}
		}
		if ( m_pControl != nullptr ) {
			memset( m_pControl, SIRHashProbe::EMPTY, m_nCapacity );
		}
		m_nSize = 0;
		m_nDeleted = 0;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		size_t nOffset = 0;
		size_t nBytes = 0;
		if ( m_nCapacity != 0 ) {
			layout( m_nCapacity, &nOffset, &nBytes );
		}
		return { 0, nBytes, 0, 0, ( uint64_t )m_nSize * sizeof( K ) };
	}
};

#endif // __SIRENGINE_SIRFLATHASHSET_H__
