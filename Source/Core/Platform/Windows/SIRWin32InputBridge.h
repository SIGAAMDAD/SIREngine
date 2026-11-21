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
#ifndef __SIRENGINE_SIRWIN32INPUTBRIDGE_H__
#define __SIRENGINE_SIRWIN32INPUTBRIDGE_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Windows/SIRWindowsRawBackend.h>
#include <Core/Input/SIRInputCursorMode.h>
#include <Core/Platform/Window/Windows/SIRWindowDriverWin32.h>

// Win32 message decoder only; the native window and message pump belong to WindowSystem.
class SIRWin32InputBridge final
{
public:
	SIRWin32InputBridge( void ) noexcept = default;
	~SIRWin32InputBridge( void ) noexcept;

	bool bind( SIRInputService& input, SIRWindowsRawBackend& backend, const sirWindowNativeView_t& view ) noexcept;
	bool isBound( void ) const noexcept;
	bool registerWindow( const sirWindowNativeView_t& view ) noexcept;
	bool unregisterWindow( uintptr_t nNativeWindow ) noexcept;
	bool acceptsWindow( uintptr_t nWindow ) const noexcept;
	bool setCursorMode( sirInputCursorMode_t eMode ) noexcept;
	sirInputCursorMode_t cursorMode( void ) const noexcept;
	void setTextEnabled( bool bEnabled ) noexcept;
	void finishPump( void ) noexcept;
	intptr_t processWindowMessage( const sirWindowWin32Message_t& message, bool *pHandled ) noexcept;
	void unbind( void ) noexcept;

protected:
private:
	intptr_t processMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam, bool *pHandled ) noexcept;
	SIRInputService *m_pInput = nullptr;
	SIRWindowsRawBackend *m_pBackend = nullptr;
	uintptr_t m_nWindow = 0;
	uintptr_t m_nInstance = 0;
	uintptr_t m_hRegisteredWindows[SIR_WINDOW_MAX_WINDOWS] = {};
	uint32_t m_nWindowCount = 0;
	bool m_bFocused = false;
	bool m_bFocusLost = false;
	bool m_bCursorCaptured = false;
	sirInputCursorMode_t m_eCursorMode = SIR_INPUT_CURSOR_FREE;
};
#endif // __SIRENGINE_SIRWIN32INPUTBRIDGE_H__
