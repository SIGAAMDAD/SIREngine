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

#include <Audio/FMOD/SIRFMODMemoryBudget.h>
#include <Core/Containers/SIRRawStorage.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

SIRFMODMemoryBudget::~SIRFMODMemoryBudget( void ) noexcept
{
	if ( shutdown() != SIRAudioResult::SIR_AUDIO_OK ) {
		abort();
	}
}

SIRAudioResult SIRFMODMemoryBudget::configure( const sirContainerAllocator_t *pUpstream, uint64_t nBudgetBytes ) noexcept
{
	if ( m_pUpstream != nullptr || !SIRRawStorage::isValidAllocator( pUpstream ) ) {
		return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
	}
	m_pUpstream = pUpstream;
	m_nBudgetBytes = nBudgetBytes;
	m_Allocator = { this, allocateCallback, deallocateCallback, pUpstream->nDomainId, pUpstream->nOwnerTag };
	m_nCurrent.store( 0, std::memory_order_relaxed );
	m_nPeak.store( 0, std::memory_order_relaxed );
	m_nAllocations.store( 0, std::memory_order_relaxed );
	m_nFailures.store( 0, std::memory_order_relaxed );
	return SIRAudioResult::SIR_AUDIO_OK;
}

SIRAudioResult SIRFMODMemoryBudget::shutdown( void ) noexcept
{
	if ( m_nCurrent.load( std::memory_order_acquire ) != 0 ) {
		return SIRAudioResult::SIR_AUDIO_BUSY;
	}
	m_pUpstream = nullptr;
	m_Allocator = {};
	m_nBudgetBytes = 0;
	return SIRAudioResult::SIR_AUDIO_OK;
}

const sirContainerAllocator_t *SIRFMODMemoryBudget::getAllocator( void ) const noexcept
{
	return m_pUpstream != nullptr ? &m_Allocator : nullptr;
}

sirFMODMemoryBudgetStats_t SIRFMODMemoryBudget::getStats( void ) const noexcept
{
	return { m_nBudgetBytes, m_nCurrent.load( std::memory_order_acquire ), m_nPeak.load( std::memory_order_relaxed ),
	         m_nAllocations.load( std::memory_order_relaxed ), m_nFailures.load( std::memory_order_relaxed ) };
}

void *SIRFMODMemoryBudget::allocateCallback( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	SIRFMODMemoryBudget *pBudget = static_cast<SIRFMODMemoryBudget *>(pContext);
	if ( pBudget == nullptr || pBudget->m_pUpstream == nullptr || nBytes == 0 || nAlignment == 0 ||
	     ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
		return nullptr;
	}
	const size_t nRequiredAlignment = nAlignment < alignof( sirFMODMemoryHeader_t ) ? alignof( sirFMODMemoryHeader_t ) : nAlignment;
	size_t nCharge = 0;
	if ( !SIRRawStorage::checkedAdd( nBytes, sizeof( sirFMODMemoryHeader_t ), &nCharge ) ||
	     !SIRRawStorage::checkedAdd( nCharge, nRequiredAlignment - 1, &nCharge ) ) {
		pBudget->m_nFailures.fetch_add( 1, std::memory_order_relaxed );
		return nullptr;
	}
	uint64_t nCurrent = pBudget->m_nCurrent.load( std::memory_order_relaxed );
	for ( ;; ) {
		if ( nCharge > UINT64_MAX - nCurrent ||
		     ( pBudget->m_nBudgetBytes != 0 &&
		       nCharge > pBudget->m_nBudgetBytes - ( nCurrent > pBudget->m_nBudgetBytes ? pBudget->m_nBudgetBytes : nCurrent ) ) ) {
			pBudget->m_nFailures.fetch_add( 1, std::memory_order_relaxed );
			return nullptr;
		}
		if ( pBudget->m_nCurrent.compare_exchange_weak( nCurrent, nCurrent + nCharge, std::memory_order_acq_rel ) ) {
			break;
		}
	}
	void *pBase = pBudget->m_pUpstream->pfnAllocate( pBudget->m_pUpstream->pContext, nCharge, alignof( max_align_t ) );
	if ( pBase == nullptr || ( reinterpret_cast<uintptr_t>(pBase) & ( alignof( max_align_t ) - 1 ) ) != 0 ) {
		if ( pBase != nullptr ) {
			pBudget->m_pUpstream->pfnDeallocate( pBudget->m_pUpstream->pContext, pBase, nCharge, alignof( max_align_t ) );
		}
		pBudget->m_nCurrent.fetch_sub( nCharge, std::memory_order_release );
		pBudget->m_nFailures.fetch_add( 1, std::memory_order_relaxed );
		return nullptr;
	}
	const uintptr_t nStart = reinterpret_cast<uintptr_t>(pBase) + sizeof( sirFMODMemoryHeader_t );
	const uintptr_t nAligned = ( nStart + nRequiredAlignment - 1 ) & ~static_cast<uintptr_t>( nRequiredAlignment - 1 );
	sirFMODMemoryHeader_t *pHeader = reinterpret_cast<sirFMODMemoryHeader_t *>(  nAligned - sizeof( sirFMODMemoryHeader_t )  );
	pHeader->pBase = pBase;
	pHeader->nCharge = nCharge;
	uint64_t nPeak = pBudget->m_nPeak.load( std::memory_order_relaxed );
	while ( nPeak < nCurrent + nCharge && !pBudget->m_nPeak.compare_exchange_weak( nPeak, nCurrent + nCharge, std::memory_order_relaxed ) ) {
	}
	pBudget->m_nAllocations.fetch_add( 1, std::memory_order_relaxed );
	return reinterpret_cast<void *>(nAligned);
}

void SIRFMODMemoryBudget::deallocateCallback( void *pContext, void *pMemory, size_t, size_t ) noexcept
{
	if ( pMemory == nullptr ) {
		return;
	}
	SIRFMODMemoryBudget *pBudget = static_cast<SIRFMODMemoryBudget *>(pContext);
	sirFMODMemoryHeader_t *pHeader = ( sirFMODMemoryHeader_t * )( reinterpret_cast<uintptr_t>(pMemory) - sizeof(  sirFMODMemoryHeader_t  ) );
	const size_t nCharge = pHeader->nCharge;
	void *pBase = pHeader->pBase;
	pBudget->m_pUpstream->pfnDeallocate( pBudget->m_pUpstream->pContext, pBase, nCharge, alignof( max_align_t ) );
	pBudget->m_nCurrent.fetch_sub( nCharge, std::memory_order_release );
}
