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

#ifndef __SIRENGINE_SIRRCUPTR_H__
#define __SIRENGINE_SIRRCUPTR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <stdlib.h>
#include <atomic>

class SIRRCUReaderRecord
{
public:
	SIRRCUReaderRecord( void ) noexcept : m_nEpoch( 0 )
	{
	}
	SIRRCUReaderRecord( const SIRRCUReaderRecord& ) = delete;
	SIRRCUReaderRecord& operator=( const SIRRCUReaderRecord& ) = delete;
	
	std::atomic<uint64_t> m_nEpoch;
};

typedef struct sirRCURetiredEntry_s
{
	void *pObject;
	uint64_t nEpoch;
} sirRCURetiredEntry_t;

// One publishing writer; arbitrarily many readers up to the fixed capacity.
// The caller retains and frees retired objects ONLY after canReclaim().
class SIRRCUDomain
{
public:
	SIRRCUDomain( SIRRCUReaderRecord *pReaders, uint32_t nCapacity ) noexcept
	    : m_pReaders( pReaders ), m_nCapacity( nCapacity ), m_nClaimCursor( 0 ), m_nEpoch( 1 )
	{
	}
	~SIRRCUDomain( void ) noexcept
	{
		for ( uint32_t iReader = 0; iReader < m_nCapacity; ++iReader ) {
			if ( m_pReaders[iReader].m_nEpoch.load() != 0 ) {
				abort();
			}
		}
	}
	SIRRCUDomain( const SIRRCUDomain& ) = delete;
	SIRRCUDomain& operator=( const SIRRCUDomain& ) = delete;

	SIRRCUReaderRecord *claim( void ) noexcept
	{
		if ( m_nCapacity == 0 ) {
			return nullptr;
		}

		const uint32_t nNext = m_nClaimCursor.fetch_add( 1, std::memory_order_relaxed );
		const uint32_t nMask = m_nCapacity - 1;
		const uint32_t iReaderStart = ( ( m_nCapacity & nMask ) == 0 ) ? ( nNext & nMask ) : ( nNext % m_nCapacity );
		uint32_t iReader = iReaderStart;
		for ( uint32_t nAttempt = 0; nAttempt < m_nCapacity; ++nAttempt ) {
			uint64_t nEmpty = 0;
			if ( m_pReaders[iReader].m_nEpoch.compareExchangeStrong( nEmpty, UINT64_MAX ) ) {
				return &m_pReaders[iReader];
			}
			++iReader;
			if ( iReader == m_nCapacity ) {
				iReader = 0;
			}
		}
		return nullptr;
	}
	uint64_t currentEpoch( void ) const noexcept
	{
		return m_nEpoch.load();
	}
	bool canReclaim( uint64_t nRetiredEpoch ) const noexcept
	{
		if ( nRetiredEpoch == 0 ) {
			return false;
		}
		for ( uint32_t iReader = 0; iReader < m_nCapacity; ++iReader ) {
			const uint64_t nReaderEpoch = m_pReaders[iReader].m_nEpoch.load();
			if ( nReaderEpoch == UINT64_MAX ) {
				return false;
			}
			if ( nReaderEpoch != 0 && nReaderEpoch <= nRetiredEpoch ) {
				return false;
			}
		}
		return true;
	}
	template <typename T>
	sirRCURetiredEntry_t publish( std::atomic<T *>& rSource, T *pReplacement ) noexcept
	{
		const uint64_t nOldEpoch = m_nEpoch.load();
		if ( nOldEpoch >= UINT64_MAX - 1 ) {
			abort();
		}
		T *pOld = rSource.exchange( pReplacement );
		m_nEpoch.store( nOldEpoch + 1 );
		return { pOld, nOldEpoch };
	}

private:
	SIRRCUReaderRecord *m_pReaders;
	uint32_t m_nCapacity;
	std::atomic<uint32_t> m_nClaimCursor;
	std::atomic<uint64_t> m_nEpoch;
};

typedef struct sirRCURetirementEntry_s
{
	sirRCURetiredEntry_t retired;
	void *pContext;
	void ( *pDestroy )( void *pContext, void *pObject ) noexcept;
} sirRCURetirementEntry_t;

// Writer-owned bounded retirement queue for RCU. Reader epochs protect each
// retained object, and callbacks run only after the corresponding grace period.
class SIRRCURetireList
{
public:
	SIRRCURetireList( SIRRCUDomain& rDomain, sirRCURetirementEntry_t *pEntries, uint32_t nCapacity ) noexcept
	    : m_pDomain( &rDomain ), m_pEntries( pEntries ), m_nCapacity( nCapacity )
	{
	}
	~SIRRCURetireList( void ) noexcept
	{
		if ( m_nCount != 0 ) {
			abort();
		}
	}
	bool retire( const sirRCURetirementEntry_t& rEntry ) noexcept
	{
		if ( rEntry.retired.pObject == nullptr || rEntry.pDestroy == nullptr || m_nCount == m_nCapacity ) {
			return false;
		}
		if ( rEntry.retired.nEpoch == 0 ) {
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
			const sirRCURetirementEntry_t entry = m_pEntries[iEntry];
			if ( !m_pDomain->canReclaim( entry.retired.nEpoch ) ) {
				++iEntry;
				continue;
			}
			--m_nCount;
			m_pEntries[iEntry] = m_pEntries[m_nCount];
			entry.pDestroy( entry.pContext, entry.retired.pObject );
			++nCollected;
		}
		return nCollected;
	}
	uint32_t pending( void ) const noexcept
	{
		return m_nCount;
	}

private:
	SIRRCUDomain *m_pDomain;
	sirRCURetirementEntry_t *m_pEntries;
	uint32_t m_nCapacity;
	uint32_t m_nCount = 0;
};

template <typename T>
class SIRRCUPtr
{
public:
	SIRRCUPtr( void ) noexcept = default;
	~SIRRCUPtr( void ) noexcept
	{
		reset();
	}
	SIRRCUPtr( const SIRRCUPtr& ) = delete;
	SIRRCUPtr& operator=( const SIRRCUPtr& ) = delete;
	SIRRCUPtr( SIRRCUPtr&& rOther ) noexcept : m_pReader( rOther.m_pReader ), m_pObject( rOther.m_pObject )
	{
		rOther.m_pReader = nullptr;
		rOther.m_pObject = nullptr;
	}
	SIRRCUPtr& operator=( SIRRCUPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pReader = rOther.m_pReader;
			m_pObject = rOther.m_pObject;
			rOther.m_pReader = nullptr;
			rOther.m_pObject = nullptr;
		}
		return *this;
	}
	bool read( SIRRCUDomain& rDomain, const std::atomic<T *>& rSource ) noexcept
	{
		reset();
		SIRRCUReaderRecord *pReader = rDomain.claim();
		if ( pReader == nullptr ) {
			return false;
		}
		T *pObject = nullptr;
		for ( ;; ) {
			const uint64_t nEpoch = rDomain.currentEpoch();
			pReader->m_nEpoch.store( nEpoch );
			pObject = rSource.load();
			if ( nEpoch == rDomain.currentEpoch() ) {
				break;
			}
			pReader->m_nEpoch.store( UINT64_MAX );
		}
		m_pReader = pReader;
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
		if ( m_pReader != nullptr ) {
			m_pReader->m_nEpoch.store( 0 );
		}
		m_pReader = nullptr;
		m_pObject = nullptr;
	}

private:
	SIRRCUReaderRecord *m_pReader = nullptr;
	T *m_pObject = nullptr;
};

#endif // __SIRENGINE_SIRRCUPTR_H__
