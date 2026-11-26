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

#include <Core/Strings/SIRVirtualStringArena.h>
#include <Core/Strings/SIRStringOps.h>

#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>

SIRVirtualStringArena::~SIRVirtualStringArena() noexcept
{
	if ( tryRelease() != SIRContainerResult::SIR_CONTAINER_OK ) {
		abort();
	}
}

uint32_t SIRVirtualStringArena::getUsedBytes( void ) const noexcept
{
	return m_nUsed;
}

size_t SIRVirtualStringArena::getCommittedBytes( void ) const noexcept
{
	return m_nCommitted;
}

size_t SIRVirtualStringArena::getReservedBytes( void ) const noexcept
{
	return m_nReserved;
}

SIRContainerResult SIRVirtualStringArena::tryInitialize( uint32_t nCapacity ) noexcept
{
	if ( m_pBytes != nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_BUSY;
	}
	const size_t nPage = SIRVirtualMemory::getPageSize();
	const size_t nGranularity = SIRVirtualMemory::getAllocationGranularity();
	if ( nPage == 0 || nGranularity == 0 || nCapacity == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const size_t nUnit = nGranularity > nPage ? nGranularity : nPage;
	if ( static_cast<size_t>( nCapacity ) > SIZE_MAX - ( nUnit - 1 ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const size_t nReserve = ( static_cast<size_t>( nCapacity ) + nUnit - 1 ) / nUnit * nUnit;
	void *pBytes = SIRVirtualMemory::reserve( nReserve );
	if ( pBytes == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_OUT_OF_MEMORY;
	}
	m_pBytes = static_cast<char *>( pBytes );
	m_nReserved = nReserve;
	m_nCapacity = nCapacity;
	m_nPage = nPage;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRVirtualStringArena::trySave( sirStringView_t input, sirStringView_t& output ) noexcept
{
	if ( m_pBytes == nullptr || !SIRStringOps::isValid( input ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( input.nBytes >= m_nCapacity - m_nUsed ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const uint32_t nEnd = m_nUsed + input.nBytes + 1;
	const size_t nRequired = ( static_cast<size_t>( nEnd ) - 1 ) / m_nPage * m_nPage + m_nPage;
	if ( nRequired > m_nCommitted ) {
		if ( !SIRVirtualMemory::commit( m_pBytes + m_nCommitted, nRequired - m_nCommitted ) ) {
			return SIRContainerResult::SIR_CONTAINER_OUT_OF_MEMORY;
		}
		m_nCommitted = nRequired;
	}
	char *pDestination = m_pBytes + m_nUsed;
	if ( input.nBytes != 0 ) {
		memmove( pDestination, input.pBytes, input.nBytes );
	}
	pDestination[input.nBytes] = '\0';
	output = { pDestination, input.nBytes };
	m_nUsed = nEnd;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRVirtualStringArena::tryReset( bool bDecommit ) noexcept
{
	if ( bDecommit && m_nCommitted != 0 ) {
		if ( !SIRVirtualMemory::decommit( m_pBytes, m_nCommitted ) ) {
			return SIRContainerResult::SIR_CONTAINER_BUSY;
		}
		m_nCommitted = 0;
	}
	m_nUsed = 0;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRVirtualStringArena::tryRelease( void ) noexcept
{
	if ( m_pBytes != nullptr && !SIRVirtualMemory::release( m_pBytes, m_nReserved ) ) {
		return SIRContainerResult::SIR_CONTAINER_BUSY;
	}
	m_pBytes = nullptr;
	m_nUsed = 0;
	m_nCapacity = 0;
	m_nReserved = 0;
	m_nCommitted = 0;
	m_nPage = 0;
	return SIRContainerResult::SIR_CONTAINER_OK;
}