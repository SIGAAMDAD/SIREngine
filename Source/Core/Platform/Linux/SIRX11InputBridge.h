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
#ifndef __SIRENGINE_SIRX11INPUTBRIDGE_H__
#define __SIRENGINE_SIRX11INPUTBRIDGE_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Linux/SIRLinuxX11Backend.h>
#include <Core/Input/SIRInputCursorMode.h>
#include <Core/Platform/Window/SIRWindowTypes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Borrows the WindowSystem display and XID. Never creates, closes or pumps a display.
class SIRX11InputBridge final
{
public:
	SIRX11InputBridge( void ) noexcept = default;
	~SIRX11InputBridge( void ) noexcept;

	bool bind( SIRInputService& input, SIRLinuxX11Backend& backend, const sirWindowNativeView_t& view ) noexcept;
	bool isBound( void ) const noexcept;
	bool beginFrame( void ) noexcept;
	bool registerWindow( const sirWindowNativeView_t& view ) noexcept;
	bool unregisterWindow( uintptr_t nNativeWindow ) noexcept;
	bool setCursorMode( sirInputCursorMode_t eMode ) noexcept;
	sirInputCursorMode_t cursorMode( void ) const noexcept;
	void setTextEnabled( bool bEnabled ) noexcept;
	void processEvent( const XEvent& event ) noexcept;
	void unbind( void ) noexcept;

protected:
private:
	SIRInputService *m_pInput = nullptr;
	SIRLinuxX11Backend *m_pBackend = nullptr;
	Display *m_pDisplay = nullptr;
	Window m_hWindow = 0;
	XIM m_hInputMethod = nullptr;
	XIC m_hInputContext = nullptr;
	Window m_hRegisteredWindows[SIR_WINDOW_MAX_WINDOWS] = {};
	XIC m_hRegisteredContexts[SIR_WINDOW_MAX_WINDOWS] = {};
	uint32_t m_nWindowCount = 0;
	bool m_bAttached = false;
	bool m_bFocused = false;
	bool m_bDeferredResync = false;
	bool m_bDeferredRetarget = false;
	bool m_bCursorCaptured = false;
	sirInputCursorMode_t m_eCursorMode = SIR_INPUT_CURSOR_FREE;
	uintptr_t m_nInvisibleCursor = 0;
};
#endif // __SIRENGINE_SIRX11INPUTBRIDGE_H__
