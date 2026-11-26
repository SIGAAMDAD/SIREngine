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

#include <Editor/Widgets/SIREditorVirtualList.h>

#include <limits.h>

static uint64_t sirEditorSaturatedAdd( uint64_t nLeft, uint64_t nRight ) noexcept
{
	return nRight > UINT64_MAX - nLeft ? UINT64_MAX : nLeft + nRight;
}

SIREditorVirtualList::SIREditorVirtualList( void ) noexcept
    : m_pRows( nullptr ), m_pFenwick( nullptr ), m_nCapacity( 0 ), m_nSize( 0 ), m_nTotalHeight( 0 )
{
}

SIREditorVirtualListResult SIREditorVirtualList::initialize( sirEditorVirtualRow_t *pRows, uint64_t *pFenwick,
                                                               uint32_t nCapacity ) noexcept
{
	if ( m_pRows != nullptr || pRows == nullptr || pFenwick == nullptr || nCapacity == 0 || nCapacity == UINT32_MAX ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_INVALID_ARGUMENT;
	}
	m_pRows = pRows;
	m_pFenwick = pFenwick;
	m_nCapacity = nCapacity;
	m_nSize = 0;
	m_nTotalHeight = 0;
	memset( m_pFenwick, 0, sizeof( uint64_t ) * ( static_cast<size_t>(nCapacity) + 1 ) );
	return SIREditorVirtualListResult::SIR_EDITOR_LIST_OK;
}

void SIREditorVirtualList::shutdown( void ) noexcept
{
	m_pRows = nullptr;
	m_pFenwick = nullptr;
	m_nCapacity = 0;
	m_nSize = 0;
	m_nTotalHeight = 0;
}

SIREditorVirtualListResult SIREditorVirtualList::setRows( const sirEditorVirtualRow_t *pRows, uint32_t nCount ) noexcept
{
	if ( m_pRows == nullptr ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_NOT_INITIALIZED;
	}
	if ( nCount > m_nCapacity ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_OUT_OF_CAPACITY;
	}
	if ( pRows == nullptr && nCount != 0 ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_INVALID_ARGUMENT;
	}
	for ( uint32_t iRow = 0; iRow < nCount; ++iRow ) {
		if ( pRows[iRow].nId == 0 || pRows[iRow].nHeight == 0 ) {
			return SIREditorVirtualListResult::SIR_EDITOR_LIST_INVALID_ARGUMENT;
		}
	}

	if ( nCount != 0 ) {
		memmove( m_pRows, pRows, sizeof( sirEditorVirtualRow_t ) * nCount );
	}
	memset( m_pFenwick, 0, sizeof( uint64_t ) * ( static_cast<size_t>(m_nCapacity) + 1 ) );
	m_nSize = nCount;
	m_nTotalHeight = 0;
	for ( uint32_t iRow = 1; iRow <= nCount; ++iRow ) {
		const uint32_t nHeight = m_pRows[iRow - 1].nHeight;
		m_nTotalHeight += nHeight;
		m_pFenwick[iRow] += nHeight;
		uint32_t iParent = iRow + ( iRow & ( ~iRow + 1 ) );
		if ( iParent <= nCount ) {
			m_pFenwick[iParent] += m_pFenwick[iRow];
		}
	}
	return SIREditorVirtualListResult::SIR_EDITOR_LIST_OK;
}

SIREditorVirtualListResult SIREditorVirtualList::setHeight( uint32_t iRow, uint32_t nHeight ) noexcept
{
	if ( m_pRows == nullptr ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_NOT_INITIALIZED;
	}
	if ( iRow >= m_nSize || nHeight == 0 ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_INVALID_ARGUMENT;
	}
	uint32_t nPrevious = m_pRows[iRow].nHeight;
	if ( nPrevious == nHeight ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_OK;
	}

	m_pRows[iRow].nHeight = nHeight;
	if ( nHeight > nPrevious ) {
		uint64_t nDelta = nHeight - nPrevious;
		m_nTotalHeight += nDelta;
		for ( uint32_t iFenwick = iRow + 1; iFenwick <= m_nSize; iFenwick += iFenwick & ( ~iFenwick + 1 ) ) {
			m_pFenwick[iFenwick] += nDelta;
		}
	} else {
		uint64_t nDelta = nPrevious - nHeight;
		m_nTotalHeight -= nDelta;
		for ( uint32_t iFenwick = iRow + 1; iFenwick <= m_nSize; iFenwick += iFenwick & ( ~iFenwick + 1 ) ) {
			m_pFenwick[iFenwick] -= nDelta;
		}
	}
	return SIREditorVirtualListResult::SIR_EDITOR_LIST_OK;
}

uint64_t SIREditorVirtualList::prefixHeight( uint32_t nCount ) const noexcept
{
	uint64_t nHeight = 0;
	for ( uint32_t iFenwick = nCount; iFenwick != 0; iFenwick -= iFenwick & ( ~iFenwick + 1 ) ) {
		nHeight += m_pFenwick[iFenwick];
	}
	return nHeight;
}

uint32_t SIREditorVirtualList::lowerBound( uint64_t nOffset ) const noexcept
{
	if ( nOffset >= m_nTotalHeight ) {
		return m_nSize;
	}
	uint32_t iPosition = 0;
	uint32_t nStep = 1;
	while ( nStep <= m_nSize / 2 ) {
		nStep <<= 1;
	}
	uint64_t nPrefix = 0;
	while ( nStep != 0 ) {
		uint32_t iNext = iPosition + nStep;
		if ( iNext <= m_nSize && m_pFenwick[iNext] <= nOffset - nPrefix ) {
			nPrefix += m_pFenwick[iNext];
			iPosition = iNext;
		}
		nStep >>= 1;
	}
	return iPosition;
}

SIREditorVirtualListResult SIREditorVirtualList::getRowOffset( uint32_t iRow, uint64_t *pOutOffset ) const noexcept
{
	if ( m_pRows == nullptr ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_NOT_INITIALIZED;
	}
	if ( iRow > m_nSize || pOutOffset == nullptr ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_INVALID_ARGUMENT;
	}
	*pOutOffset = prefixHeight( iRow );
	return SIREditorVirtualListResult::SIR_EDITOR_LIST_OK;
}

SIREditorVirtualListResult SIREditorVirtualList::getVisibleRange( uint64_t nScroll, uint64_t nViewportHeight, uint64_t nOverscan,
                                                                    sirEditorVirtualRange_t *pOutRange ) const noexcept
{
	if ( m_pRows == nullptr ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_NOT_INITIALIZED;
	}
	if ( pOutRange == nullptr ) {
		return SIREditorVirtualListResult::SIR_EDITOR_LIST_INVALID_ARGUMENT;
	}
	uint64_t nStart = nScroll > nOverscan ? nScroll - nOverscan : 0;
	uint64_t nEnd = sirEditorSaturatedAdd( nScroll, sirEditorSaturatedAdd( nViewportHeight, nOverscan ) );
	pOutRange->iBegin = lowerBound( nStart );
	pOutRange->iEnd = nEnd >= m_nTotalHeight ? m_nSize : ( nEnd > nStart ? lowerBound( nEnd - 1 ) + 1 : pOutRange->iBegin );
	return SIREditorVirtualListResult::SIR_EDITOR_LIST_OK;
}

uint32_t SIREditorVirtualList::getSize( void ) const noexcept
{
	return m_nSize;
}

uint64_t SIREditorVirtualList::getTotalHeight( void ) const noexcept
{
	return m_nTotalHeight;
}
