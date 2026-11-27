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

#ifndef __SIR_PACKEDMEMORYMAP_H__
#define __SIR_PACKEDMEMORYMAP_H__

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
class SIRPackedMemoryMap
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
		return iterator( m_slots.data(), m_slots.size(), 0 );
	}

	iterator end( void ) const noexcept
	{
		return iterator( m_slots.data(), m_slots.size(), m_slots.size() );
	}

	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	const_iterator cend( void ) const noexcept
	{
		return end();
	}


	explicit SIRPackedMemoryMap( const sirContainerAllocator_t *pAllocator, Less less = Less() )
	    : m_pAllocator( pAllocator ),
	      m_slots( pAllocator ),
	      m_less( std::move( less ) )
	{
	}

	SIRContainerResult tryInsert( const K& key, const V& value )
	{
		if ( m_nCount == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		if ( find( key ) != nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		// Relocation below requires a nonthrowing move to keep the existing slots intact.
		static_assert( std::is_nothrow_move_constructible<Entry>::value && std::is_nothrow_move_assignable<Entry>::value,
		               "Gap movements require noexcept relocation" );
		Entry staged{ key, value };
		if ( m_nCount == m_slots.getSize() ) {
			SIRContainerResult eResult = grow();
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		const uint32_t nCapacity = m_slots.getSize();
		const uint32_t iInsert = lowerBoundPhysical( staged.key );
		if ( iInsert == nCapacity ) {
			uint32_t iLast = nCapacity - 1;
			while ( m_slots[iLast].has_value() ) {
				--iLast;
			}
			for ( uint32_t iIndex = iLast; iIndex + 1 < nCapacity; ++iIndex ) {
				m_slots[iIndex] = std::move( m_slots[iIndex + 1] );
				m_slots[iIndex + 1].reset();
			}
			m_slots[nCapacity - 1].emplace( std::move( staged ) );
			++m_nCount;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( !m_slots[iInsert].has_value() ) {
			m_slots[iInsert].emplace( std::move( staged ) );
		} else {
			uint32_t iLeft = iInsert;
			while ( iLeft != 0 && m_slots[iLeft - 1].has_value() ) {
				--iLeft;
			}
			uint32_t iRight = iInsert;
			while ( iRight < nCapacity && m_slots[iRight].has_value() ) {
				++iRight;
			}
			const bool bUseLeft = iLeft != 0 && ( iRight == nCapacity || iInsert - iLeft <= iRight - iInsert );
			if ( bUseLeft ) {
				for ( uint32_t iIndex = iLeft; iIndex < iInsert; ++iIndex ) {
					m_slots[iIndex - 1] = std::move( m_slots[iIndex] );
					m_slots[iIndex].reset();
				}
				m_slots[iInsert - 1].emplace( std::move( staged ) );
			} else {
				for ( uint32_t iIndex = iRight; iIndex > iInsert; --iIndex ) {
					m_slots[iIndex] = std::move( m_slots[iIndex - 1] );
					m_slots[iIndex - 1].reset();
				}
				m_slots[iInsert].emplace( std::move( staged ) );
			}
		}
		++m_nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	const V *find( const K& key ) const
	{
		uint32_t iIndex = lowerBoundPhysical( key );
		while ( iIndex < m_slots.getSize() && !m_slots[iIndex].has_value() ) {
			++iIndex;
		}
		if ( iIndex == m_slots.getSize() ) {
			return nullptr;
		}
		if ( !equivalent( m_slots[iIndex]->key, key ) ) {
			return nullptr;
		}
		return &m_slots[iIndex]->value;
	}

	bool erase( const K& key )
	{
		uint32_t iIndex = lowerBoundPhysical( key );
		while ( iIndex < m_slots.getSize() && !m_slots[iIndex].has_value() ) {
			++iIndex;
		}
		if ( iIndex == m_slots.getSize() ) {
			return false;
		}
		if ( !equivalent( m_slots[iIndex]->key, key ) ) {
			return false;
		}
		m_slots[iIndex].reset();
		--m_nCount;
		return true;
	}

	template <typename Visitor>
	void forEach( Visitor&& visitor ) const
	{
		for ( uint32_t iIndex = 0; iIndex < m_slots.getSize(); ++iIndex ) {
			if ( m_slots[iIndex].has_value() ) {
				visitor( m_slots[iIndex]->key, m_slots[iIndex]->value );
			}
		}
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nCount;
	}

private:
	const sirContainerAllocator_t *m_pAllocator;
	SIRVector<std::optional<Entry>> m_slots;
	uint32_t m_nCount = 0;
	Less m_less;

	bool equivalent( const K& left, const K& right ) const
	{
		return !m_less( left, right ) && !m_less( right, left );
	}

	uint32_t lowerBoundPhysical( const K& key ) const
	{
		uint32_t nLo = 0;
		uint32_t nHi = m_slots.getSize();
		while ( nLo < nHi ) {
			const uint32_t iMid = nLo + ( nHi - nLo ) / 2;
			int64_t iLeft = iMid;
			uint64_t iRight = iMid + 1ull;
			uint32_t iProbe = UINT32_MAX;
			while ( iLeft >= static_cast<int64_t>( nLo ) || iRight < nHi ) {
				if ( iLeft >= static_cast<int64_t>( nLo ) && m_slots[static_cast<uint32_t>( iLeft )].has_value() ) {
					iProbe = static_cast<uint32_t>( iLeft );
					break;
				}
				if ( iRight < nHi && m_slots[static_cast<uint32_t>( iRight )].has_value() ) {
					iProbe = static_cast<uint32_t>( iRight );
					break;
				}
				--iLeft;
				++iRight;
			}
			if ( iProbe == UINT32_MAX ) {
				return nLo;
			}
			if ( m_less( m_slots[iProbe]->key, key ) ) {
				nLo = iProbe + 1;
			} else {
				nHi = iProbe;
			}
		}
		return nLo;
	}

	SIRContainerResult grow( void )
	{
		const uint32_t nBefore = m_slots.getSize();
		if ( nBefore > UINT32_MAX / 2 ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const uint32_t nCapacity = nBefore == 0 ? 8 : nBefore * 2;
		SIRVector<std::optional<Entry>> staged( m_pAllocator );
		SIRContainerResult eResult = staged.tryResize( nCapacity );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		// Retain distributed gaps so middle inserts shift only local neighbors.
		uint32_t iMoved = 0;
		for ( uint32_t iIndex = 0; iIndex < nBefore; ++iIndex ) {
			if ( !m_slots[iIndex].has_value() ) {
				continue;
			}
			const uint32_t iTarget = ( uint32_t )( ( static_cast<uint64_t>( iMoved + 1 ) * nCapacity ) / ( m_nCount + 1 ) ) - 1;
			staged[iTarget].emplace( *m_slots[iIndex] );
			++iMoved;
		}
		m_slots = std::move( staged );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
};
#endif /* __SIR_PACKEDMEMORYMAP_H__ */
