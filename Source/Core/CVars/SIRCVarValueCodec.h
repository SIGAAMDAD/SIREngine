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

#ifndef __SIRENGINE_SIRCVARVALUECODEC_H__
#define __SIRENGINE_SIRCVARVALUECODEC_H__

#include <SIRPCH.hpp>
#include <Core/CVars/SIRCVarRegistry.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef struct sirCVarTextBuffer_s
{
	char *pszOutput;
	uint32_t nCapacity;
	uint32_t nLength;
} sirCVarTextBuffer_t;

// Shared strict textual value codec for console and config, no allocations.
sirRegistrationResult_t sirCVarParseText( sirCVarType_t eType, const char *pszInput, uint32_t nLength, sirCVarValue_t *pValue ) noexcept;
sirRegistrationResult_t sirCVarFormatText( sirCVarType_t eType, const sirCVarValue_t& value, sirCVarTextBuffer_t *pBuffer ) noexcept;

#endif // __SIRENGINE_SIRCVARVALUECODEC_H__
