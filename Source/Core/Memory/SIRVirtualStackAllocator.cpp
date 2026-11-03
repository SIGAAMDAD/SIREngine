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

#include <Core/Memory/Allocators/SIRVirtualStackAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>

#include <cassert>
#include <limits>

SIRStackBookmark::SIRStackBookmark( SIRVirtualStackAllocator *pOwner, uint64_t nMark, uint32_t nDepth )
    : m_pOwner( pOwner ), m_nMark( nMark ), m_nDepth( nDepth )
{
}

SIRStackBookmark::~SIRStackBookmark( void )
{
	if ( m_pOwner != nullptr ) {
		m_pOwner->popBookmark( m_nMark, m_nDepth );
	}
}

SIRVirtualStackAllocator::SIRVirtualStackAllocator( void )
    : m_nRecentHighWaterMark( 0 ), m_nBookmarkDepth( 0 ), m_eMode( SIR_STACK_DECOMMIT_ON_SHUTDOWN )
{
}

bool SIRVirtualStackAllocator::init( SIRMemorySegment *pSegment, sirStackDecommitMode_e eMode, uint64_t nCommitIncrementBytes )
{
	if ( pSegment == nullptr || m_Linear.getSegment() != nullptr || eMode > SIR_STACK_DECOMMIT_EXCESS_ON_EMPTY ) {
		return false;
	}

	const uint64_t nPageSize = pSegment->getPageSize();
	if ( nPageSize == 0 || pSegment->getReservedBytes() < 2 * nPageSize ) {
		return false;
	}

	const uint64_t nGuardOffset = pSegment->getReservedBytes() - nPageSize;
	if ( !pSegment->decommit( nGuardOffset, nPageSize ) || !m_Linear.init( pSegment, nCommitIncrementBytes, nGuardOffset ) ) {
		return false;
	}

	m_eMode = eMode;
	return true;
}

void *SIRVirtualStackAllocator::alloc( uint64_t nBytes, uint64_t nAlignment )
{
	void *pMemory = m_Linear.alloc( nBytes, nAlignment );
	if ( pMemory != nullptr && nBytes != 0 ) {
		const uint64_t nCurrentUsed = m_Linear.getUsedBytes();
		if ( nCurrentUsed > m_nRecentHighWaterMark ) {
			m_nRecentHighWaterMark = nCurrentUsed;
		}
	}
	return pMemory;
}

bool SIRVirtualStackAllocator::precommit( uint64_t nBytes )
{
	return m_Linear.precommit( nBytes );
}

SIRStackBookmark SIRVirtualStackAllocator::createScopedBookmark( void )
{
	assert( m_Linear.getSegment() != nullptr );
	assert( m_nBookmarkDepth != ( std::numeric_limits<uint32_t>::max )() );
	++m_nBookmarkDepth;
	return SIRStackBookmark( this, m_Linear.getMark(), m_nBookmarkDepth );
}

void SIRVirtualStackAllocator::popBookmark( uint64_t nMark, uint32_t nDepth )
{
	assert( nDepth == m_nBookmarkDepth );
	assert( nMark <= m_Linear.getUsedBytes() );
	if ( nDepth != m_nBookmarkDepth || !m_Linear.rewind( nMark ) ) {
		return;
	}

	--m_nBookmarkDepth;
	if ( m_Linear.getUsedBytes() == 0 ) {
		const bool bReclaimed = onStackEmpty();
		( void )bReclaimed;
		assert( bReclaimed );
	}
}

bool SIRVirtualStackAllocator::reset( void )
{
	if ( m_Linear.getSegment() == nullptr || m_nBookmarkDepth != 0 || !m_Linear.rewind( 0 ) ) {
		return false;
	}
	return onStackEmpty();
}

bool SIRVirtualStackAllocator::shutdown( void )
{
	if ( m_nBookmarkDepth != 0 || m_Linear.getUsedBytes() != 0 ) {
		return false;
	}
	if ( !m_Linear.shutdown() ) {
		return false;
	}
	m_nRecentHighWaterMark = 0;
	m_eMode = SIR_STACK_DECOMMIT_ON_SHUTDOWN;
	return true;
}

bool SIRVirtualStackAllocator::onStackEmpty( void )
{
	const uint64_t nCommitted = m_Linear.getCommittedPrefixBytes();
	if ( m_eMode == SIR_STACK_DECOMMIT_ON_EMPTY ) {
		if ( !m_Linear.trimTo( 0 ) ) {
			return false;
		}
	} else if ( m_eMode == SIR_STACK_DECOMMIT_EXCESS_ON_EMPTY ) {
		// Align once on the cold reclamation path, not on every allocation.
		uint64_t nKeep = 0;
		if ( !SIR_IS_POWER_OF_TWO( m_Linear.getSegment()->getPageSize() ) ||
		     ( uint64_t )( m_Linear.getSegment()->getPageSize() ) > UINTPTR_MAX ||
		     ( uint64_t )( m_nRecentHighWaterMark ) > UINTPTR_MAX - ( ( uint64_t )( m_Linear.getSegment()->getPageSize() ) - 1 ) ) {
			return false;
		}
		nKeep = SIR_ALIGN_UP( m_nRecentHighWaterMark, m_Linear.getSegment()->getPageSize() );
		const uint64_t nUnused = nCommitted > nKeep ? nCommitted - nKeep : 0;
		if ( nUnused > nCommitted / 4 && !m_Linear.trimTo( nKeep ) ) {
			return false;
		}
	}

	m_nRecentHighWaterMark = 0;
	return true;
}

uint64_t SIRVirtualStackAllocator::getUsedBytes( void ) const
{
	return m_Linear.getUsedBytes();
}

uint64_t SIRVirtualStackAllocator::getCommittedBytes( void ) const
{
	return m_Linear.getCommittedPrefixBytes();
}

uint64_t SIRVirtualStackAllocator::getCapacity( void ) const
{
	return m_Linear.getCapacity();
}
