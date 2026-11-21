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

#ifndef __SIRENGINE_SIRINPUTTEXT_H__
#define __SIRENGINE_SIRINPUTTEXT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once

#endif
// clang-format on

constexpr uint32_t SIR_INPUT_TEXT_CAPACITY = 256;
constexpr uint32_t SIR_INPUT_PREEDIT_CAPACITY = 128;

typedef struct sirInputTextSnapshot_s
{
	uint64_t nTick;
	uint32_t nCount;
	uint32_t nPreeditCount;
	uint32_t nCaret;
	uint32_t nSelection;
	uint32_t nPreeditRevision;
	bool bEnabled;
	bool bFocused;
	bool bOverflow;
	uint32_t szCodepoints[SIR_INPUT_TEXT_CAPACITY];
	uint32_t szPreedit[SIR_INPUT_PREEDIT_CAPACITY];
} sirInputTextSnapshot_t;

// Native text/IME is separate from gameplay key/button state. The caller is
// the single input-owner thread and may inspect the frozen state after publish.
class SIRInputText
{
public:
	SIRInputText( void ) noexcept;
	void begin( uint64_t nTick ) noexcept;
	void publish( void ) noexcept;
	void setEnabled( bool bEnabled ) noexcept;
	void setFocused( bool bFocused ) noexcept;
	bool appendCodepoint( uint32_t nCodepoint ) noexcept;
	bool appendUtf16( uint16_t nUnit ) noexcept;
	bool appendUtf8( const char *pszUtf8, uint32_t nBytes ) noexcept;
	bool setPreeditUtf16( const uint16_t *pUnits, uint32_t nCount, uint32_t nCaret, uint32_t nSelection ) noexcept;
	bool setPreeditCodepoints( const uint32_t *pCodepoints, uint32_t nCount, uint32_t nCaret, uint32_t nSelection ) noexcept;
	bool setPreeditUtf8( const char *pszUtf8, uint32_t nBytes, uint32_t nCaret, uint32_t nSelection ) noexcept;
	void clearPreedit( void ) noexcept;
	const sirInputTextSnapshot_t& snapshot( void ) const noexcept;
	void reset( void ) noexcept;
	static bool decodeUtf8( const char *pszUtf8, uint32_t nBytes, uint32_t *pOut, uint32_t nCapacity, uint32_t *pCount ) noexcept;

private:
	static bool validCodepoint( uint32_t nCodepoint ) noexcept;
	static bool decodeUtf16( const uint16_t *pUnits, uint32_t nCount, uint32_t *pOut, uint32_t nCapacity, uint32_t *pWritten ) noexcept;
	void clearPreeditInternal( void ) noexcept;
	sirInputTextSnapshot_t m_Snapshot;
	uint16_t m_nPendingSurrogate;
	bool m_bCollecting;
};

#endif // __SIRENGINE_SIRINPUTTEXT_H__
