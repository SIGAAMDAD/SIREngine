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

#ifndef __SIR_FLATHASHSET_H__
#define __SIR_FLATHASHSET_H__

#include <Core/SIRPCH.h>
#include <iterator>

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

template <typename K, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>, bool bCompactControl = false>
class SIRFlatHashSet
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

	using key_type = K;
	using value_type = K;
	using difference_type = ptrdiff_t;

	size_type count( const K& rKey ) const
	{
		return contains( rKey ) ? 1 : 0;
	}


	// Set keys cannot be mutated through an iterator.
	class const_iterator
	{
	public:
		using value_type = K;
		using difference_type = ptrdiff_t;
		using pointer = const K *;
		using reference = const K&;
		using iterator_category = std::forward_iterator_tag;

		const_iterator( void ) noexcept = default;
		const_iterator( const SIRFlatHashSet *pOwner, uint32_t iSlot ) noexcept
		    : m_pOwner( pOwner ), m_iSlot( iSlot )
		{
			skipVacant();
		}

		reference operator*( void ) const noexcept
		{
			assert( m_pOwner != nullptr && m_iSlot < m_pOwner->m_nCapacity );
			return m_pOwner->m_pKeys[m_iSlot];
		}
		pointer operator->( void ) const noexcept
		{
			return &operator*();
		}
		const_iterator& operator++( void ) noexcept
		{
			++m_iSlot;
			skipVacant();
			return *this;
		}
		const_iterator operator++( int ) noexcept
		{
			const_iterator old( *this );
			++*this;
			return old;
		}
		bool operator==( const const_iterator& rOther ) const noexcept
		{
			return m_pOwner == rOther.m_pOwner && m_iSlot == rOther.m_iSlot;
		}
		bool operator!=( const const_iterator& rOther ) const noexcept
		{
			return !( *this == rOther );
		}

	private:
		const SIRFlatHashSet *m_pOwner = nullptr;
		uint32_t m_iSlot = 0;
		void skipVacant( void ) noexcept
		{
			if ( m_pOwner == nullptr )
			{
				return;
			}
			while ( m_iSlot < m_pOwner->m_nCapacity ) {
				if ( SIRHashProbe::getControl<bCompactControl>( m_pOwner->m_pControl, m_iSlot ) == SIRHashProbe::OCCUPIED )
				{
					break;
				}
				++m_iSlot;
			}
		}
	};

	using iterator = const_iterator;
	iterator begin( void ) noexcept
	{
		return { this, 0 };
	}
	iterator end( void ) noexcept
	{
		return { this, m_nCapacity };
	}
	const_iterator begin( void ) const noexcept
	{
		return { this, 0 };
	}
	const_iterator end( void ) const noexcept
	{
		return { this, m_nCapacity };
	}
	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}
	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	float loadFactor( void ) const noexcept
	{
		return m_nCapacity == 0 ? 0.0f : static_cast<float>( m_nSize ) / m_nCapacity;
	}

	explicit SIRFlatHashSet( const sirContainerAllocator_t *pAllocator, Hash hash = Hash(), Equal equal = Equal() )
	    : m_pAllocator( pAllocator ),
	      m_hash( std::move( hash ) ),
	      m_equal( std::move( equal ) )
	{
	}

	~SIRFlatHashSet() noexcept
	{
		clear();
		release( m_pControl, m_nCapacity );
	}

	SIRFlatHashSet( const SIRFlatHashSet& ) = delete;
	SIRFlatHashSet& operator=( const SIRFlatHashSet& ) = delete;
	SIRFlatHashSet( SIRFlatHashSet&& ) = delete;
	SIRFlatHashSet& operator=( SIRFlatHashSet&& ) = delete;

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return m_nCapacity;
	}

	bool contains( const K& key ) const
	{
		return locate( key ) != UINT32_MAX;
	}

	SIRContainerResult tryReserve( uint32_t nElements )
	{
		if ( nElements == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		uint32_t nRequired = 0;
		SIRContainerResult eResult = SIRHashProbe::capacityForCount( nElements > m_nSize ? nElements : m_nSize, &nRequired );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		return nRequired <= m_nCapacity ? SIRContainerResult::SIR_CONTAINER_OK : rehash( nRequired );
	}

	// Rebuild at the smallest load-safe capacity, or release an empty table.
	SIRContainerResult tryShrinkToFit( void )
	{
		if ( m_nSize == 0 ) {
			clear();
			release( m_pControl, m_nCapacity );
			m_pControl = nullptr;
			m_pKeys = nullptr;
			m_nCapacity = 0;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		uint32_t nRequired = 0;
		SIRContainerResult eResult = SIRHashProbe::capacityForCount( m_nSize, &nRequired );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		if ( nRequired == m_nCapacity && m_nDeleted == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		return rehash( nRequired );
	}

	SIRContainerResult tryInsert( const K& key, bool *pInserted = nullptr )
	{
		return insertInternal( key, pInserted );
	}

	SIRContainerResult tryInsert( K&& key, bool *pInserted = nullptr )
	{
		return insertInternal( std::move( key ), pInserted );
	}

	template <typename KeyArg>
	SIRContainerResult tryEmplace( KeyArg&& key, bool *pInserted = nullptr )
	{
		return insertInternal( std::forward<KeyArg>( key ), pInserted );
	}

	bool erase( const K& key )
	{
		uint32_t iIndex = locate( key );
		if ( iIndex == UINT32_MAX ) {
			return false;
		}
		m_pKeys[iIndex].~K();
		SIRHashProbe::setControl<bCompactControl>( m_pControl, iIndex, SIRHashProbe::DELETED );
		--m_nSize;
		++m_nDeleted;
		return true;
	}

	void clear( void ) noexcept
	{
		for ( uint32_t iIndex = 0; iIndex < m_nCapacity; ++iIndex ) {
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) == SIRHashProbe::OCCUPIED ) {
				m_pKeys[iIndex].~K();
			}
		}
		if ( m_pControl != nullptr ) {
			memset( m_pControl, SIRHashProbe::EMPTY, SIRHashProbe::controlBytes<bCompactControl>( m_nCapacity ) );
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
		return { 0, nBytes, 0, 0, static_cast<uint64_t>( m_nSize * sizeof( K ) ) };
	}

private:
	static_assert( SIRLifetimeOps<K>::CAN_RELOCATE, "Hash set growth requires copy or nothrow move" );
	static_assert( std::is_nothrow_invocable_r<size_t, Hash, const K&>::value, "Hash must be noexcept" );
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
		if ( !SIRRawStorage::checkedAdd( SIRHashProbe::controlBytes<bCompactControl>( nCapacity ), alignof( K ) - 1, &nOffset ) ) {
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

	bool ownsAddress( const void *pAddress ) const noexcept
	{
		if ( m_pControl == nullptr || pAddress == nullptr ) {
			return false;
		}
		size_t nOffset = 0;
		size_t nBytes = 0;
		layout( m_nCapacity, &nOffset, &nBytes );
		uintptr_t nAddress = reinterpret_cast<uintptr_t>( pAddress );
		uintptr_t nStart = reinterpret_cast<uintptr_t>( m_pControl );
		return nAddress >= nStart && nAddress - nStart < nBytes;
	}

	uint32_t locate( const K& key ) const
	{
		if ( m_nCapacity == 0 ) {
			return UINT32_MAX;
		}
		uint32_t iIndex = SIRHashProbe::firstIndex( m_hash( key ), m_nCapacity );
		for ( uint32_t nProbe = 0; nProbe < m_nCapacity; ++nProbe ) {
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) == SIRHashProbe::EMPTY ) {
				return UINT32_MAX;
			}
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) == SIRHashProbe::OCCUPIED &&
			     m_equal( m_pKeys[iIndex], key ) ) {
				return iIndex;
			}
			iIndex = ( iIndex + 1 ) & ( m_nCapacity - 1 );
		}
		return UINT32_MAX;
	}

	SIRContainerResult rehash( uint32_t nCapacity )
	{
		size_t nOffset = 0;
		size_t nBytes = 0;
		if ( !layout( nCapacity, &nOffset, &nBytes ) ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		void *pBlock = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( K ), &pBlock );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint8_t *pControl = static_cast<uint8_t *>( pBlock );
		K *pKeys = reinterpret_cast<K *>( pControl + nOffset );
		memset( pControl, SIRHashProbe::EMPTY, SIRHashProbe::controlBytes<bCompactControl>( nCapacity ) );
		try {
			for ( uint32_t iOld = 0; iOld < m_nCapacity; ++iOld ) {
				if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iOld ) != SIRHashProbe::OCCUPIED ) {
					continue;
				}
				uint32_t iNew = SIRHashProbe::firstIndex( m_hash( m_pKeys[iOld] ), nCapacity );
				while ( SIRHashProbe::getControl<bCompactControl>( pControl, iNew ) == SIRHashProbe::OCCUPIED ) {
					iNew = ( iNew + 1 ) & ( nCapacity - 1 );
				}
				new ( pKeys + iNew ) K( std::move_if_noexcept( m_pKeys[iOld] ) );
				SIRHashProbe::setControl<bCompactControl>( pControl, iNew, SIRHashProbe::OCCUPIED );
			}
		} catch ( ... ) {
			for ( uint32_t iIndex = 0; iIndex < nCapacity; ++iIndex ) {
				if ( SIRHashProbe::getControl<bCompactControl>( pControl, iIndex ) == SIRHashProbe::OCCUPIED ) {
					pKeys[iIndex].~K();
				}
			}
			release( pBlock, nCapacity );
			throw;
		}
		uint32_t nOldSize = m_nSize;
		clear();
		release( m_pControl, m_nCapacity );
		m_pControl = pControl;
		m_pKeys = pKeys;
		m_nSize = nOldSize;
		m_nCapacity = nCapacity;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	template <typename KeyArg>
	SIRContainerResult insertInternal( KeyArg&& key, bool *pInserted )
	{
		if ( pInserted != nullptr ) {
			*pInserted = false;
		}
		if ( contains( key ) ) {
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( m_nCapacity == 0 || ( static_cast<uint64_t>( m_nSize ) + m_nDeleted + 1 ) * 8 > static_cast<uint64_t>( m_nCapacity ) * 7 ) {
			uint32_t nNeeded = 0;
			SIRContainerResult eResult = SIRHashProbe::capacityForCount( m_nSize + 1, &nNeeded );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			uint32_t nCapacity = nNeeded > m_nCapacity ? nNeeded : m_nCapacity;
			if ( ownsAddress( &key ) ) {
				if constexpr ( std::is_copy_constructible<K>::value ) {
					K staged( key );
					eResult = rehash( nCapacity );
					if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
						return eResult;
					}
					return insertInternal( std::move( staged ), pInserted );
				}
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
			eResult = rehash( nCapacity );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		uint32_t iIndex = SIRHashProbe::firstIndex( m_hash( key ), m_nCapacity );
		uint32_t iDeleted = UINT32_MAX;
		while ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) != SIRHashProbe::EMPTY ) {
			if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) == SIRHashProbe::DELETED && iDeleted == UINT32_MAX ) {
				iDeleted = iIndex;
			}
			iIndex = ( iIndex + 1 ) & ( m_nCapacity - 1 );
		}
		if ( iDeleted != UINT32_MAX ) {
			iIndex = iDeleted;
		}
		new ( m_pKeys + iIndex ) K( std::forward<KeyArg>( key ) );
		if ( SIRHashProbe::getControl<bCompactControl>( m_pControl, iIndex ) == SIRHashProbe::DELETED ) {
			--m_nDeleted;
		}
		SIRHashProbe::setControl<bCompactControl>( m_pControl, iIndex, SIRHashProbe::OCCUPIED );
		++m_nSize;
		if ( pInserted != nullptr ) {
			*pInserted = true;
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
};

#endif // __SIR_FLATHASHSET_H__
