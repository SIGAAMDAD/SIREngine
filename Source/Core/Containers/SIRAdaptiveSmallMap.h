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

#ifndef __SIR_ADAPTIVESMALLMAP_H__
#define __SIR_ADAPTIVESMALLMAP_H__

#include <Core/SIRPCH.h>
#include <iterator>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRStaticVector.h>
#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Containers/SIRRawStorage.h>
#include <functional>

template <typename K, typename V, uint32_t INLINE_COUNT = 8, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRAdaptiveSmallMap
{
	using Entry = std::pair<K, V>;

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

	using key_type = K;
	using mapped_type = V;

	bool contains( const K& rKey ) const
	{
		return find( rKey ) != nullptr;
	}

	size_type count( const K& rKey ) const
	{
		return contains( rKey ) ? 1 : 0;
	}

	using value_type = Entry;
	using difference_type = ptrdiff_t;

	class const_iterator
	{
	public:
		using value_type = Entry;
		using difference_type = ptrdiff_t;
		using pointer = const Entry *;
		using reference = const Entry&;
		using iterator_category = std::forward_iterator_tag;

		const_iterator( void ) noexcept = default;
		const_iterator( const SIRAdaptiveSmallMap *pOwner, bool bEnd ) noexcept
		    : m_pOwner( pOwner )
		{
			if ( pOwner == nullptr )
			{
				return;
			}
			if ( pOwner->m_pHash != nullptr ) {
				m_hashIterator = bEnd ? pOwner->m_pHash->cend() : pOwner->m_pHash->cbegin();
			} else {
				m_iInline = bEnd ? pOwner->m_inline.size() : 0;
			}
		}

		reference operator*( void ) const noexcept
		{
			assert( m_pOwner != nullptr );
			return m_pOwner->m_pHash == nullptr ? m_pOwner->m_inline[m_iInline] : *m_hashIterator;
		}
		pointer operator->( void ) const noexcept
		{
			return &operator*();
		}
		const_iterator& operator++( void ) noexcept
		{
			if ( m_pOwner->m_pHash != nullptr ) {
				++m_hashIterator;
			} else {
				++m_iInline;
			}
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
			if ( m_pOwner != rOther.m_pOwner )
			{
				return false;
			}
			if ( m_pOwner == nullptr )
			{
				return true;
			}
			return m_pOwner->m_pHash == nullptr ? m_iInline == rOther.m_iInline : m_hashIterator == rOther.m_hashIterator;
		}
		bool operator!=( const const_iterator& rOther ) const noexcept
		{
			return !( *this == rOther );
		}

	private:
		const SIRAdaptiveSmallMap *m_pOwner = nullptr;
		uint32_t m_iInline = 0;
		typename SIRFlatHashMap<K, V, Hash, Equal>::const_iterator m_hashIterator;
	};

	using iterator = const_iterator;
	iterator begin( void ) noexcept
	{
		return { this, false };
	}
	iterator end( void ) noexcept
	{
		return { this, true };
	}
	const_iterator begin( void ) const noexcept
	{
		return { this, false };
	}
	const_iterator end( void ) const noexcept
	{
		return { this, true };
	}
	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}
	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	uint32_t capacity( void ) const noexcept
	{
		return m_pHash == nullptr ? INLINE_COUNT : m_pHash->capacity();
	}

	void clear( void ) noexcept
	{
		if ( m_pHash == nullptr ) {
			m_inline.clear();
		} else {
			m_pHash->clear();
		}
	}

	explicit SIRAdaptiveSmallMap( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_pAllocator( pAllocator )
	{
	}

	~SIRAdaptiveSmallMap() noexcept
	{
		if ( m_pHash != nullptr ) {
			m_pHash->~SIRFlatHashMap<K, V, Hash, Equal>();
			SIRRawStorage::deallocate( m_pAllocator, m_pHash, sizeof( *m_pHash ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ) );
		}
	}

	SIRAdaptiveSmallMap( const SIRAdaptiveSmallMap& ) = delete;
	SIRAdaptiveSmallMap& operator=( const SIRAdaptiveSmallMap& ) = delete;

	const V *find( const K& key ) const
	{
		if ( m_pHash != nullptr ) {
			return m_pHash->find( key );
		}
		for ( uint32_t iIndex = 0; iIndex < m_inline.getSize(); ++iIndex ) {
			if ( m_equal( m_inline[iIndex].first, key ) ) {
				return &m_inline[iIndex].second;
			}
		}
		return nullptr;
	}

	SIRContainerResult tryInsert( const K& key, const V& value )
	{
		if ( find( key ) != nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_pHash != nullptr ) {
			return m_pHash->tryInsert( key, value );
		}
		if ( m_inline.getSize() < INLINE_COUNT ) {
			return m_inline.tryEmplaceBack( key, value );
		}
		void *pMemory = nullptr;
		SIRContainerResult eResult = SIRRawStorage::tryAllocate(
		    m_pAllocator, sizeof( SIRFlatHashMap<K, V, Hash, Equal> ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ), &pMemory );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		auto *pNew = new ( pMemory ) SIRFlatHashMap<K, V, Hash, Equal>( m_pAllocator, m_hash, m_equal );
		try {
			eResult = pNew->tryReserve( INLINE_COUNT + 1 );
			for ( uint32_t iIndex = 0; eResult == SIRContainerResult::SIR_CONTAINER_OK && iIndex < m_inline.getSize(); ++iIndex ) {
				eResult = pNew->tryInsert( m_inline[iIndex].first, m_inline[iIndex].second );
			}
			if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
				eResult = pNew->tryInsert( key, value );
			}
		} catch ( ... ) {
			pNew->~SIRFlatHashMap<K, V, Hash, Equal>();
			SIRRawStorage::deallocate( m_pAllocator, pNew, sizeof( *pNew ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ) );
			throw;
		}
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			pNew->~SIRFlatHashMap<K, V, Hash, Equal>();
			SIRRawStorage::deallocate( m_pAllocator, pNew, sizeof( *pNew ), alignof( SIRFlatHashMap<K, V, Hash, Equal> ) );
			return eResult;
		}
		m_inline.clear();
		m_pHash = pNew;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool erase( const K& key )
	{
		if ( m_pHash != nullptr ) {
			return m_pHash->erase( key );
		}
		static_assert( std::is_nothrow_move_assignable<Entry>::value, "Inline map needs nonthrowing erase shift" );
		for ( uint32_t iIndex = 0; iIndex < m_inline.getSize(); ++iIndex ) {
			if ( !m_equal( m_inline[iIndex].first, key ) ) {
				continue;
			}
			for ( uint32_t iMove = iIndex + 1; iMove < m_inline.getSize(); ++iMove ) {
				m_inline[iMove - 1] = std::move( m_inline[iMove] );
			}
			m_inline.tryPopBack();
			return true;
		}
		return false;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_pHash == nullptr ? m_inline.getSize() : m_pHash->getSize();
	}

	bool isHashed( void ) const noexcept
	{
		return m_pHash != nullptr;
	}

private:
	const sirContainerAllocator_t *m_pAllocator;
	SIRStaticVector<Entry, INLINE_COUNT> m_inline;
	SIRFlatHashMap<K, V, Hash, Equal> *m_pHash = nullptr;
	Hash m_hash;
	Equal m_equal;
};
#endif /* __SIR_ADAPTIVESMALLMAP_H__ */
