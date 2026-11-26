/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Strings/SIRArenaString.h>
#include <Core/Strings/SIRStringOps.h>

SIRStringArena::SIRStringArena( char *pStorage, uint32_t nCapacity ) noexcept
    : m_pStorage( pStorage ),
      m_nCapacity( pStorage == nullptr ? 0 : nCapacity )
{
}

uint32_t SIRStringArena::getUsedBytes( void ) const noexcept
{
	return m_nUsed;
}

SIRContainerResult SIRStringArena::trySave( sirStringView_t view, SIRArenaString& output ) noexcept
{
	if ( m_pStorage == nullptr || !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes >= m_nCapacity - m_nUsed ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	char *pszDestination = m_pStorage + m_nUsed;
	const SIRContainerResult eResult = SIRStringOps::copyTerminated( view, pszDestination, m_nCapacity - m_nUsed );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	output.m_View = { pszDestination, view.nBytes };
	m_nUsed += view.nBytes + 1;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRStringArena::reset( void ) noexcept
{
	m_nUsed = 0;
}

SIRContainerResult SIRStringArena::trySaveRelative( sirStringView_t view, SIROffsetString& output ) noexcept
{
	const uint32_t iOffset = m_nUsed;
	SIRArenaString saved;
	const SIRContainerResult eResult = trySave( view, saved );
	if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
		output = SIROffsetString( iOffset, view.nBytes );
	}
	return eResult;
}

sirStringView_t SIRStringArena::getTable( void ) const noexcept
{
	return { m_pStorage, m_nUsed };
}

SIRContainerResult SIRStringArena::tryGenerate( uint32_t nBytes, void ( *pfnWrite )( char *, uint32_t ) noexcept, SIRArenaString& output ) noexcept
{
	if ( m_pStorage == nullptr || pfnWrite == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes >= m_nCapacity - m_nUsed ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	char *pDestination = m_pStorage + m_nUsed;
	pfnWrite( pDestination, nBytes );
	pDestination[nBytes] = '\0';
	output.m_View = { pDestination, nBytes };
	m_nUsed += nBytes + 1;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRStringArenaSlabSource::SIRStringArenaSlabSource( char *pStorage, uint32_t nCapacity ) noexcept
    : m_pStorage( pStorage ),
      m_nCapacity( pStorage == nullptr ? 0 : nCapacity )
{
}

SIRContainerResult SIRStringArenaSlabSource::tryAcquire( uint32_t nBytes, sirStringArenaSlab_t& slab ) noexcept
{
	if ( nBytes == 0 || m_pStorage == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t nUsed = m_nUsed.load( std::memory_order_relaxed );
	do {
		if ( nBytes > m_nCapacity - nUsed ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
	} while ( !m_nUsed.compare_exchange_weak( nUsed, nUsed + nBytes, std::memory_order_relaxed, std::memory_order_relaxed ) );
	slab = { m_pStorage + nUsed, nBytes };
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRStringArenaSlabSource::reset( void ) noexcept
{
	m_nUsed.store( 0, std::memory_order_relaxed );
}
