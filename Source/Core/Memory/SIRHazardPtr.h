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

#ifndef SIRENGINE_SIRHAZARDPTR_H
#define SIRENGINE_SIRHAZARDPTR_H

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRAtomic.h>
#include <stdint.h>
#include <stdlib.h>

// Fixed-capacity storage is supplied by the caller. Every concurrent reader
// needs a distinct claimed record until its hazard guard is destroyed.
class SIRHazardRecord
{
public:
	SIRHazardRecord( void ) noexcept : m_pProtected( nullptr ), m_nClaimed( 0 )
	{
	}
	SIRHazardRecord( const SIRHazardRecord& ) = delete;
	SIRHazardRecord& operator=( const SIRHazardRecord& ) = delete;

	SIRAtomic<void *> m_pProtected;
	SIRAtomic<uint32_t> m_nClaimed;
};

class SIRHazardDomain
{
public:
	SIRHazardDomain( SIRHazardRecord *pRecords, uint32_t nCapacity ) noexcept
	    : m_pRecords( pRecords ), m_nCapacity( nCapacity ), m_nClaimCursor( 0 )
	{
	}
	~SIRHazardDomain( void ) noexcept
	{
		for ( uint32_t iRecord = 0; iRecord < m_nCapacity; ++iRecord ) {
			if ( m_pRecords[iRecord].m_nClaimed.load() != 0 ) {
				abort();
			}
		}
	}
	SIRHazardDomain( const SIRHazardDomain& ) = delete;
	SIRHazardDomain& operator=( const SIRHazardDomain& ) = delete;

	SIRHazardRecord *claim( void ) noexcept
	{
		if ( m_nCapacity == 0 ) {
			return nullptr;
		}

		const uint32_t nNext = m_nClaimCursor.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nMask = m_nCapacity - 1;
		const uint32_t iRecordStart = ( ( m_nCapacity & nMask ) == 0 ) ? ( nNext & nMask ) : ( nNext % m_nCapacity );
		uint32_t iRecord = iRecordStart;
		for ( uint32_t nAttempt = 0; nAttempt < m_nCapacity; ++nAttempt ) {
			uint32_t nVacant = 0;
			if ( m_pRecords[iRecord].m_nClaimed.compareExchangeStrong( nVacant, 1 ) ) {
				return &m_pRecords[iRecord];
			}
			++iRecord;
			if ( iRecord == m_nCapacity ) {
				iRecord = 0;
			}
		}
		return nullptr;
	}
	bool isProtected( const void *pObject ) const noexcept
	{
		if ( pObject == nullptr ) {
			return false;
		}
		for ( uint32_t iRecord = 0; iRecord < m_nCapacity; ++iRecord ) {
			if ( m_pRecords[iRecord].m_pProtected.load() == pObject ) {
				return true;
			}
		}
		return false;
	}

private:
	SIRHazardRecord *m_pRecords;
	uint32_t m_nCapacity;
	SIRAtomic<uint32_t> m_nClaimCursor;
};

template <typename T>
class SIRHazardPtr
{
public:
	SIRHazardPtr( void ) noexcept = default;
	~SIRHazardPtr( void ) noexcept
	{
		reset();
	}
	SIRHazardPtr( const SIRHazardPtr& ) = delete;
	SIRHazardPtr& operator=( const SIRHazardPtr& ) = delete;
	SIRHazardPtr( SIRHazardPtr&& rOther ) noexcept : m_pRecord( rOther.m_pRecord ), m_pObject( rOther.m_pObject )
	{
		rOther.m_pRecord = nullptr;
		rOther.m_pObject = nullptr;
	}
	SIRHazardPtr& operator=( SIRHazardPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pRecord = rOther.m_pRecord;
			m_pObject = rOther.m_pObject;
			rOther.m_pRecord = nullptr;
			rOther.m_pObject = nullptr;
		}
		return *this;
	}
	bool protect( SIRHazardDomain& rDomain, const SIRAtomic<T *>& rSource ) noexcept
	{
		reset();
		SIRHazardRecord *pRecord = rDomain.claim();
		if ( pRecord == nullptr ) {
			return false;
		}
		T *pObject = nullptr;
		do {
			pObject = rSource.load();
			pRecord->m_pProtected.store( pObject );
		} while ( pObject != rSource.load() );
		m_pRecord = pRecord;
		m_pObject = pObject;
		return true;
	}
	T *get( void ) const noexcept
	{
		return m_pObject;
	}
	T *operator->( void ) const noexcept
	{
		return m_pObject;
	}
	void reset( void ) noexcept
	{
		if ( m_pRecord != nullptr ) {
			m_pRecord->m_pProtected.store( nullptr );
			m_pRecord->m_nClaimed.store( 0 );
		}
		m_pRecord = nullptr;
		m_pObject = nullptr;
	}

private:
	SIRHazardRecord *m_pRecord = nullptr;
	T *m_pObject = nullptr;
};

typedef struct sirHazardRetiredEntry_s
{
	void *pObject;
	void *pContext;
	void ( *pDestroy )( void *pContext, void *pObject ) noexcept;
} sirHazardRetiredEntry_t;

// Single-writer retirement list. Reclamation checks every hazard record;
// the caller controls when retired objects are submitted for reclamation.
class SIRHazardRetireList
{
public:
	SIRHazardRetireList( SIRHazardDomain& rDomain, sirHazardRetiredEntry_t *pEntries, uint32_t nCapacity ) noexcept
	    : m_pDomain( &rDomain ), m_pEntries( pEntries ), m_nCapacity( nCapacity )
	{
	}
	~SIRHazardRetireList( void ) noexcept
	{
		if ( m_nCount != 0 ) {
			abort();
		}
	}
	bool retire( const sirHazardRetiredEntry_t& rEntry ) noexcept
	{
		if ( rEntry.pObject == nullptr || rEntry.pDestroy == nullptr || m_nCount >= m_nCapacity ) {
			return false;
		}
		m_pEntries[m_nCount] = rEntry;
		++m_nCount;
		return true;
	}
	uint32_t collect( void ) noexcept
	{
		uint32_t nCollected = 0;
		uint32_t iEntry = 0;
		while ( iEntry < m_nCount ) {
			const sirHazardRetiredEntry_t entry = m_pEntries[iEntry];
			if ( m_pDomain->isProtected( entry.pObject ) ) {
				++iEntry;
				continue;
			}
			--m_nCount;
			m_pEntries[iEntry] = m_pEntries[m_nCount];
			entry.pDestroy( entry.pContext, entry.pObject );
			++nCollected;
		}
		return nCollected;
	}
	uint32_t pending( void ) const noexcept
	{
		return m_nCount;
	}

private:
	SIRHazardDomain *m_pDomain;
	sirHazardRetiredEntry_t *m_pEntries;
	uint32_t m_nCapacity;
	uint32_t m_nCount = 0;
};

#endif // SIRENGINE_SIRHAZARDPTR_H
