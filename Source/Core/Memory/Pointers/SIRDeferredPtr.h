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

#ifndef __SIR_SIRDEFERREDPTR_H__
#define __SIR_SIRDEFERREDPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>

typedef struct sirDeferredRetirementEntry_s
{
	void *pObject;
	void *pContext;
	void ( *pDestroy )( void *pContext, void *pObject ) noexcept;
} sirDeferredRetirementEntry_t;

// A caller-allocated bounded queue. Multiple producers may enqueue. The
// consumer drains on the required owner thread; callbacks run outside lock.
class SIRDeferredRetirementQueue
{
public:
	SIRDeferredRetirementQueue( sirDeferredRetirementEntry_t *pEntries, uint32_t nCapacity ) noexcept
	    : m_pEntries( pEntries ), m_nCapacity( nCapacity )
	{
	}
	~SIRDeferredRetirementQueue( void ) noexcept
	{
		SIRScopedLock<SIRMutex> lock( m_Mutex );
		if ( m_nCount != 0 ) {
			abort();
		}
	}
	SIRDeferredRetirementQueue( const SIRDeferredRetirementQueue& ) noexcept = delete;
	SIRDeferredRetirementQueue& operator=( const SIRDeferredRetirementQueue& ) noexcept = delete;

	bool enqueue( const sirDeferredRetirementEntry_t& rEntry ) noexcept
	{
		if ( rEntry.pObject == nullptr || rEntry.pDestroy == nullptr ) {
			return false;
		}
		SIRScopedLock<SIRMutex> lock( m_Mutex );
		if ( m_nCount >= m_nCapacity ) {
			return false;
		}
		m_pEntries[m_nCount] = rEntry;
		++m_nCount;
		return true;
	}

	uint32_t drain( uint32_t nMaximum ) noexcept
	{
		constexpr uint32_t SIR_DEFERRED_BATCH_SIZE = 32;
		sirDeferredRetirementEntry_t szBatch[SIR_DEFERRED_BATCH_SIZE] = {};
		uint32_t nDrained = 0;
		while ( nDrained < nMaximum ) {
			uint32_t nSelected = 0;
			{
				SIRScopedLock<SIRMutex> lock( m_Mutex );
				while ( nSelected < SIR_DEFERRED_BATCH_SIZE && nDrained + nSelected < nMaximum && m_nCount != 0 ) {
					--m_nCount;
					szBatch[nSelected] = m_pEntries[m_nCount];
					++nSelected;
				}
			}
			if ( nSelected == 0 ) {
				break;
			}
			for ( uint32_t iEntry = 0; iEntry < nSelected; ++iEntry ) {
				szBatch[iEntry].pDestroy( szBatch[iEntry].pContext, szBatch[iEntry].pObject );
			}
			nDrained += nSelected;
		}
		return nDrained;
	}
	
	uint32_t pending( void ) noexcept
	{
		SIRScopedLock<SIRMutex> lock( m_Mutex );
		return m_nCount;
	}

private:
	sirDeferredRetirementEntry_t *m_pEntries;
	uint32_t m_nCapacity;
	uint32_t m_nCount = 0;
	SIRMutex m_Mutex;
};

template <typename T>
class SIRDeferredPtr
{
public:
	SIRDeferredPtr( void ) noexcept = default;
	SIRDeferredPtr( T *pObject, SIRDeferredRetirementQueue& rQueue, void *pContext,
	                void ( *pDestroy )( void *pContext, void *pObject ) noexcept ) noexcept
	    : m_pObject( pObject ), m_pQueue( &rQueue ), m_pContext( pContext ), m_pDestroy( pDestroy )
	{
	}
	~SIRDeferredPtr( void ) noexcept
	{
		if ( !reset() ) {
			abort();
		}
	}
	SIRDeferredPtr( const SIRDeferredPtr& ) = delete;
	SIRDeferredPtr& operator=( const SIRDeferredPtr& ) = delete;
	SIRDeferredPtr( SIRDeferredPtr&& rOther ) noexcept
	    : m_pObject( rOther.m_pObject ), m_pQueue( rOther.m_pQueue ), m_pContext( rOther.m_pContext ), m_pDestroy( rOther.m_pDestroy )
	{
		rOther.m_pObject = nullptr;
	}
	SIRDeferredPtr& operator=( SIRDeferredPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			if ( !reset() ) {
				abort();
			}
			m_pObject = rOther.m_pObject;
			m_pQueue = rOther.m_pQueue;
			m_pContext = rOther.m_pContext;
			m_pDestroy = rOther.m_pDestroy;
			rOther.m_pObject = nullptr;
		}
		return *this;
	}
	T *get( void ) const noexcept
	{
		return m_pObject;
	}
	bool reset( void ) noexcept
	{
		if ( m_pObject == nullptr ) {
			return true;
		}
		if ( m_pQueue == nullptr || m_pDestroy == nullptr ) {
			return false;
		}
		const sirDeferredRetirementEntry_t entry = { m_pObject, m_pContext, m_pDestroy };
		if ( !m_pQueue->enqueue( entry ) ) {
			return false;
		}
		m_pObject = nullptr;
		return true;
	}

private:
	T *m_pObject = nullptr;
	SIRDeferredRetirementQueue *m_pQueue = nullptr;
	void *m_pContext = nullptr;
	void ( *m_pDestroy )( void *, void * ) noexcept = nullptr;
};

#endif // __SIR_SIRDEFERREDPTR_H__
