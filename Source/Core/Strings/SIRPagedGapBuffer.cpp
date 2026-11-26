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

#include <Core/Strings/SIRPagedGapBuffer.h>
#include <Core/Strings/SIRStringOps.h>

SIRPagedGapBuffer::SIRPagedGapBuffer( const sirPagedGapStorage_t& storage ) noexcept
    : m_Storage( storage )
{
	if ( storage.pBytes == nullptr || storage.pPages == nullptr || storage.nPageBytes == 0 ) {
		m_Storage.nPages = 0;
	}
	if ( static_cast<uint64_t>( storage.nPages ) * storage.nPageBytes > UINT32_MAX ) {
		m_Storage.nPages = 0;
	}
	clear();
}

uint32_t SIRPagedGapBuffer::getByteLength( void ) const noexcept
{
	return m_nLength;
}

uint32_t SIRPagedGapBuffer::getFreePages( void ) const noexcept
{
	return m_nFree;
}

char *SIRPagedGapBuffer::data( uint32_t iPage ) const noexcept
{
	return m_Storage.pBytes + static_cast<size_t>( iPage ) * m_Storage.nPageBytes;
}

uint32_t SIRPagedGapBuffer::length( uint32_t iPage ) const noexcept
{
	const sirPagedGapPage_t& page = m_Storage.pPages[iPage];
	return m_Storage.nPageBytes - ( page.iGapEnd - page.iGapBegin );
}

void SIRPagedGapBuffer::clear( void ) noexcept
{
	m_iHead = INVALID;
	m_iFree = m_Storage.nPages == 0 ? INVALID : 0;
	m_nFree = m_Storage.nPages;
	m_nLength = 0;
	m_nIndexed = 0;
	for ( uint32_t i = 0; i < m_nFree; ++i ) {
		m_Storage.pPages[i].iNext = i + 1 == m_nFree ? INVALID : i + 1;
	}
}

uint32_t SIRPagedGapBuffer::locate( uint32_t& iPosition, uint32_t& iPrevious, uint32_t *pRank ) const noexcept
{
	iPrevious = INVALID;
	if ( m_nIndexed != 0 ) {
		const uint32_t iRank = rankAt( iPosition );
		if ( pRank != nullptr ) { *pRank = iRank; }
		iPrevious = iRank == 0 ? INVALID : m_Storage.pOrder[iRank - 1];
		return m_Storage.pOrder[iRank];
	}
	uint32_t iPage = m_iHead;
	while ( iPage != INVALID ) {
		const uint32_t nBytes = length( iPage );
		if ( iPosition <= nBytes ) {
			break;
		}
		iPosition -= nBytes;
		iPrevious = iPage;
		iPage = m_Storage.pPages[iPage].iNext;
	}
	return iPage;
}

uint32_t SIRPagedGapBuffer::allocateAfter( uint32_t iPrevious ) noexcept
{
	const uint32_t iPage = m_iFree;
	sirPagedGapPage_t& page = m_Storage.pPages[iPage];
	m_iFree = page.iNext;
	--m_nFree;
	page.iGapBegin = 0;
	page.iGapEnd = m_Storage.nPageBytes;
	if ( iPrevious == INVALID ) {
		page.iNext = m_iHead;
		m_iHead = iPage;
	} else {
		page.iNext = m_Storage.pPages[iPrevious].iNext;
		m_Storage.pPages[iPrevious].iNext = iPage;
	}
	return iPage;
}

void SIRPagedGapBuffer::release( uint32_t iPage, uint32_t iPrevious ) noexcept
{
	sirPagedGapPage_t& page = m_Storage.pPages[iPage];
	if ( iPrevious == INVALID ) {
		m_iHead = page.iNext;
	} else {
		m_Storage.pPages[iPrevious].iNext = page.iNext;
	}
	page.iNext = m_iFree;
	m_iFree = iPage;
	++m_nFree;
}

