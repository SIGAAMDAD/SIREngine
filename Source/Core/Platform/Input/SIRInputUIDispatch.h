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

#ifndef __SIRENGINE_SIRINPUTUIDISPATCH_H__
#define __SIRENGINE_SIRINPUTUIDISPATCH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputActions.h>
#include <Core/Platform/Input/SIRInputText.h>

class SIRInputService;

// Native POD callback ABI suitable for editor widgets, C++ gameplay, and
// future script runtimes. Context ownership remains with the caller.
typedef struct sirInputUIConsumer_s
{
	void *pContext;
	const uint32_t *pActionIds;
	uint32_t nActionCount;
	uint32_t nUser;
	void ( *pOnAction )( void *pContext, const sirInputActionState_t *pState );
	void ( *pOnText )( void *pContext, uint32_t nCodepoint );
	void ( *pOnPreedit )( void *pContext, const uint32_t *pCodepoints, uint32_t nCount, uint32_t nCaret, uint32_t nSelection );
	void ( *pOnFocus )( void *pContext, bool bFocused );
	void ( *pOnPointer )( void *pContext, const sirInputEvent_t *pEvent );
} sirInputUIConsumer_t;

// One dispatch cursor per subscriber; never owns or copies the input frame.
class SIRInputUIDispatch
{
public:
	SIRInputUIDispatch( void ) noexcept;
	bool dispatch( const SIRInputService& input, const sirInputUIConsumer_t& consumer ) noexcept;
	void reset( void ) noexcept;

private:
	uint64_t m_nLastTick;
	uint32_t m_nPreeditRevision;
	bool m_bFocusKnown;
	bool m_bFocused;
};

#endif // __SIRENGINE_SIRINPUTUIDISPATCH_H__
