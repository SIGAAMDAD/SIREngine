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

#ifndef __SIR_FROZENHASHINDEX_H__
#define __SIR_FROZENHASHINDEX_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <functional>
#include <optional>
#include <utility>

template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRFrozenHashIndex
{
public:
	struct Entry
	{
		K key;
		V value;
	};

	explicit SIRFrozenHashIndex( const sirContainerAllocator_t *pAllocator, Hash hash = Hash(), Equal equal = Equal() )
	    : m_pAllocator( pAllocator ),
	      m_slots( pAllocator ),
	      m_hash( std::move( hash ) ),
	      m_equal( std::move( equal ) )
	{
	}

	SIRContainerResult tryBuild( const Entry *pEntries, uint32_t nCount )
	{
		if ( nCount != 0 && pEntries == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount > UINT32_MAX / 2 - 1 ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		uint32_t nCapacity = 8;
		while ( nCapacity < nCount * 2 ) {
			if ( nCapacity > UINT32_MAX / 2 ) {
				return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
			}
			nCapacity *= 2;
		}
		SIRVector<std::optional<Entry>> staged( m_pAllocator );
		SIRContainerResult eResult = staged.tryResize( nCapacity );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t iEntry = 0; iEntry < nCount; ++iEntry ) {
			uint32_t iSlot = static_cast<uint32_t>( m_hash( pEntries[iEntry].key ) & ( nCapacity - 1 ) );
			while ( staged[iSlot].has_value() ) {
				if ( m_equal( staged[iSlot]->key, pEntries[iEntry].key ) ) {
					return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
				}
				iSlot = ( iSlot + 1 ) & ( nCapacity - 1 );
			}
			staged[iSlot].emplace( pEntries[iEntry] );
		}
		m_slots = std::move( staged );
		m_nSize = nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	const V *find( const K& key ) const
	{
		if ( m_slots.isEmpty() ) {
			return nullptr;
		}
		const uint32_t nCapacity = m_slots.getSize();
		uint32_t iSlot = static_cast<uint32_t>( m_hash( key ) & ( nCapacity - 1 ) );
		for ( uint32_t nProbe = 0; nProbe < nCapacity; ++nProbe ) {
			if ( !m_slots[iSlot].has_value() ) {
				return nullptr;
			}
			if ( m_equal( m_slots[iSlot]->key, key ) ) {
				return &m_slots[iSlot]->value;
			}
			iSlot = ( iSlot + 1 ) & ( nCapacity - 1 );
		}
		return nullptr;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return m_slots.getFootprint();
	}

	publicprivate :

	    const sirContainerAllocator_t *m_pAllocator;
	SIRVector<std::optional<Entry>> m_slots;
	uint32_t m_nSize = 0;
	Hash m_hash;
	Equal m_equal;
};
#endif /* __SIR_FROZENHASHINDEX_H__ */
