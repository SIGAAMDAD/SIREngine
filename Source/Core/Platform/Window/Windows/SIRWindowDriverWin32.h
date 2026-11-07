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
#ifndef __SIRENGINE_SIRWINDOWDRIVERWIN32_H__
#define __SIRENGINE_SIRWINDOWDRIVERWIN32_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Platform/Window/SIRWindowDriver.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

typedef struct sirWindowWin32Message_s
{
	uintptr_t nWindow;
	uint32_t nMessage;
	uintptr_t nWParam;
	intptr_t nLParam;
} sirWindowWin32Message_t;

typedef struct sirWindowWin32Dispatch_s
{
	intptr_t nResult;
	bool bHandled;
} sirWindowWin32Dispatch_t;

typedef sirWindowWin32Dispatch_t ( *sirWindowWin32MessageSink_t )( const sirWindowWin32Message_t& message, void *pContext ) noexcept;

class SIRWindowDriverWin32;

typedef struct sirWin32Slot_s
{
	SIRWindowDriverWin32 *pDriver;
	HWND hWindow;
	uint32_t nIndex;
	bool bDestroying;
	bool bLost;
	bool bFullscreen;
	DWORD nWindowedStyle;
	DWORD nWindowedExStyle;
	WINDOWPLACEMENT windowedPlacement;
} sirWin32Slot_t;

class SIRWindowDriverWin32 final : public SIRWindowDriver
{
public:
	SIRWindowDriverWin32( void ) noexcept = default;
	~SIRWindowDriverWin32( void ) noexcept override;

	sirWindowResult_t initialize( void ) noexcept override;
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override;
	sirWindowResult_t destroyWindow( uint32_t nSlot ) noexcept override;
	sirWindowResult_t mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override;
	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override;
	void shutdown( void ) noexcept override;
	bool setMessageSink( sirWindowWin32MessageSink_t pSink, void *pContext ) noexcept;

protected:
private:
	static LRESULT CALLBACK windowProc( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam );
	void queueEvent( uint32_t nSlot, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept;

	sirWin32Slot_t m_Slots[SIR_WINDOW_MAX_WINDOWS] = {};
	sirWindowBackendEvent_t m_Events[SIR_WINDOW_EVENT_CAPACITY] = {};
	uint32_t m_nEventRead = 0;
	uint32_t m_nEventWrite = 0;
	uint32_t m_nEventCount = 0;
	sirWindowWin32MessageSink_t m_pSink = nullptr;
	void *m_pSinkContext = nullptr;
	bool m_bRegistered = false;
	bool m_bOverflow = false;
	bool m_bClosePending[SIR_WINDOW_MAX_WINDOWS] = {};
	bool m_bLostPending[SIR_WINDOW_MAX_WINDOWS] = {};
	uintptr_t m_nLostNativeWindow[SIR_WINDOW_MAX_WINDOWS] = {};
};
#endif // __SIRENGINE_SIRWINDOWDRIVERWIN32_H__
