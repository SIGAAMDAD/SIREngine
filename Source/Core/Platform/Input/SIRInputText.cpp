/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/Input/SIRInputText.h>
#include <string.h>

SIRInputText::SIRInputText( void ) noexcept : m_Snapshot{}, m_nPendingSurrogate( 0 ), m_bCollecting( false )
{
}

bool SIRInputText::validCodepoint( uint32_t nCodepoint ) noexcept
{
	return nCodepoint != 0 && nCodepoint <= 0x10ffff && ( nCodepoint < 0xd800 || nCodepoint > 0xdfff );
}

void SIRInputText::begin( uint64_t nTick ) noexcept
{
	m_Snapshot.nTick = nTick;
	m_Snapshot.nCount = 0;
	m_Snapshot.bOverflow = false;
	m_nPendingSurrogate = 0;
	m_bCollecting = true;
}

void SIRInputText::publish( void ) noexcept
{
	m_bCollecting = false;
	m_nPendingSurrogate = 0;
}

void SIRInputText::setEnabled( bool bEnabled ) noexcept
{
	if ( m_Snapshot.bEnabled == bEnabled ) {
		return;
	}
	m_Snapshot.bEnabled = bEnabled;
	m_nPendingSurrogate = 0;
	if ( !bEnabled ) {
		clearPreeditInternal();
	}
}

void SIRInputText::setFocused( bool bFocused ) noexcept
{
	if ( m_Snapshot.bFocused == bFocused ) {
		return;
	}
	m_Snapshot.bFocused = bFocused;
	m_nPendingSurrogate = 0;
	if ( !bFocused ) {
		clearPreeditInternal();
	}
}

bool SIRInputText::appendCodepoint( uint32_t nCodepoint ) noexcept
{
	if ( !m_bCollecting || !m_Snapshot.bEnabled || !m_Snapshot.bFocused || !validCodepoint( nCodepoint ) ) {
		return false;
	}
	if ( m_Snapshot.nCount == SIR_INPUT_TEXT_CAPACITY ) {
		m_Snapshot.bOverflow = true;
		return false;
	}
	m_Snapshot.szCodepoints[m_Snapshot.nCount++] = nCodepoint;
	return true;
}

bool SIRInputText::appendUtf16( uint16_t nUnit ) noexcept
{
	if ( !m_bCollecting || !m_Snapshot.bEnabled || !m_Snapshot.bFocused ) {
		return false;
	}
	if ( nUnit >= 0xd800 && nUnit <= 0xdbff ) {
		m_nPendingSurrogate = nUnit;
		return true;
	}
	if ( m_nPendingSurrogate != 0 ) {
		const uint16_t nHigh = m_nPendingSurrogate;
		m_nPendingSurrogate = 0;
		if ( nUnit < 0xdc00 || nUnit > 0xdfff ) {
			return false;
		}
		const uint32_t nScalar = 0x10000 + ( ( uint32_t )( nHigh - 0xd800 ) << 10 ) + ( uint32_t )( nUnit - 0xdc00 );
		return appendCodepoint( nScalar );
	}
	return appendCodepoint( nUnit );
}

bool SIRInputText::decodeUtf8( const char *pszUtf8, uint32_t nBytes, uint32_t *pOut, uint32_t nCapacity, uint32_t *pCount ) noexcept
{
	if ( ( pszUtf8 == nullptr && nBytes != 0 ) || pOut == nullptr || pCount == nullptr ) {
		return false;
	}
	uint32_t nDecoded = 0;
	for ( uint32_t i = 0; i < nBytes; ) {
		const uint8_t nFirst = ( uint8_t )pszUtf8[i++];
		uint32_t nCodepoint = 0;
		uint32_t nContinuation = 0;
		uint32_t nMinimum = 0;
		if ( nFirst < 0x80 ) {
			nCodepoint = nFirst;
		} else if ( nFirst >= 0xc2 && nFirst <= 0xdf ) {
			nCodepoint = nFirst & 0x1f;
			nContinuation = 1;
			nMinimum = 0x80;
		} else if ( nFirst >= 0xe0 && nFirst <= 0xef ) {
			nCodepoint = nFirst & 0x0f;
			nContinuation = 2;
			nMinimum = 0x800;
		} else if ( nFirst >= 0xf0 && nFirst <= 0xf4 ) {
			nCodepoint = nFirst & 0x07;
			nContinuation = 3;
			nMinimum = 0x10000;
		} else {
			return false;
		}
		if ( nContinuation > nBytes - i ) {
			return false;
		}
		for ( uint32_t j = 0; j < nContinuation; ++j ) {
			const uint8_t nNext = ( uint8_t )pszUtf8[i++];
			if ( ( nNext & 0xc0 ) != 0x80 ) {
				return false;
			}
			nCodepoint = ( nCodepoint << 6 ) | ( nNext & 0x3f );
		}
		if ( nCodepoint < nMinimum || !validCodepoint( nCodepoint ) || nDecoded == nCapacity ) {
			return false;
		}
		pOut[nDecoded++] = nCodepoint;
	}
	*pCount = nDecoded;
	return true;
}

