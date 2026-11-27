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

#ifndef __SIR_INTRUSIVELIST_H__
#define __SIR_INTRUSIVELIST_H__

#include <Core/SIRPCH.h>
#include <iterator>
#include <type_traits>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <stdint.h>

// Link lifetime is owned by the containing object, never by this list.
typedef struct sirIntrusiveLink_s
{
	struct sirIntrusiveLink_s *pPrevious;
	struct sirIntrusiveLink_s *pNext;
	const void *pOwner;
} sirIntrusiveLink_t;

class SIRIntrusiveList
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

	using value_type = sirIntrusiveLink_t;
	using difference_type = ptrdiff_t;

	template <bool IS_CONST>
	class IteratorBase
	{
		template <bool> friend class IteratorBase;
	public:
		using value_type = sirIntrusiveLink_t;
		using difference_type = ptrdiff_t;
		using iterator_category = std::forward_iterator_tag;
		using pointer = typename std::conditional<IS_CONST, const value_type *, value_type *>::type;
		using reference = typename std::conditional<IS_CONST, const value_type&, value_type&>::type;

		IteratorBase( void ) noexcept = default;
		explicit IteratorBase( pointer pLink ) noexcept : m_pLink( pLink ) {}
		template <bool OTHER_CONST, typename std::enable_if<IS_CONST && !OTHER_CONST, int>::type = 0>
		IteratorBase( const IteratorBase<OTHER_CONST>& rOther ) noexcept : m_pLink( rOther.m_pLink ) {}
		reference operator*( void ) const noexcept
		{
			assert( m_pLink != nullptr );
			return *m_pLink;
		}
		pointer operator->( void ) const noexcept
		{
			return m_pLink;
		}
		IteratorBase& operator++( void ) noexcept
		{
			assert( m_pLink != nullptr );
			m_pLink = m_pLink->pNext;
			return *this;
		}
		IteratorBase operator++( int ) noexcept
		{
			IteratorBase old( *this );
			++*this;
			return old;
		}
		bool operator==( const IteratorBase& rOther ) const noexcept
		{
			return m_pLink == rOther.m_pLink;
		}
		bool operator!=( const IteratorBase& rOther ) const noexcept
		{
			return !( *this == rOther );
		}
	private:
		pointer m_pLink = nullptr;
	};

	using iterator = IteratorBase<false>;
	using const_iterator = IteratorBase<true>;
	iterator begin( void ) noexcept
	{
		return iterator( m_pHead );
	}
	iterator end( void ) noexcept
	{
		return iterator();
	}
	const_iterator begin( void ) const noexcept
	{
		return const_iterator( m_pHead );
	}
	const_iterator end( void ) const noexcept
	{
		return const_iterator();
	}
	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}
	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	SIRContainerResult tryPopFront( sirIntrusiveLink_t **ppLink ) noexcept
	{
		if ( ppLink == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		*ppLink = nullptr;
		if ( m_pHead == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		*ppLink = m_pHead;
		return tryRemove( m_pHead );
	}

	SIRContainerResult tryPopBack( sirIntrusiveLink_t **ppLink ) noexcept
	{
		if ( ppLink == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		*ppLink = nullptr;
		if ( m_pTail == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		*ppLink = m_pTail;
		return tryRemove( m_pTail );
	}

	explicit SIRIntrusiveList( void ) noexcept = default;

	~SIRIntrusiveList() noexcept
	{
		clear();
	}

	SIRIntrusiveList( const SIRIntrusiveList& ) = delete;
	SIRIntrusiveList& operator=( const SIRIntrusiveList& ) = delete;
	SIRIntrusiveList( SIRIntrusiveList&& ) = delete;
	SIRIntrusiveList& operator=( SIRIntrusiveList&& ) = delete;

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	bool isEmpty( void ) const noexcept
	{
		return m_nSize == 0;
	}

	sirIntrusiveLink_t *front( void ) const noexcept
	{
		return m_pHead;
	}

	sirIntrusiveLink_t *back( void ) const noexcept
	{
		return m_pTail;
	}

	SIRContainerResult tryPushBack( sirIntrusiveLink_t *pLink ) noexcept
	{
		if ( pLink == nullptr || pLink->pOwner != nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		pLink->pPrevious = m_pTail;
		pLink->pNext = nullptr;
		pLink->pOwner = this;
		if ( m_pTail != nullptr ) {
			m_pTail->pNext = pLink;
		} else {
			m_pHead = pLink;
		}
		m_pTail = pLink;
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPushFront( sirIntrusiveLink_t *pLink ) noexcept
	{
		if ( pLink == nullptr || pLink->pOwner != nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		pLink->pPrevious = nullptr;
		pLink->pNext = m_pHead;
		pLink->pOwner = this;
		if ( m_pHead != nullptr ) {
			m_pHead->pPrevious = pLink;
		} else {
			m_pTail = pLink;
		}
		m_pHead = pLink;
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryRemove( sirIntrusiveLink_t *pLink ) noexcept
	{
		if ( pLink == nullptr || pLink->pOwner != this ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_HANDLE;
		}
		if ( pLink->pPrevious != nullptr ) {
			pLink->pPrevious->pNext = pLink->pNext;
		} else {
			m_pHead = pLink->pNext;
		}
		if ( pLink->pNext != nullptr ) {
			pLink->pNext->pPrevious = pLink->pPrevious;
		} else {
			m_pTail = pLink->pPrevious;
		}
		pLink->pPrevious = nullptr;
		pLink->pNext = nullptr;
		pLink->pOwner = nullptr;
		--m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		sirIntrusiveLink_t *pLink = m_pHead;
		while ( pLink != nullptr ) {
			sirIntrusiveLink_t *pNext = pLink->pNext;
			pLink->pPrevious = nullptr;
			pLink->pNext = nullptr;
			pLink->pOwner = nullptr;
			pLink = pNext;
		}
		m_pHead = nullptr;
		m_pTail = nullptr;
		m_nSize = 0;
	}

private:
	sirIntrusiveLink_t *m_pHead = nullptr;
	sirIntrusiveLink_t *m_pTail = nullptr;
	uint32_t m_nSize = 0;
};

#endif // __SIR_INTRUSIVELIST_H__
