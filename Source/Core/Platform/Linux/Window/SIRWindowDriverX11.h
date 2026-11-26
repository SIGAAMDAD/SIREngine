/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_WINDOWDRIVERX11_H__
#define __SIR_WINDOWDRIVERX11_H__
#include <Core/SIRPCH.h>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Shared/Window/SIRWindowDriver.h>
#include <X11/Xlib.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>

typedef void ( *sirWindowX11EventSink_t )( const XEvent& event, void *pContext ) noexcept;

// The sole XOpenDisplay/XNextEvent owner for windows created by this driver.
class SIRWindowDriverX11 final : public SIRWindowDriver
{
public:
	explicit SIRWindowDriverX11( void ) noexcept = default;
	~SIRWindowDriverX11() noexcept override;

	[[nodiscard]] sirWindowResult initialize( void ) noexcept override;
	[[nodiscard]] sirWindowResult createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override;
	[[nodiscard]] sirWindowResult destroyWindow( uint32_t nSlot ) noexcept override;
	[[nodiscard]] sirWindowResult mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override;
	[[nodiscard]] sirWindowResult pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override;
	void shutdown( void ) noexcept override;
	bool setEventSink( sirWindowX11EventSink_t pSink, void *pContext ) noexcept;

protected:
private:
	[[nodiscard]] sirWindowResult createWindowNative( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept;
	[[nodiscard]] sirWindowResult destroyWindowNative( uint32_t nSlot ) noexcept;
	[[nodiscard]] sirWindowResult mutateWindowNative( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept;
	[[nodiscard]] sirWindowResult pumpEventsNative( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept;
	void shutdownNative( void ) noexcept;
	void markServerLost( void ) noexcept;
	bool transportAlive( void ) noexcept;
	[[nodiscard]] sirWindowResult emitServerLoss( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept;
	void *m_pDisplay = nullptr;
	sirWindowX11EventSink_t m_pSink = nullptr;
	void *m_pSinkContext = nullptr;
	uintptr_t m_hWindows[SIR_WINDOW_MAX_WINDOWS] = {};
	bool m_bLost[SIR_WINDOW_MAX_WINDOWS] = {};
	uintptr_t m_nProtocols = 0;
	uintptr_t m_nDeleteWindow = 0;
	uintptr_t m_nLostNative[SIR_WINDOW_MAX_WINDOWS] = {};
	uint32_t m_nLossPendingMask = 0;
	bool m_bServerLost = false;
	bool m_bHandlerRegistered = false;
	uintptr_t m_nRootWindow = 0;
	uint32_t m_nDisplayChangePendingMask = 0;
	uint32_t m_nServerWidth = 0;
	uint32_t m_nServerHeight = 0;
	int32_t m_nRandrEventBase = -1;
};
#endif // __SIR_WINDOWDRIVERX11_H__
