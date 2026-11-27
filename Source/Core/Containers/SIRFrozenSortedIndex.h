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

#ifndef __SIR_FROZENSORTEDINDEX_H__
#define __SIR_FROZENSORTEDINDEX_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRFrozenOptionalIterator.h>
#include <optional>
#include <functional>
#include <utility>

template <typename K, typename V, typename Less = std::less<K>>
class SIRFrozenSortedIndex
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

	struct Entry
	{
		K key;
		V value;
	};

	using value_type = Entry;
	using const_reference = const Entry&;
	using const_iterator = SIRFrozenOptionalIterator<Entry>;
	using iterator = const_iterator;

	iterator begin( void ) const noexcept
	{
		return iterator( m_nodes.data(), m_nodes.size(), 0 );
	}

	iterator end( void ) const noexcept
	{
		return iterator( m_nodes.data(), m_nodes.size(), m_nodes.size() );
	}

	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	const_iterator cend( void ) const noexcept
	{
		return end();
	}


	explicit SIRFrozenSortedIndex( const sirContainerAllocator_t *pAllocator, Less less = Less() )
	    : m_nodes( pAllocator ),
	      m_less( std::move( less ) ),
	      m_pAllocatorStore( pAllocator )
	{
	}

	SIRContainerResult tryBuildSorted( const Entry *pSorted, uint32_t nCount )
	{
		if ( nCount != 0 && pSorted == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		for ( uint32_t iIndex = 1; iIndex < nCount; ++iIndex ) {
			if ( !m_less( pSorted[iIndex - 1].key, pSorted[iIndex].key ) ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
		}
		SIRVector<std::optional<Entry>> staged( m_pAllocator() );
		const SIRContainerResult eResult = staged.tryResize( nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		uint32_t iInput = 0;
		layout( staged, pSorted, nCount, 1, iInput );
		m_nodes = std::move( staged );
		m_nSize = nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	const V *find( const K& key ) const
	{
		uint64_t iHeap = 1;
		while ( iHeap <= m_nSize ) {
			const Entry& entry = *m_nodes[static_cast<uint32_t>( iHeap ) - 1];
			if ( m_less( key, entry.key ) ) {
				iHeap *= 2;
			} else if ( m_less( entry.key, key ) ) {
				iHeap = iHeap * 2 + 1;
			} else {
				return &entry.value;
			}
		}
		return nullptr;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return m_nodes.getFootprint();
	}

private:
	SIRVector<std::optional<Entry>> m_nodes;
	uint32_t m_nSize = 0;
	Less m_less;

	static void layout( SIRVector<std::optional<Entry>>& nodes, const Entry *pSorted, uint32_t nCount, uint64_t iHeap, uint32_t& iInput )
	{
		if ( iHeap > nCount ) {
			return;
		}
		layout( nodes, pSorted, nCount, iHeap * 2, iInput );
		nodes[iHeap - 1].emplace( pSorted[iInput] );
		++iInput;
		layout( nodes, pSorted, nCount, iHeap * 2 + 1, iInput );
	}

	const sirContainerAllocator_t *m_pAllocator( void ) const noexcept
	{
		return m_pAllocatorStore;
	}

	const sirContainerAllocator_t *m_pAllocatorStore = nullptr;
};
#endif /* __SIR_FROZENSORTEDINDEX_H__ */
