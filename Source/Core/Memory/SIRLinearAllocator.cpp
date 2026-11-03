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

#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>

#include <cstdint>
#include <limits>

SIRLinearAllocator::SIRLinearAllocator( void )
    : m_pSegment( nullptr ), m_nUsedBytes( 0 ), m_nCommittedPrefixBytes( 0 ), m_nCapacity( 0 ), m_nCommitIncrementBytes( 0 )
{
}

bool SIRLinearAllocator::init( SIRMemorySegment *pSegment, uint64_t nCommitIncrementBytes, uint64_t nUsableBytes )
{
	if ( pSegment == nullptr || m_pSegment != nullptr ) {
		return false;
	}

	const uint64_t nPageSize = pSegment->getPageSize();
	const uint64_t nCapacity = nUsableBytes != 0 ? nUsableBytes : pSegment->getReservedBytes();
	if ( nPageSize == 0 || nCapacity == 0 || nCapacity > pSegment->getReservedBytes() || nCapacity % nPageSize != 0 ) {
		return false;
	}

	uint64_t nIncrement = 0;
	if ( !SIR_IS_POWER_OF_TWO( nPageSize ) || ( uint64_t )( nPageSize ) > UINTPTR_MAX ||
	     ( uint64_t )( nCommitIncrementBytes != 0 ? nCommitIncrementBytes : nPageSize ) > UINTPTR_MAX - ( ( uint64_t )( nPageSize )-1 ) ) {
		return false;
	}
	nIncrement = SIR_ALIGN_UP( nCommitIncrementBytes != 0 ? nCommitIncrementBytes : nPageSize, nPageSize );

	m_pSegment = pSegment;
	m_nCapacity = nCapacity;
	m_nCommitIncrementBytes = nIncrement;
	return true;
}

void *SIRLinearAllocator::alloc( uint64_t nBytes, uint64_t nAlignment )
{
	if ( m_pSegment == nullptr || nBytes == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) ) {
		return nullptr;
	}

	const uintptr_t nBase = ( ( uintptr_t )( m_pSegment->getBase() ) );
	if ( m_nUsedBytes > ( std::numeric_limits<uintptr_t>::max )() - nBase ) {
		return nullptr;
	}

	uint64_t nAlignedAddress = 0;
	if ( !SIR_IS_POWER_OF_TWO( nAlignment ) || ( uint64_t )( nAlignment ) > UINTPTR_MAX ||
	     ( uint64_t )( nBase + m_nUsedBytes ) > UINTPTR_MAX - ( ( uint64_t )( nAlignment )-1 ) ) {
		return nullptr;
	}
	nAlignedAddress = SIR_ALIGN_UP( nBase + m_nUsedBytes, nAlignment );
	if ( nAlignedAddress < nBase ) {
		return nullptr;
	}

	const uint64_t nAlignedOffset = nAlignedAddress - nBase;
	if ( nAlignedOffset > m_nCapacity || nBytes > m_nCapacity - nAlignedOffset ) {
		return nullptr;
	}

	const uint64_t nNewUsed = nAlignedOffset + nBytes;
	if ( nNewUsed > m_nCommittedPrefixBytes && !precommit( nNewUsed ) ) {
		return nullptr;
	}

	m_nUsedBytes = nNewUsed;
	return ( ( void * )( ( ( uintptr_t )( nAlignedAddress ) ) ) );
}

