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

#ifndef __SIR_INDEXEDCONTAINERITERATOR_H__
#define __SIR_INDEXEDCONTAINERITERATOR_H__

#include <Core/SIRPCH.h>
#include <assert.h>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

// Logical-index iterator for noncontiguous containers with O(1) operator[].
// Invalidate on structural mutation; never assume contiguous underlying storage.
template <typename Container, bool IS_CONST>
class SIRIndexedContainerIterator
{
	template <typename, bool> friend class SIRIndexedContainerIterator;

public:
	using value_type = typename Container::value_type;
	using difference_type = ptrdiff_t;
	using iterator_category = std::random_access_iterator_tag;
	using reference = typename std::conditional<IS_CONST, const value_type&, value_type&>::type;
	using pointer = typename std::conditional<IS_CONST, const value_type *, value_type *>::type;
	using owner_type = typename std::conditional<IS_CONST, const Container, Container>::type;

	SIRIndexedContainerIterator( void ) noexcept = default;

	SIRIndexedContainerIterator( owner_type *pOwner, uint32_t iIndex ) noexcept
	    : m_pOwner( pOwner ),
	      m_iIndex( iIndex )
	{
	}

	template <bool OTHER_CONST, typename std::enable_if<IS_CONST && !OTHER_CONST, int>::type = 0>
	SIRIndexedContainerIterator( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) noexcept
	    : m_pOwner( rOther.m_pOwner ),
	      m_iIndex( rOther.m_iIndex )
	{
	}

	reference operator*( void ) const noexcept
	{
		assert( m_pOwner != nullptr && m_iIndex < m_pOwner->size() );
		return ( *m_pOwner )[m_iIndex];
	}

	pointer operator->( void ) const noexcept
	{
		return &operator*();
	}

	SIRIndexedContainerIterator& operator++( void ) noexcept
	{
		++m_iIndex;
		return *this;
	}

	SIRIndexedContainerIterator operator++( int ) noexcept
	{
		SIRIndexedContainerIterator previous( *this );
		++*this;
		return previous;
	}

	SIRIndexedContainerIterator& operator--( void ) noexcept
	{
		--m_iIndex;
		return *this;
	}

	SIRIndexedContainerIterator operator--( int ) noexcept
	{
		SIRIndexedContainerIterator previous( *this );
		--*this;
		return previous;
	}

	SIRIndexedContainerIterator& operator+=( difference_type nOffset ) noexcept
	{
		assert( nOffset >= -static_cast<difference_type>( m_iIndex ) );
		assert( nOffset <= static_cast<difference_type>( UINT32_MAX - m_iIndex ) );
		const difference_type nIndex = static_cast<difference_type>( m_iIndex ) + nOffset;
		assert( nIndex >= 0 && nIndex <= UINT32_MAX );
		m_iIndex = static_cast<uint32_t>( nIndex );
		return *this;
	}

	SIRIndexedContainerIterator& operator-=( difference_type nOffset ) noexcept
	{
		assert( nOffset != PTRDIFF_MIN );
		return *this += -nOffset;
	}

	SIRIndexedContainerIterator operator+( difference_type nOffset ) const noexcept
	{
		SIRIndexedContainerIterator result( *this );
		return result += nOffset;
	}

	friend SIRIndexedContainerIterator operator+( difference_type nOffset, const SIRIndexedContainerIterator& rIterator ) noexcept
	{
		return rIterator + nOffset;
	}

	SIRIndexedContainerIterator operator-( difference_type nOffset ) const noexcept
	{
		SIRIndexedContainerIterator result( *this );
		return result -= nOffset;
	}

	template <bool OTHER_CONST>
	difference_type operator-( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		assert( m_pOwner == rOther.m_pOwner );
		return static_cast<difference_type>( m_iIndex ) - static_cast<difference_type>( rOther.m_iIndex );
	}

	reference operator[]( difference_type nOffset ) const noexcept
	{
		return *( *this + nOffset );
	}

	template <bool OTHER_CONST>
	bool operator==( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		return m_pOwner == rOther.m_pOwner && m_iIndex == rOther.m_iIndex;
	}

	template <bool OTHER_CONST>
	bool operator!=( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		return !( *this == rOther );
	}

	template <bool OTHER_CONST>
	bool operator<( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		assert( m_pOwner == rOther.m_pOwner );
		return m_iIndex < rOther.m_iIndex;
	}

	template <bool OTHER_CONST>
	bool operator>( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		return rOther < *this;
	}

	template <bool OTHER_CONST>
	bool operator<=( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		return !( rOther < *this );
	}

	template <bool OTHER_CONST>
	bool operator>=( const SIRIndexedContainerIterator<Container, OTHER_CONST>& rOther ) const noexcept
	{
		return !( *this < rOther );
	}

private:
	owner_type *m_pOwner = nullptr;
	uint32_t m_iIndex = 0;
};

#endif // __SIR_INDEXEDCONTAINERITERATOR_H__
