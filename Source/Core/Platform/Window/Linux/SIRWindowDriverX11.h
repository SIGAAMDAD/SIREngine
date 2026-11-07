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
#ifndef __SIRENGINE_SIRWINDOWDRIVERX11_H__
#define __SIRENGINE_SIRWINDOWDRIVERX11_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Window/SIRWindowDriver.h>
#include <X11/Xlib.h>

typedef void ( *sirWindowX11EventSink_t )( const XEvent& event, void *pContext ) noexcept;

// The sole XOpenDisplay/XNextEvent owner for windows created by this driver.
class SIRWindowDriverX11 final : public SIRWindowDriver
{
public:
	SIRWindowDriverX11( void ) noexcept = default;
	~SIRWindowDriverX11( void ) noexcept override;

	sirWindowResult_t initialize( void ) noexcept override;
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override;
	sirWindowResult_t destroyWindow( uint32_t nSlot ) noexcept override;
	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override;
	void shutdown( void ) noexcept override;
	bool setEventSink( sirWindowX11EventSink_t pSink, void *pContext ) noexcept;

protected:
private:
	void *m_pDisplay = nullptr;
	sirWindowX11EventSink_t m_pSink = nullptr;
	void *m_pSinkContext = nullptr;
	uintptr_t m_hWindows[SIR_WINDOW_MAX_WINDOWS] = {};
	bool m_bLost[SIR_WINDOW_MAX_WINDOWS] = {};
	uintptr_t m_nProtocols = 0;
	uintptr_t m_nDeleteWindow = 0;
};
#endif // __SIRENGINE_SIRWINDOWDRIVERX11_H__
