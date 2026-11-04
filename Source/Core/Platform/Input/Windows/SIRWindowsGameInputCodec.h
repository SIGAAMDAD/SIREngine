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

#ifndef __SIRENGINE_SIRWINDOWSGAMEINPUTCODEC_H__
#define __SIRENGINE_SIRWINDOWSGAMEINPUTCODEC_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Core/Platform/Input/SIRInputTypes.h>

typedef struct sirGameInputRawGamepad_s
{
	uint32_t nButtons;
	float fLX;
	float fLY;
	float fRX;
	float fRY;
	float fLT;
	float fRT;
} sirGameInputRawGamepad_t;

typedef struct sirGameInputDecodedGamepad_s
{
	uint32_t nButtons;
	int32_t nAxes[SIR_INPUT_AXIS_COUNT];
	bool bValid;
} sirGameInputDecodedGamepad_t;

class SIRWindowsGameInputCodec final
{
public:
	static constexpr uint32_t MAX_CHANGES = 20;

	static int32_t normalizeAxis( float fValue, bool bTrigger ) noexcept;
	static uint64_t timestampNanoseconds( uint64_t nMicroseconds ) noexcept;
	static bool decode( const sirGameInputRawGamepad_t &raw, const sirGameInputDecodedGamepad_t &previous,
	                    sirGameInputDecodedGamepad_t *pNext, sirInputControlChange_t *pChanges, uint32_t nCapacity,
	                    uint32_t *pCount ) noexcept;
};

#endif // __SIRENGINE_SIRWINDOWSGAMEINPUTCODEC_H__
