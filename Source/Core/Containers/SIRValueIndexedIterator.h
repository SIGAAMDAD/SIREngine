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

#ifndef __SIR_VALUEINDEXEDITERATOR_H__
#define __SIR_VALUEINDEXEDITERATOR_H__

#include <Core/SIRPCH.h>
#include <assert.h>
#include <iterator>
#include <stddef.h>
#include <stdint.h>

// Decode-on-dereference input iterator: never returns references into packed bytes.
template <typename Container>
class SIRValueIndexedIterator
{
public:
	using value_type = typename Container::value_type;
	using reference = value_type;
	using pointer = void;
	using difference_type = ptrdiff_t;
	using iterator_category = std::input_iterator_tag;

	SIRValueIndexedIterator( void ) noexcept = default;
	SIRValueIndexedIterator( const Container *pOwner, uint32_t iIndex ) noexcept
	    : m_pOwner( pOwner ),
	      m_iIndex( iIndex )
	{
	}

	reference operator*( void ) const noexcept
	{
		assert( m_pOwner != nullptr && m_iIndex < m_pOwner->size() );
		return ( *m_pOwner )[m_iIndex];
	}

	SIRValueIndexedIterator& operator++( void ) noexcept
	{
		++m_iIndex;
		return *this;
	}

	SIRValueIndexedIterator operator++( int ) noexcept
	{
		SIRValueIndexedIterator previous( *this );
		++*this;
		return previous;
	}

	bool operator==( const SIRValueIndexedIterator& rOther ) const noexcept
	{
		return m_pOwner == rOther.m_pOwner && m_iIndex == rOther.m_iIndex;
	}

	bool operator!=( const SIRValueIndexedIterator& rOther ) const noexcept
	{
		return !( *this == rOther );
	}

private:
	const Container *m_pOwner = nullptr;
	uint32_t m_iIndex = 0;
};

#endif /* __SIR_VALUEINDEXEDITERATOR_H__ */
