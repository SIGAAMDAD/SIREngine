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

#ifndef __SIR_FENCERETIREDPTR_H__
#define __SIR_FENCERETIREDPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRProviderCompletion.h>
#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct sirFenceRetiredEntry_s
{
	void *pObject;
	void *pContext;
	void ( *pDestroy )( void *pContext, void *pObject ) noexcept;
	sirProviderCompletionSet_t required;
} sirFenceRetiredEntry_t;

typedef struct sirFenceRetirementPolicy_s
{
	void *pContext;
	void ( *pDestroy )( void *pContext, void *pObject ) noexcept;
	sirProviderCompletionSet_t required;
} sirFenceRetirementPolicy_t;

// Bounded completion-aware retirement queue. Only the native provider reports
// observed fence values, and only the owning executor calls collect().
class SIRFenceRetirementQueue
{
public:
	SIRFenceRetirementQueue( sirFenceRetiredEntry_t *pEntries, uint32_t nCapacity ) noexcept
	    : m_pEntries( pEntries ), m_nCapacity( nCapacity )
	{
	}
	~SIRFenceRetirementQueue( void ) noexcept
	{
		SIRScopedLock<SIRMutex> lock( m_Mutex );
		if ( m_nCount != 0 ) {
			abort();
		}
	}
	SIRFenceRetirementQueue( const SIRFenceRetirementQueue& ) = delete;
	SIRFenceRetirementQueue& operator=( const SIRFenceRetirementQueue& ) = delete;
	bool enqueue( const sirFenceRetiredEntry_t& rEntry ) noexcept
	{
		if ( rEntry.pObject == nullptr || rEntry.pDestroy == nullptr || !sirProviderCompletionValid( rEntry.required ) ) {
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
	uint32_t collect( const sirProviderCompletionSet_t& rObserved, uint32_t nMaximum ) noexcept
	{
		if ( !sirProviderCompletionValid( rObserved ) ) {
			return 0;
		}
		constexpr uint32_t SIR_FENCE_BATCH_SIZE = 16;
		sirFenceRetiredEntry_t szBatch[SIR_FENCE_BATCH_SIZE] = {};
		uint32_t nCollected = 0;
		while ( nCollected < nMaximum ) {
			uint32_t nSelected = 0;
			{
				SIRScopedLock<SIRMutex> lock( m_Mutex );
				uint32_t iEntry = 0;
				while ( iEntry < m_nCount && nSelected < SIR_FENCE_BATCH_SIZE && nCollected + nSelected < nMaximum ) {
					if ( sirProviderCompletionSatisfied( m_pEntries[iEntry].required, rObserved ) ) {
						szBatch[nSelected] = m_pEntries[iEntry];
						++nSelected;
						--m_nCount;
						m_pEntries[iEntry] = m_pEntries[m_nCount];
					} else {
						++iEntry;
					}
				}
			}
			if ( nSelected == 0 ) {
				break;
			}
			for ( uint32_t iBatch = 0; iBatch < nSelected; ++iBatch ) {
				szBatch[iBatch].pDestroy( szBatch[iBatch].pContext, szBatch[iBatch].pObject );
			}
			nCollected += nSelected;
		}
		return nCollected;
	}
	uint32_t pending( void ) noexcept
	{
		SIRScopedLock<SIRMutex> lock( m_Mutex );
		return m_nCount;
	}

private:
	sirFenceRetiredEntry_t *m_pEntries;
	uint32_t m_nCapacity;
	uint32_t m_nCount = 0;
	SIRMutex m_Mutex;
};

template <typename T>
class SIRFenceRetiredPtr
{
public:
	SIRFenceRetiredPtr( void ) noexcept = default;
	SIRFenceRetiredPtr( T *pObject, SIRFenceRetirementQueue& rQueue, const sirFenceRetirementPolicy_t& rPolicy ) noexcept
	    : m_pObject( pObject ), m_pQueue( &rQueue ), m_pContext( rPolicy.pContext ), m_pDestroy( rPolicy.pDestroy ),
	      m_Required( rPolicy.required )
	{
	}
	~SIRFenceRetiredPtr( void ) noexcept
	{
		if ( !reset() ) {
			abort();
		}
	}
	SIRFenceRetiredPtr( const SIRFenceRetiredPtr& ) = delete;
	SIRFenceRetiredPtr& operator=( const SIRFenceRetiredPtr& ) = delete;
	SIRFenceRetiredPtr( SIRFenceRetiredPtr&& rOther ) noexcept
	    : m_pObject( rOther.m_pObject ), m_pQueue( rOther.m_pQueue ), m_pContext( rOther.m_pContext ), m_pDestroy( rOther.m_pDestroy ),
	      m_Required( rOther.m_Required )
	{
		rOther.m_pObject = nullptr;
	}
	SIRFenceRetiredPtr& operator=( SIRFenceRetiredPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			if ( !reset() ) {
				abort();
			}
			m_pObject = rOther.m_pObject;
			m_pQueue = rOther.m_pQueue;
			m_pContext = rOther.m_pContext;
			m_pDestroy = rOther.m_pDestroy;
			m_Required = rOther.m_Required;
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
		const sirFenceRetiredEntry_t entry = { m_pObject, m_pContext, m_pDestroy, m_Required };
		if ( !m_pQueue->enqueue( entry ) ) {
			return false;
		}
		m_pObject = nullptr;
		return true;
	}

private:
	T *m_pObject = nullptr;
	SIRFenceRetirementQueue *m_pQueue = nullptr;
	void *m_pContext = nullptr;
	void ( *m_pDestroy )( void *, void * ) noexcept = nullptr;
	sirProviderCompletionSet_t m_Required {};
};

#endif // __SIR_FENCERETIREDPTR_H__