bool SIRLinearAllocator::precommit( uint64_t nBytes )
{
	if ( m_pSegment == nullptr || nBytes > m_nCapacity ) {
		return false;
	}
	if ( nBytes <= m_nCommittedPrefixBytes ) {
		return true;
	}

	uint64_t nTarget = 0;
	{
		const uint64_t nRoundValue = nBytes;
		const uint64_t nRoundMultiple = m_nCommitIncrementBytes;
		if ( nRoundMultiple == 0 ) {
			return false;
		}
		const uint64_t nRemainder = nRoundValue % nRoundMultiple;
		const uint64_t nPadding = nRemainder == 0 ? 0 : nRoundMultiple - nRemainder;
		if ( nRoundValue > UINT64_MAX - nPadding ) {
			return false;
		}
		nTarget = nRoundValue <= UINT64_MAX - ( nRoundMultiple - 1 ) ? SIR_ROUND_UP_MULTIPLE( nRoundValue, nRoundMultiple )
		                                                             : nRoundValue + nPadding;
	}
	if ( nTarget > m_nCapacity ) {
		if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) || ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
		     ( uint64_t )( nBytes ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) ) {
			return false;
		}
		nTarget = SIR_ALIGN_UP( nBytes, m_pSegment->getPageSize() );
		if ( nTarget > m_nCapacity ) {
			return false;
		}
	}

	if ( !m_pSegment->commit( m_nCommittedPrefixBytes, nTarget - m_nCommittedPrefixBytes ) ) {
		return false;
	}
	m_nCommittedPrefixBytes = nTarget;
	return true;
}

uint64_t SIRLinearAllocator::getMark( void ) const
{
	return m_nUsedBytes;
}

bool SIRLinearAllocator::rewind( uint64_t nMark )
{
	if ( m_pSegment == nullptr || nMark > m_nUsedBytes ) {
		return false;
	}
	m_nUsedBytes = nMark;
	return true;
}

bool SIRLinearAllocator::reset( bool bDecommit )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}

	if ( bDecommit && m_nCommittedPrefixBytes != 0 ) {
		// Do not invalidate the logical allocations if the OS refuses
		// to release the backing pages.
		if ( !m_pSegment->decommit( 0, m_nCommittedPrefixBytes ) ) {
			return false;
		}
		m_nCommittedPrefixBytes = 0;
	}

	m_nUsedBytes = 0;
	return true;
}

bool SIRLinearAllocator::shutdown( void )
{
	if ( m_pSegment == nullptr || m_nUsedBytes != 0 || !trimTo( 0 ) ) {
		return false;
	}

	m_pSegment = nullptr;
	m_nUsedBytes = 0;
	m_nCommittedPrefixBytes = 0;
	m_nCapacity = 0;
	m_nCommitIncrementBytes = 0;
	return true;
}

bool SIRLinearAllocator::trimTo( uint64_t nKeepBytes )
{
	if ( m_pSegment == nullptr || nKeepBytes < m_nUsedBytes || nKeepBytes > m_nCapacity ) {
		return false;
	}

	uint64_t nKeepAligned = 0;
	if ( !SIR_IS_POWER_OF_TWO( m_pSegment->getPageSize() ) || ( uint64_t )( m_pSegment->getPageSize() ) > UINTPTR_MAX ||
	     ( uint64_t )( nKeepBytes ) > UINTPTR_MAX - ( ( uint64_t )( m_pSegment->getPageSize() ) - 1 ) ) {
		return false;
	}
	nKeepAligned = SIR_ALIGN_UP( nKeepBytes, m_pSegment->getPageSize() );

	if ( nKeepAligned >= m_nCommittedPrefixBytes ) {
		return true;
	}

	const uint64_t nTrimBytes = m_nCommittedPrefixBytes - nKeepAligned;
	if ( !m_pSegment->decommit( nKeepAligned, nTrimBytes ) ) {
		return false;
	}

	m_nCommittedPrefixBytes = nKeepAligned;
	return true;
}

uint64_t SIRLinearAllocator::getUsedBytes( void ) const
{
	return m_nUsedBytes;
}

uint64_t SIRLinearAllocator::getCommittedPrefixBytes( void ) const
{
	return m_nCommittedPrefixBytes;
}

uint64_t SIRLinearAllocator::getCapacity( void ) const
{
	return m_nCapacity;
}

SIRMemorySegment *SIRLinearAllocator::getSegment( void ) const
{
	return m_pSegment;
}
