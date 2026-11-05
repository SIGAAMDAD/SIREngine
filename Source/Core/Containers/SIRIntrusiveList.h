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

#ifndef __SIRENGINE_SIRINTRUSIVELIST_H__
#define __SIRENGINE_SIRINTRUSIVELIST_H__

#include <SIRPCH.hpp>

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
private:
	sirIntrusiveLink_t *m_pHead = nullptr;
	sirIntrusiveLink_t *m_pTail = nullptr;
	uint32_t m_nSize = 0;

public:
	SIRIntrusiveList( void ) noexcept = default;
	~SIRIntrusiveList( void ) noexcept
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

	sirContainerResult_t tryPushBack( sirIntrusiveLink_t *pLink ) noexcept
	{
		if ( pLink == nullptr || pLink->pOwner != nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
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
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryPushFront( sirIntrusiveLink_t *pLink ) noexcept
	{
		if ( pLink == nullptr || pLink->pOwner != nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( m_nSize == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
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
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryRemove( sirIntrusiveLink_t *pLink ) noexcept
	{
		if ( pLink == nullptr || pLink->pOwner != this ) {
			return SIR_CONTAINER_INVALID_HANDLE;
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
		return SIR_CONTAINER_OK;
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
};

#endif // __SIRENGINE_SIRINTRUSIVELIST_H__
