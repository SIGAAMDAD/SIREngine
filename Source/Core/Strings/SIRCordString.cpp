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

#include <Core/Strings/SIRCordString.h>
#include <Core/Strings/SIRStringOps.h>

SIRCordString::SIRCordString() noexcept
    : m_pSlots( m_Inline ),
      m_nCapacity( 2 )
{
}

SIRCordString::SIRCordString( SIRSharedString *pSlots, uint32_t nCapacity ) noexcept
    : m_pSlots( pSlots ),
      m_nCapacity( pSlots == nullptr || nCapacity > UINT32_MAX / sizeof( SIRSharedString ) ? 0 : nCapacity )
{
}

SIRCordString::~SIRCordString() noexcept
{
	clear();
}

uint32_t SIRCordString::getByteLength( void ) const noexcept
{
	return m_nLength;
}

uint32_t SIRCordString::getChunkCount( void ) const noexcept
{
	return m_nCount;
}

uint32_t SIRCordString::indexOf( uint32_t iChunk ) const noexcept
{
	// At most one wrap, without unsigned overflow or integer division.
	const uint32_t nUntilWrap = m_nCapacity - m_iHead;
	return iChunk < nUntilWrap ? m_iHead + iChunk : iChunk - nUntilWrap;
}

SIRContainerResult SIRCordString::tryGetChunk( uint32_t iChunk, sirStringView_t& view ) const noexcept
{
	if ( iChunk >= m_nCount ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	view = m_pSlots[indexOf( iChunk )].getView();
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCordString::tryAppend( const SIRSharedString& chunk ) noexcept
{
	return tryInsert( chunk, false );
}

SIRContainerResult SIRCordString::tryPrepend( const SIRSharedString& chunk ) noexcept
{
	return tryInsert( chunk, true );
}

SIRContainerResult SIRCordString::tryInsert( const SIRSharedString& chunk, bool bPrepend ) noexcept
{
	const sirStringView_t chunkView = chunk.getView();
	const uint32_t nBytes = chunkView.nBytes;
	if ( m_pIndex != nullptr &&
	     SIRStringOps::overlaps( chunkView, { reinterpret_cast<const char *>( m_pIndex ), m_nCapacity * static_cast<uint32_t>( sizeof( uint32_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( m_nCount == m_nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( nBytes > UINT32_MAX - m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const uint32_t iSlot = bPrepend ? ( m_iHead == 0 ? m_nCapacity - 1 : m_iHead - 1 ) : indexOf( m_nCount );
	const SIRContainerResult eResult = m_pSlots[iSlot].tryShare( chunk );
	if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
		return eResult;
	}
	if ( bPrepend ) {
		m_iHead = iSlot;
	}
	updateIndex( iSlot, nBytes, false );
	++m_nCount;
	m_nLength += nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCordString::tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nCapacity <= m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const sirStringView_t output{ pszOutput, m_nLength + 1 };
	if ( SIRStringOps::overlaps( output, { reinterpret_cast<const char *>( m_pSlots ), static_cast<uint32_t>( m_nCapacity * sizeof( SIRSharedString ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_pIndex != nullptr &&
	     SIRStringOps::overlaps( output, { reinterpret_cast<const char *>( m_pIndex ), m_nCapacity * static_cast<uint32_t>( sizeof( uint32_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t iChunk = 0; iChunk < m_nCount; ++iChunk ) {
		if ( SIRStringOps::overlaps( output, m_pSlots[indexOf( iChunk )].getView() ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	uint32_t iOutput = 0;
	for ( uint32_t iChunk = 0; iChunk < m_nCount; ++iChunk ) {
		const sirStringView_t view = m_pSlots[indexOf( iChunk )].getView();
		memcpy( pszOutput + iOutput, view.pBytes, view.nBytes );
		iOutput += view.nBytes;
	}
	pszOutput[iOutput] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCordString::tryPopFront( void ) noexcept
{
	if ( m_nCount == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_EMPTY;
	}
	const uint32_t nBytes = m_pSlots[m_iHead].getView().nBytes;
	updateIndex( m_iHead, nBytes, true );
	m_nLength -= nBytes;
	m_pSlots[m_iHead].reset();
	m_iHead = indexOf( 1 );
	--m_nCount;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCordString::tryPopBack( void ) noexcept
{
	if ( m_nCount == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_EMPTY;
	}
	const uint32_t iSlot = indexOf( --m_nCount );
	const uint32_t nBytes = m_pSlots[iSlot].getView().nBytes;
	updateIndex( iSlot, nBytes, true );
	m_nLength -= nBytes;
	m_pSlots[iSlot].reset();
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRCordString::clear( void ) noexcept
{
	while ( m_nCount != 0 ) {
		m_pSlots[indexOf( --m_nCount )].reset();
	}
	m_iHead = 0;
	m_nLength = 0;
	if ( m_pIndex != nullptr ) {
		memset( m_pIndex, 0, static_cast<size_t>( m_nCapacity ) * sizeof( uint32_t ) );
	}
}

SIRContainerResult SIRCordString::tryAppendOwned( SIRSharedString& chunk ) noexcept
{
	const sirStringView_t chunkView = chunk.getView();
	const uint32_t nBytes = chunkView.nBytes;
	if ( m_pIndex != nullptr &&
	     SIRStringOps::overlaps( chunkView, { reinterpret_cast<const char *>( m_pIndex ), m_nCapacity * static_cast<uint32_t>( sizeof( uint32_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( m_nCount == m_nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( nBytes > UINT32_MAX - m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	const sirStringView_t slots{ reinterpret_cast<const char *>( m_pSlots ), static_cast<uint32_t>( m_nCapacity * sizeof( SIRSharedString ) ) };
	const sirStringView_t source{ reinterpret_cast<const char *>( &chunk ), static_cast<uint32_t>( sizeof( SIRSharedString ) ) };
	if ( SIRStringOps::overlaps( slots, source ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t iSlot = indexOf( m_nCount );
	m_pSlots[iSlot] = static_cast<SIRSharedString&&>( chunk );
	updateIndex( iSlot, nBytes, false );
	++m_nCount;
	m_nLength += nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRCordString::tryGetByte( uint32_t iByte, char& nByte ) const noexcept
{
	if ( iByte >= m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_pIndex != nullptr ) {
		const uint32_t iSlot = indexedSlot( iByte );
		nByte = m_pSlots[iSlot].getView().pBytes[iByte];
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	for ( uint32_t iChunk = 0; iChunk < m_nCount; ++iChunk ) {
		const sirStringView_t view = m_pSlots[indexOf( iChunk )].getView();
		if ( iByte < view.nBytes ) {
			nByte = view.pBytes[iByte];
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		iByte -= view.nBytes;
	}
	return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
}

SIRContainerResult SIRCordString::tryEnableIndex( uint32_t *pIndex, uint32_t nEntries ) noexcept
{
	if ( pIndex == nullptr || m_nCapacity == 0 || nEntries < m_nCapacity ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const sirStringView_t index{ reinterpret_cast<const char *>( pIndex ), m_nCapacity * static_cast<uint32_t>( sizeof( uint32_t ) ) };
	const sirStringView_t slots{ reinterpret_cast<const char *>( m_pSlots ), static_cast<uint32_t>( m_nCapacity * sizeof( SIRSharedString ) ) };
	if ( SIRStringOps::overlaps( index, slots ) ||
	     SIRStringOps::overlaps( index, { reinterpret_cast<const char *>( this ), static_cast<uint32_t>( sizeof( *this ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t iChunk = 0; iChunk < m_nCount; ++iChunk ) {
		if ( SIRStringOps::overlaps( index, m_pSlots[indexOf( iChunk )].getView() ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	m_pIndex = pIndex;
	memset( m_pIndex, 0, static_cast<size_t>( m_nCapacity ) * sizeof( uint32_t ) );
	for ( uint32_t iChunk = 0; iChunk < m_nCount; ++iChunk ) {
		const uint32_t iSlot = indexOf( iChunk );
		updateIndex( iSlot, m_pSlots[iSlot].getView().nBytes, false );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRCordString::updateIndex( uint32_t iSlot, uint32_t nBytes, bool bRemove ) noexcept
{
	if ( m_pIndex == nullptr ) {
		return;
	}
	for ( uint32_t i = iSlot; i < m_nCapacity; i |= i + 1 ) {
		if ( bRemove ) {
			m_pIndex[i] -= nBytes;
		} else {
			m_pIndex[i] += nBytes;
		}
	}
}

uint32_t SIRCordString::prefixBytes( uint32_t nSlots ) const noexcept
{
	uint32_t nSum = 0;
	while ( nSlots != 0 ) {
		nSum += m_pIndex[nSlots - 1];
		nSlots &= nSlots - 1;
	}
	return nSum;
}

uint32_t SIRCordString::indexedSlot( uint32_t& iByte ) const noexcept
{
	const uint32_t nPrefix = prefixBytes( m_iHead );
	const uint32_t nTail = m_nLength - nPrefix;
	iByte = iByte < nTail ? nPrefix + iByte : iByte - nTail;
	uint32_t nStep = 1;
	while ( nStep <= m_nCapacity / 2 ) {
		nStep <<= 1;
	}
	uint32_t iSlot = 0;
	for ( ; nStep != 0; nStep >>= 1 ) {
		if ( nStep <= m_nCapacity - iSlot ) {
			const uint32_t iNext = iSlot + nStep;
			const uint32_t nBytes = m_pIndex[iNext - 1];
			if ( nBytes <= iByte ) {
				iByte -= nBytes;
				iSlot = iNext;
			}
		}
	}
	assert( iSlot < m_nCapacity );
	return iSlot;
}