void SIRPagedGapBuffer::moveGap( uint32_t iPage, uint32_t iPosition ) noexcept
{
	sirPagedGapPage_t& page = m_Storage.pPages[iPage];
	char *pBytes = data( iPage );
	if ( iPosition < page.iGapBegin ) {
		const uint32_t nMove = page.iGapBegin - iPosition;
		memmove( pBytes + page.iGapEnd - nMove, pBytes + iPosition, nMove );
		page.iGapEnd -= nMove;
	} else if ( iPosition > page.iGapBegin ) {
		const uint32_t nMove = iPosition - page.iGapBegin;
		memmove( pBytes + page.iGapBegin, pBytes + page.iGapEnd, nMove );
		page.iGapEnd += nMove;
	}
	page.iGapBegin = iPosition;
}

SIRContainerResult SIRPagedGapBuffer::tryInsert( uint32_t iPosition, sirStringView_t input ) noexcept
{
	if ( iPosition > m_nLength || !SIRStringOps::isValid( input ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( input.nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( m_Storage.nPages == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( SIRStringOps::overlaps( input, { m_Storage.pBytes, m_Storage.nPages * m_Storage.nPageBytes } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iPrevious = INVALID;
	uint32_t iRank = 0;
	uint32_t iPage = locate( iPosition, iPrevious, &iRank );
	const uint32_t nPageBytes = m_Storage.nPageBytes;
	const uint32_t nExisting = iPage == INVALID ? 0 : length( iPage );
	const uint64_t nCombined = static_cast<uint64_t>( nExisting ) + input.nBytes;
	const uint64_t nRequired = ( nCombined + nPageBytes - 1 ) / nPageBytes;
	const uint64_t nExtra = nRequired - ( iPage == INVALID ? 0 : 1 );
	if ( nExtra > m_nFree ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( iPage == INVALID ) {
		iPage = allocateAfter( iPrevious );
	}
	moveGap( iPage, iPosition );
	// Fast path leaves the gap at the active cursor.
	sirPagedGapPage_t& first = m_Storage.pPages[iPage];
	if ( input.nBytes <= first.iGapEnd - first.iGapBegin ) {
		memcpy( data( iPage ) + first.iGapBegin, input.pBytes, input.nBytes );
		first.iGapBegin += input.nBytes;
	} else {
		// Split into fresh pages. Preserve the old suffix in the last page first.
		// No existing page other than the edited page moves.
		const uint32_t nSuffix = nPageBytes - first.iGapEnd;
		uint32_t iLast = iPage;
		for ( uint64_t i = 0; i < nExtra; ++i ) {
			iLast = allocateAfter( iLast );
		}
		sirPagedGapPage_t& last = m_Storage.pPages[iLast];
		last.iGapEnd = nPageBytes - nSuffix;
		memcpy( data( iLast ) + last.iGapEnd, data( iPage ) + first.iGapEnd, nSuffix );
		first.iGapEnd = nPageBytes;
		uint32_t iInput = 0;
		while ( iInput < input.nBytes ) {
			sirPagedGapPage_t& page = m_Storage.pPages[iPage];
			const uint32_t nGap = page.iGapEnd - page.iGapBegin;
			const uint32_t nWrite = nGap < input.nBytes - iInput ? nGap : input.nBytes - iInput;
			memcpy( data( iPage ) + page.iGapBegin, input.pBytes + iInput, nWrite );
			page.iGapBegin += nWrite;
			iInput += nWrite;
			iPage = page.iNext;
		}
	}
	m_nLength += input.nBytes;
	if ( nExtra != 0 || m_nIndexed == 0 ) {
		rebuildIndex();
	} else {
		updateIndex( iRank, input.nBytes, 0 );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRPagedGapBuffer::tryErase( uint32_t iPosition, uint32_t nBytes ) noexcept
{
	if ( iPosition > m_nLength || nBytes > m_nLength - iPosition ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	uint32_t iPrevious = INVALID;
	uint32_t iRank = 0;
	uint32_t iPage = locate( iPosition, iPrevious, &iRank );
	bool bTopologyChanged = false;
	m_nLength -= nBytes;
	while ( nBytes != 0 ) {
		sirPagedGapPage_t& page = m_Storage.pPages[iPage];
		const uint32_t nAvailable = length( iPage ) - iPosition;
		const uint32_t nErase = nBytes < nAvailable ? nBytes : nAvailable;
		moveGap( iPage, iPosition );
		page.iGapEnd += nErase;
		nBytes -= nErase;
		const uint32_t iNext = page.iNext;
		if ( length( iPage ) == 0 ) {
			release( iPage, iPrevious );
			bTopologyChanged = true;
		} else {
			iPrevious = iPage;
		}
		if ( m_nIndexed != 0 && !bTopologyChanged ) {
			updateIndex( iRank, 0, nErase );
		}
		++iRank;
		iPage = iNext;
		iPosition = 0;
	}
	if ( bTopologyChanged ) { rebuildIndex(); }
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRPagedGapBuffer::tryGetByte( uint32_t iPosition, char& nByte ) const noexcept
{
	if ( iPosition >= m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iPrevious = INVALID;
	uint32_t iPage = locate( iPosition, iPrevious );
	if ( iPosition == length( iPage ) ) {
		iPage = m_Storage.pPages[iPage].iNext;
		iPosition = 0;
	}
	const sirPagedGapPage_t& page = m_Storage.pPages[iPage];
	nByte = data( iPage )[iPosition < page.iGapBegin ? iPosition : iPosition + page.iGapEnd - page.iGapBegin];
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRPagedGapBuffer::tryCopyTo( char *pOutput, uint32_t nCapacity ) const noexcept
{
	if ( pOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nCapacity <= m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( SIRStringOps::overlaps( { pOutput, m_nLength + 1 }, { m_Storage.pBytes, m_Storage.nPages * m_Storage.nPageBytes } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	uint32_t iOutput = 0;
	for ( uint32_t iPage = m_iHead; iPage != INVALID; iPage = m_Storage.pPages[iPage].iNext ) {
		const sirPagedGapPage_t& page = m_Storage.pPages[iPage];
		memcpy( pOutput + iOutput, data( iPage ), page.iGapBegin );
		iOutput += page.iGapBegin;
		const uint32_t nSuffix = m_Storage.nPageBytes - page.iGapEnd;
		memcpy( pOutput + iOutput, data( iPage ) + page.iGapEnd, nSuffix );
		iOutput += nSuffix;
	}
	pOutput[iOutput] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRPagedGapBuffer::rebuildIndex( void ) noexcept
{
	m_nIndexed = 0;
	if ( m_Storage.pOrder == nullptr || m_Storage.pWeights == nullptr ) {
		return;
	}
	for ( uint32_t iPage = m_iHead; iPage != INVALID; iPage = m_Storage.pPages[iPage].iNext ) {
		m_Storage.pOrder[m_nIndexed] = iPage;
		m_Storage.pWeights[m_nIndexed++] = length( iPage );
	}
	for ( uint64_t i = 1; i <= m_nIndexed; ++i ) {
		const uint64_t iParent = i + ( i & ( 0ULL - i ) );
		if ( iParent <= m_nIndexed ) {
			m_Storage.pWeights[iParent - 1] += m_Storage.pWeights[i - 1];
		}
	}
}

void SIRPagedGapBuffer::updateIndex( uint32_t iRank, uint32_t nAdded, uint32_t nRemoved ) noexcept
{
	for ( uint64_t i = static_cast<uint64_t>( iRank ) + 1; i <= m_nIndexed; i += i & ( 0ULL - i ) ) {
		m_Storage.pWeights[i - 1] -= nRemoved;
		m_Storage.pWeights[i - 1] += nAdded;
	}
}

uint32_t SIRPagedGapBuffer::rankAt( uint32_t& iPosition ) const noexcept
{
	// Lower bound on cumulative ends; an exact boundary belongs to the preceding page.
	uint32_t nStep = 1;
	while ( nStep <= m_nIndexed / 2 ) {
		nStep <<= 1;
	}
	uint32_t iRank = 0;
	for ( ; nStep != 0; nStep >>= 1 ) {
		if ( nStep <= m_nIndexed - iRank ) {
			const uint32_t iCandidate = iRank + nStep;
			const uint32_t nBytes = m_Storage.pWeights[iCandidate - 1];
			if ( nBytes < iPosition ) {
				iRank = iCandidate;
				iPosition -= nBytes;
			}
		}
	}
	return iRank;
}
