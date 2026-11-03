/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Memory/Allocators/SIRPageRunAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <cstring>
#include <limits>

SIRPageRunAllocator::SIRPageRunAllocator( void )
    : m_pSegment( nullptr ), m_pBase( nullptr ), m_pFreeRuns( nullptr ), m_pAllocationPages( nullptr ), m_nPageCount( 0 ),
      m_nPageBytes( 0 ), m_nFreeHead( SIR_NONE ), m_nAllocationCount( 0 ), m_nAllocatedBytes( 0 )
{
}

uint64_t SIRPageRunAllocator::getRequiredMetadataBytes( uint64_t nCapacityBytes, uint64_t nPageBytes )
{
	if ( nCapacityBytes == 0 || nPageBytes == 0 || !SIR_IS_POWER_OF_TWO( nPageBytes ) || nCapacityBytes % nPageBytes != 0 ||
	     nCapacityBytes / nPageBytes > UINT32_MAX || nCapacityBytes / nPageBytes > UINT64_MAX / ( sizeof( sirFreeRun_t ) + 4 ) ) {
		return 0;
	}
	return ( nCapacityBytes / nPageBytes ) * ( sizeof( sirFreeRun_t ) + 4 ) + 8;
}

bool SIRPageRunAllocator::init( SIRMemorySegment *pSegment )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || pSegment->getPageSize() > UINT32_MAX ) {
		return false;
	}
	const uint64_t nBytes = getRequiredMetadataBytes( pSegment->getReservedBytes(), pSegment->getPageSize() );
	if ( nBytes == 0 ) {
		return false;
	}
	void *pRaw = pSegment->getManager()->allocateMetadata( nBytes, alignof( uint64_t ), pSegment->getDomain() );
	if ( pRaw == nullptr ) {
		return false;
	}
	m_nPageCount = ( ( uint32_t )( pSegment->getReservedBytes() / pSegment->getPageSize() ) );
	m_nPageBytes = ( ( uint32_t )( pSegment->getPageSize() ) );
	m_pFreeRuns = ( ( sirFreeRun_t * )( pRaw ) );
	m_pAllocationPages = ( ( uint32_t * )( m_pFreeRuns + m_nPageCount ) );
	m_pFreeRuns[0] = { m_nPageCount, SIR_NONE, SIR_NONE };
	m_nFreeHead = 0;
	m_pSegment = pSegment;
	m_pBase = ( ( uint8_t * )( pSegment->getBase() ) );
	return true;
}

void SIRPageRunAllocator::unlinkRun( uint32_t nFirst )
{
	const sirFreeRun_t run = m_pFreeRuns[nFirst];
	if ( run.nPrevious == SIR_NONE ) {
		m_nFreeHead = run.nNext;
	} else {
		m_pFreeRuns[run.nPrevious].nNext = run.nNext;
	}
	if ( run.nNext != SIR_NONE ) {
		m_pFreeRuns[run.nNext].nPrevious = run.nPrevious;
	}
	m_pFreeRuns[nFirst] = {};
}

void SIRPageRunAllocator::insertRun( uint32_t nFirst, uint32_t nPages )
{
	uint32_t nPrevious = SIR_NONE;
	uint32_t nNext = m_nFreeHead;
	while ( nNext != SIR_NONE && nNext < nFirst ) {
		nPrevious = nNext;
		nNext = m_pFreeRuns[nNext].nNext;
	}
	// Merge with the previous range, preserving its list links.
	if ( nPrevious != SIR_NONE && nPrevious + m_pFreeRuns[nPrevious].nPages == nFirst ) {
		nFirst = nPrevious;
		m_pFreeRuns[nFirst].nPages += nPages;
	} else {
		m_pFreeRuns[nFirst] = { nPages, nNext, nPrevious };
		if ( nPrevious == SIR_NONE ) {
			m_nFreeHead = nFirst;
		} else {
			m_pFreeRuns[nPrevious].nNext = nFirst;
		}
		if ( nNext != SIR_NONE ) {
			m_pFreeRuns[nNext].nPrevious = nFirst;
		}
	}
	if ( nNext != SIR_NONE && nFirst + m_pFreeRuns[nFirst].nPages == nNext ) {
		m_pFreeRuns[nFirst].nPages += m_pFreeRuns[nNext].nPages;
		unlinkRun( nNext );
	}
}

