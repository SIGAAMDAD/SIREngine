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

#ifndef __SIR_INPUTWINDOW_H__
#define __SIR_INPUTWINDOW_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputBackend.h>

class SIRInputService;

enum class SIRInputCursorMode : uint8_t {
	SIR_INPUT_CURSOR_FREE = 0,
	SIR_INPUT_CURSOR_HIDDEN = 1,
	SIR_INPUT_CURSOR_CAPTURED = 2
};

// First-party native window message owner. Native window types stay in the
// platform translation units; input is sampled within SIRApplication::tick.
class SIRInputWindow
{
public:
	SIRInputWindow( void ) noexcept;
	~SIRInputWindow( void );
	SIRInputWindow( const SIRInputWindow& ) = delete;
	SIRInputWindow& operator=( const SIRInputWindow& ) = delete;

	bool open( SIRInputService& input, SIRInputBackend& desktop ) noexcept;
	bool pump( SIRInputService& input ) noexcept;
	bool isOpen( void ) const noexcept;
	bool setCursorMode( SIRInputCursorMode eMode ) noexcept;
	SIRInputCursorMode cursorMode( void ) const noexcept;
	bool focused( void ) const noexcept;
	void setTextEnabled( bool bEnabled ) noexcept;
	void close( void ) noexcept;
#if defined( _WIN32 )
	void adoptNativeWindow( uintptr_t nWindow ) noexcept;
	intptr_t dispatchNativeMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept;
#endif

private:
	SIRInputService *m_pInput;
	SIRInputBackend *m_pDesktop;
	void *m_pDisplay;
	uintptr_t m_nWindow;
	bool m_bOpen;
	bool m_bAttached;
	bool m_bRegistered;
	bool m_bFocusLost;
	bool m_bFocused;
	bool m_bCursorCaptured;
	SIRInputCursorMode m_eCursorMode;
	void *m_pInputMethod;
	void *m_pInputContext;
	uintptr_t m_nInvisibleCursor;
};

#endif // __SIR_INPUTWINDOW_H__
