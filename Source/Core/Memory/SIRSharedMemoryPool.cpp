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

#include <Core/Memory/SIRSharedMemoryPool.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>

SIRSharedMemoryPool::SIRSharedMemoryPool( void ) : m_pSegment( nullptr ), m_nCursor( 0 )
{
}

bool SIRSharedMemoryPool::init( SIRMemorySegment *pSegment )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || pSegment->getBase() == nullptr || pSegment->getCommittedBytes() != 0 ||
	     !SIR_IS_POWER_OF_TWO( pSegment->getPageSize() ) ) {
		return false;
	}
	m_pSegment = pSegment;
	return true;
}

bool SIRSharedMemoryPool::allocateRegion( uint64_t nBytes, sirSharedMemoryRegion_t& region )
{
	if ( m_pSegment == nullptr || nBytes == 0 ) {
		return false;
	}
	const uint64_t nPage = m_pSegment->getPageSize();
	if ( nBytes > UINT64_MAX - ( nPage - 1 ) ) {
		return false;
	}
	const uint64_t nAligned = SIR_ALIGN_UP( nBytes, nPage );
	if ( nAligned > getAvailableBytes() ) {
		return false;
	}
	region = { m_pSegment, m_nCursor, nAligned };
	m_nCursor += nAligned;
	return true;
}

bool SIRSharedMemoryPool::releaseLastRegion( const sirSharedMemoryRegion_t& region )
{
	if ( region.pSegment != m_pSegment || region.nReservedBytes == 0 || region.nReservedBytes > m_nCursor ||
	     region.nOffsetBytes != m_nCursor - region.nReservedBytes ) {
		return false;
	}
	// Decommit only the carved pages. No live client may retain pointers.
	if ( region.pSegment->getCommittedBytes() != 0 && !region.pSegment->decommit( region.nOffsetBytes, region.nReservedBytes ) ) {
		return false;
	}
	m_nCursor = region.nOffsetBytes;
	return true;
}

uint64_t SIRSharedMemoryPool::getAvailableBytes( void ) const
{
	return m_pSegment == nullptr ? 0 : m_pSegment->getReservedBytes() - m_nCursor;
}

SIRMemorySegment *SIRSharedMemoryPool::getSegment( void ) const
{
	return m_pSegment;
}

void *SIRSharedMemoryPool::getBase( const sirSharedMemoryRegion_t& region )
{
	if ( region.pSegment == nullptr || region.nOffsetBytes > region.pSegment->getReservedBytes() ||
	     region.nReservedBytes > region.pSegment->getReservedBytes() - region.nOffsetBytes ) {
		return nullptr;
	}
	return static_cast<uint8_t *>( region.pSegment->getBase() ) + region.nOffsetBytes;
}

bool SIRSharedMemoryPool::commit( const sirSharedMemoryRegion_t& region, uint64_t nOffset, uint64_t nBytes )
{
	if ( getBase( region ) == nullptr || nOffset > region.nReservedBytes || nBytes > region.nReservedBytes - nOffset ) {
		return false;
	}
	return nBytes == 0 || region.pSegment->commit( region.nOffsetBytes + nOffset, nBytes );
}

bool SIRSharedMemoryPool::decommit( const sirSharedMemoryRegion_t& region, uint64_t nOffset, uint64_t nBytes )
{
	if ( getBase( region ) == nullptr || nOffset > region.nReservedBytes || nBytes > region.nReservedBytes - nOffset ) {
		return false;
	}
	return nBytes == 0 || region.pSegment->decommit( region.nOffsetBytes + nOffset, nBytes );
}
