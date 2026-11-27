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

#ifndef __SIR_FROZENOPTIONALITERATOR_H__
#define __SIR_FROZENOPTIONALITERATOR_H__

#include <Core/SIRPCH.h>
#include <assert.h>
#include <iterator>
#include <optional>
#include <stddef.h>
#include <stdint.h>

template <typename Entry>
class SIRFrozenOptionalIterator
{
public:
	using value_type = Entry;
	using reference = const Entry&;
	using pointer = const Entry *;
	using difference_type = ptrdiff_t;
	using iterator_category = std::forward_iterator_tag;

	SIRFrozenOptionalIterator( void ) noexcept = default;
	SIRFrozenOptionalIterator( const std::optional<Entry> *pSlots, uint32_t nCapacity, uint32_t iIndex ) noexcept
	    : m_pSlots( pSlots ),
	      m_nCapacity( nCapacity ),
	      m_iIndex( iIndex )
	{
		skipEmpty();
	}

	reference operator*( void ) const noexcept
	{
		assert( m_pSlots != nullptr && m_iIndex < m_nCapacity && m_pSlots[m_iIndex].has_value() );
		return *m_pSlots[m_iIndex];
	}

	pointer operator->( void ) const noexcept
	{
		return &operator*();
	}

	SIRFrozenOptionalIterator& operator++( void ) noexcept
	{
		assert( m_iIndex < m_nCapacity );
		++m_iIndex;
		skipEmpty();
		return *this;
	}

	SIRFrozenOptionalIterator operator++( int ) noexcept
	{
		SIRFrozenOptionalIterator previous( *this );
		++*this;
		return previous;
	}

	bool operator==( const SIRFrozenOptionalIterator& rOther ) const noexcept
	{
		return m_pSlots == rOther.m_pSlots && m_nCapacity == rOther.m_nCapacity && m_iIndex == rOther.m_iIndex;
	}

	bool operator!=( const SIRFrozenOptionalIterator& rOther ) const noexcept
	{
		return !( *this == rOther );
	}

private:
	void skipEmpty( void ) noexcept
	{
		while ( m_iIndex < m_nCapacity && !m_pSlots[m_iIndex].has_value() ) {
			++m_iIndex;
		}
	}

	const std::optional<Entry> *m_pSlots = nullptr;
	uint32_t m_nCapacity = 0;
	uint32_t m_iIndex = 0;
};

#endif /* __SIR_FROZENOPTIONALITERATOR_H__ */