void *SIRPageRunAllocator::alloc( uint64_t nBytes, uint64_t nAlignment )
{
	if ( m_pSegment == nullptr || nBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     nBytes > ( ( uint64_t )( m_nPageCount ) ) * m_nPageBytes || nAlignment > ( ( uint64_t )( m_nPageCount ) ) * m_nPageBytes ) {
		return nullptr;
	}
	if ( nBytes > UINT64_MAX - m_nPageBytes + 1 ) {
		return nullptr;
	}
	const uint64_t nPages64 = ( nBytes + m_nPageBytes - 1 ) / m_nPageBytes;
	if ( nPages64 > UINT32_MAX ) {
		return nullptr;
	}
	const uint32_t nPages = ( ( uint32_t )( nPages64 ) );
	const uint64_t nAlignPages = nAlignment <= m_nPageBytes ? 1 : nAlignment / m_nPageBytes;
	uint32_t nWinner = SIR_NONE;
	uint32_t nWinnerStart = 0;
	uint64_t nWaste = UINT64_MAX;
	for ( uint32_t run = m_nFreeHead; run != SIR_NONE; run = m_pFreeRuns[run].nNext ) {
		const uint64_t nBasePage = ( ( uint64_t )( ( uintptr_t )( m_pBase ) ) ) / m_nPageBytes;
		const uint64_t nAligned = ( ( nBasePage + run + nAlignPages - 1 ) & ~( nAlignPages - 1 ) ) - nBasePage;
		const uint64_t nEnd = ( ( uint64_t )( run ) ) + m_pFreeRuns[run].nPages;
		if ( nAligned > nEnd || nPages > nEnd - nAligned ) {
			continue;
		}
		const uint64_t nSlack = nEnd - nAligned - nPages;
		if ( nSlack < nWaste ) {
			nWinner = run;
			nWinnerStart = ( ( uint32_t )( nAligned ) );
			nWaste = nSlack;
			if ( nSlack == 0 ) {
				break;
			}
		}
	}
	if ( nWinner == SIR_NONE ||
	     !m_pSegment->commit( ( ( uint64_t )( nWinnerStart ) ) * m_nPageBytes, ( ( uint64_t )( nPages ) ) * m_nPageBytes ) ) {
		return nullptr;
	}
	const uint32_t nEnd = nWinner + m_pFreeRuns[nWinner].nPages;
	unlinkRun( nWinner );
	if ( nWinner < nWinnerStart ) {
		insertRun( nWinner, nWinnerStart - nWinner );
	}
	if ( nWinnerStart + nPages < nEnd ) {
		insertRun( nWinnerStart + nPages, nEnd - nWinnerStart - nPages );
	}
	m_pAllocationPages[nWinnerStart] = nPages;
	++m_nAllocationCount;
	m_nAllocatedBytes += ( ( uint64_t )( nPages ) ) * m_nPageBytes;
	return m_pBase + ( ( uint64_t )( nWinnerStart ) ) * m_nPageBytes;
}

uint64_t SIRPageRunAllocator::getUsableSize( const void *pMemory ) const
{
	if ( m_pSegment == nullptr || pMemory == nullptr ) {
		return 0;
	}
	const uintptr_t nAddress = ( ( uintptr_t )( pMemory ) );
	const uintptr_t nBase = ( ( uintptr_t )( m_pBase ) );
	if ( nAddress < nBase || nAddress - nBase >= m_pSegment->getReservedBytes() || ( nAddress - nBase ) % m_nPageBytes != 0 ) {
		return 0;
	}
	const uint32_t nFirst = ( ( uint32_t )( ( nAddress - nBase ) / m_nPageBytes ) );
	return ( ( uint64_t )( m_pAllocationPages[nFirst] ) ) * m_nPageBytes;
}

bool SIRPageRunAllocator::free( void *pMemory )
{
	const uint64_t nBytes = getUsableSize( pMemory );
	if ( nBytes == 0 ) {
		return false;
	}
	const uint32_t nFirst = ( ( uint32_t )( ( ( uintptr_t )( pMemory ) - ( ( uintptr_t )( m_pBase ) ) ) / m_nPageBytes ) );
	m_pAllocationPages[nFirst] = 0;
	const uint32_t nPages = ( ( uint32_t )( nBytes / m_nPageBytes ) );
	insertRun( nFirst, nPages );
	--m_nAllocationCount;
	m_nAllocatedBytes -= nBytes;
	return true;
}

bool SIRPageRunAllocator::trimUnusedPages( void )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}
	for ( uint32_t run = m_nFreeHead; run != SIR_NONE; run = m_pFreeRuns[run].nNext ) {
		if ( !m_pSegment->decommit( ( ( uint64_t )( run ) ) * m_nPageBytes, ( ( uint64_t )( m_pFreeRuns[run].nPages ) ) * m_nPageBytes ) ) {
			return false;
		}
	}
	return true;
}

uint64_t SIRPageRunAllocator::getAllocatedBytes( void ) const
{
	return m_nAllocatedBytes;
}

uint32_t SIRPageRunAllocator::getAllocationCount( void ) const
{
	return m_nAllocationCount;
}