bool SIRInputText::decodeUtf16( const uint16_t *pUnits, uint32_t nCount, uint32_t *pOut, uint32_t nCapacity, uint32_t *pWritten ) noexcept
{
	if ( ( pUnits == nullptr && nCount != 0 ) || pOut == nullptr || pWritten == nullptr ) {
		return false;
	}
	uint32_t nDecoded = 0;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		uint32_t nScalar = pUnits[i];
		if ( nScalar >= 0xd800 && nScalar <= 0xdbff ) {
			if ( i + 1 >= nCount || pUnits[i + 1] < 0xdc00 || pUnits[i + 1] > 0xdfff ) {
				return false;
			}
			nScalar = 0x10000 + ( ( nScalar - 0xd800 ) << 10 ) + ( pUnits[++i] - 0xdc00 );
		}
		if ( !validCodepoint( nScalar ) || nDecoded == nCapacity ) {
			return false;
		}
		pOut[nDecoded++] = nScalar;
	}
	*pWritten = nDecoded;
	return true;
}

bool SIRInputText::appendUtf8( const char *pszUtf8, uint32_t nBytes ) noexcept
{
	if ( !m_bCollecting || !m_Snapshot.bEnabled || !m_Snapshot.bFocused ) {
		return false;
	}
	uint32_t szDecoded[SIR_INPUT_TEXT_CAPACITY] = {};
	uint32_t nDecoded = 0;
	if ( !decodeUtf8( pszUtf8, nBytes, szDecoded, SIR_INPUT_TEXT_CAPACITY, &nDecoded ) ) {
		return false;
	}
	if ( nDecoded > SIR_INPUT_TEXT_CAPACITY - m_Snapshot.nCount ) {
		m_Snapshot.bOverflow = true;
		return false;
	}
	memcpy( m_Snapshot.szCodepoints + m_Snapshot.nCount, szDecoded, nDecoded * sizeof( uint32_t ) );
	m_Snapshot.nCount += nDecoded;
	return true;
}

void SIRInputText::clearPreeditInternal( void ) noexcept
{
	if ( m_Snapshot.nPreeditCount != 0 || m_Snapshot.nCaret != 0 || m_Snapshot.nSelection != 0 ) {
		m_Snapshot.nPreeditCount = 0;
		m_Snapshot.nCaret = 0;
		m_Snapshot.nSelection = 0;
		++m_Snapshot.nPreeditRevision;
	}
}

bool SIRInputText::setPreeditUtf16( const uint16_t *pUnits, uint32_t nCount, uint32_t nCaret, uint32_t nSelection ) noexcept
{
	if ( !m_bCollecting || !m_Snapshot.bEnabled || !m_Snapshot.bFocused ) {
		return false;
	}
	uint32_t szDecoded[SIR_INPUT_PREEDIT_CAPACITY] = {};
	uint32_t nDecoded = 0;
	if ( !decodeUtf16( pUnits, nCount, szDecoded, SIR_INPUT_PREEDIT_CAPACITY, &nDecoded ) || nCaret > nDecoded ||
	     nSelection > nDecoded - nCaret ) {
		return false;
	}
	memcpy( m_Snapshot.szPreedit, szDecoded, nDecoded * sizeof( uint32_t ) );
	m_Snapshot.nPreeditCount = nDecoded;
	m_Snapshot.nCaret = nCaret;
	m_Snapshot.nSelection = nSelection;
	++m_Snapshot.nPreeditRevision;
	return true;
}

bool SIRInputText::setPreeditCodepoints( const uint32_t *pCodepoints, uint32_t nCount, uint32_t nCaret, uint32_t nSelection ) noexcept
{
	if ( !m_bCollecting || !m_Snapshot.bEnabled || !m_Snapshot.bFocused || nCount > SIR_INPUT_PREEDIT_CAPACITY ||
	     ( pCodepoints == nullptr && nCount != 0 ) || nCaret > nCount || nSelection > nCount - nCaret ) {
		return false;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( !validCodepoint( pCodepoints[i] ) ) {
			return false;
		}
	}
	if ( nCount != 0 ) {
		memcpy( m_Snapshot.szPreedit, pCodepoints, nCount * sizeof( uint32_t ) );
	}
	m_Snapshot.nPreeditCount = nCount;
	m_Snapshot.nCaret = nCaret;
	m_Snapshot.nSelection = nSelection;
	++m_Snapshot.nPreeditRevision;
	return true;
}

bool SIRInputText::setPreeditUtf8( const char *pszUtf8, uint32_t nBytes, uint32_t nCaret, uint32_t nSelection ) noexcept
{
	if ( !m_bCollecting || !m_Snapshot.bEnabled || !m_Snapshot.bFocused ) {
		return false;
	}
	uint32_t szDecoded[SIR_INPUT_PREEDIT_CAPACITY] = {};
	uint32_t nDecoded = 0;
	if ( !decodeUtf8( pszUtf8, nBytes, szDecoded, SIR_INPUT_PREEDIT_CAPACITY, &nDecoded ) || nCaret > nDecoded ||
	     nSelection > nDecoded - nCaret ) {
		return false;
	}
	memcpy( m_Snapshot.szPreedit, szDecoded, nDecoded * sizeof( uint32_t ) );
	m_Snapshot.nPreeditCount = nDecoded;
	m_Snapshot.nCaret = nCaret;
	m_Snapshot.nSelection = nSelection;
	++m_Snapshot.nPreeditRevision;
	return true;
}

void SIRInputText::clearPreedit( void ) noexcept
{
	clearPreeditInternal();
}

const sirInputTextSnapshot_t& SIRInputText::snapshot( void ) const noexcept
{
	return m_Snapshot;
}

void SIRInputText::reset( void ) noexcept
{
	m_Snapshot = {};
	m_nPendingSurrogate = 0;
	m_bCollecting = false;
}
