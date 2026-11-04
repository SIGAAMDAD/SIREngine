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

#ifndef __SIRENGINE_SIRLANGLEXER_H__
#define __SIRENGINE_SIRLANGLEXER_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Scripting/Language/SIRLangToken.h>

// Initializes bounded, allocation-free state. A nonempty source must have
// valid bytes; the first call to Next reports an invalid source otherwise.
void SIRLangLexerInit( sirLangLexer_t *pLexer, const sirLangSourceView_t &source ) noexcept;

// Returns a token with byte-oriented source location. After SIR_LANG_TOKEN_END
// the lexer remains at END. Any ERROR token is recoverable except INVALID_SOURCE.
sirLangToken_t SIRLangLexerNext( sirLangLexer_t *pLexer ) noexcept;

// Draft v1 keyword classification. Grammar selection can refine this list
// without changing the lexer/source-span representation.
bool SIRLangIsKeyword( const uint8_t *pBytes, uint32_t nBytes ) noexcept;

#endif // __SIRENGINE_SIRLANGLEXER_H__
