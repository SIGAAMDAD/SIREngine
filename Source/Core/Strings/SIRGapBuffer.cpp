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

#include <Core/Strings/SIRGapBuffer.h>
#include <Core/Strings/SIRStringOps.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>

SIRGapBuffer::SIRGapBuffer() noexcept
    : SIRGapBuffer( nullptr, 0 )
{
}

SIRGapBuffer::SIRGapBuffer( char *pStorage, uint32_t nCapacity ) noexcept
    : m_pStorage( pStorage ),
      m_nCapacity( pStorage == nullptr ? 0 : nCapacity ),
      m_iGapEnd( m_nCapacity )
{
}

uint32_t SIRGapBuffer::getGapSize( void ) const noexcept
{
	return m_iGapEnd - m_iGapBegin;
}

uint32_t SIRGapBuffer::getByteLength( void ) const noexcept
{
	return m_nCapacity - getGapSize();
}

SIRContainerResult SIRGapBuffer::tryGetByte( uint32_t iByte, char& nByte ) const noexcept
{
	if ( iByte >= getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	nByte = m_pStorage[iByte < m_iGapBegin ? iByte : iByte + getGapSize()];
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRGapBuffer::moveGap( uint32_t iPosition ) noexcept
{
	if ( iPosition < m_iGapBegin ) {
		const uint32_t nMove = m_iGapBegin - iPosition;
		moveBytes( m_pStorage + m_iGapEnd - nMove, m_pStorage + iPosition, nMove );
		m_iGapEnd -= nMove;
		m_iGapBegin = iPosition;
	} else if ( iPosition > m_iGapBegin ) {
		const uint32_t nMove = iPosition - m_iGapBegin;
		moveBytes( m_pStorage + m_iGapBegin, m_pStorage + m_iGapEnd, nMove );
		m_iGapBegin += nMove;
		m_iGapEnd += nMove;
	}
}

SIRContainerResult SIRGapBuffer::tryInsert( uint32_t iPosition, sirStringView_t view ) noexcept
{
	if ( iPosition > getByteLength() || !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes > getGapSize() ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( view.nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( SIRStringOps::overlaps( view, { m_pStorage, m_nCapacity } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	moveGap( iPosition );
	memcpy( m_pStorage + m_iGapBegin, view.pBytes, view.nBytes );
	m_iGapBegin += view.nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRGapBuffer::tryErase( uint32_t iPosition, uint32_t nBytes ) noexcept
{
	if ( iPosition > getByteLength() ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes > getByteLength() - iPosition ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	const uint32_t iEnd = iPosition + nBytes;
	if ( iPosition <= m_iGapBegin && iEnd >= m_iGapBegin ) {
		m_iGapEnd += iEnd - m_iGapBegin;
		m_iGapBegin = iPosition;
	} else if ( iEnd < m_iGapBegin ) {
		moveGap( iEnd );
		m_iGapBegin -= nBytes;
	} else {
		moveGap( iPosition );
		m_iGapEnd += nBytes;
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRGapBuffer::tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t nLength = getByteLength();
	if ( nCapacity <= nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( SIRStringOps::overlaps( { pszOutput, nLength + 1 }, { m_pStorage, m_nCapacity } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_iGapBegin != 0 ) {
		memcpy( pszOutput, m_pStorage, m_iGapBegin );
	}
	if ( m_iGapEnd != m_nCapacity ) {
		memcpy( pszOutput + m_iGapBegin, m_pStorage + m_iGapEnd, m_nCapacity - m_iGapEnd );
	}
	pszOutput[nLength] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRGapBuffer::clear( void ) noexcept
{
	m_iGapBegin = 0;
	m_iGapEnd = m_nCapacity;
}

sirStringView_t SIRGapBuffer::getPrefix( void ) const noexcept
{
	return { m_pStorage, m_iGapBegin };
}

sirStringView_t SIRGapBuffer::getSuffix( void ) const noexcept
{
	return { m_pStorage == nullptr ? nullptr : m_pStorage + m_iGapEnd, m_nCapacity - m_iGapEnd };
}


SIRGapBuffer::~SIRGapBuffer() noexcept
{
	if ( m_nMappedBytes != 0 ) {
		const bool bReleased = SIRVirtualMemory::release( m_pStorage, m_nMappedBytes );
		assert( bReleased );
		static_cast<void>( bReleased );
	}
}

SIRContainerResult SIRGapBuffer::tryInitializeVirtual( uint32_t nCapacity ) noexcept
{
	if ( m_pStorage != nullptr ) { return SIRContainerResult::SIR_CONTAINER_BUSY; }
	const size_t nPage = SIRVirtualMemory::getPageSize();
	if ( nCapacity == 0 || nPage == 0 || nCapacity > SIZE_MAX - ( nPage - 1 ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const size_t nMapped = ( ( static_cast<size_t>( nCapacity ) + nPage - 1 ) / nPage ) * nPage;
	char *pMemory = static_cast<char *>( SIRVirtualMemory::allocatePages( nMapped ) );
	if ( pMemory == nullptr ) { return SIRContainerResult::SIR_CONTAINER_OUT_OF_MEMORY; }
	m_pStorage = pMemory;
	m_nMappedBytes = nMapped;
	m_nCapacity = nCapacity;
	clear();
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRGapBuffer::moveBytes( char *pDestination, char *pSource, uint32_t nBytes ) noexcept
{
	// Remap only disjoint page-aligned interior ranges in our own anonymous
	// reservation. A source range becomes part of the new gap after movement.
	if ( m_nMappedBytes != 0 && nBytes <= getGapSize() ) {
		const size_t nPage = SIRVirtualMemory::getPageSize();
		if ( nPage != 0 && nBytes / nPage >= 2 && getGapSize() % nPage == 0 ) {
			const size_t nOffset = reinterpret_cast<uintptr_t>( pSource ) % nPage;
			const size_t nPrefix = nOffset == 0 ? 0 : nPage - nOffset;
			const size_t nInterior = ( ( nBytes - nPrefix ) / nPage ) * nPage;
			if ( nInterior != 0 && SIRVirtualMemory::tryMovePages( pDestination + nPrefix, pSource + nPrefix, nInterior ) ) {
				memmove( pDestination, pSource, nPrefix );
				const size_t iTail = nPrefix + nInterior;
				memmove( pDestination + iTail, pSource + iTail, nBytes - iTail );
				return;
			}
		}
	}
	memmove( pDestination, pSource, nBytes );
}
