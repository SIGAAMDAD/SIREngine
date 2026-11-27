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

#ifndef __SIR_NATIVE_WINDOW_X11_H__
#define __SIR_NATIVE_WINDOW_X11_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRNativeWindowDriver.h>
#include <Core/Platform/Shared/Window/SIREventTypes.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

// One Xlib owner thread publishes into the engine's SPSC event queue.
// The consumer owns the queue and must outlive the driver.
class SIRNativeWindowDriverX11 final : public SIRNativeWindowDriver
{
public:
	explicit SIRNativeWindowDriverX11( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept;
	~SIRNativeWindowDriverX11() noexcept override;

	SIRWindowResult initialize( void ) noexcept override;
	SIRWindowResult createWindow( const SIRNativeWindowConfiguration& config ) noexcept override;
	void shutdown( void ) noexcept override;

	SIRWindowResult setTitle( const char *pszWindowTitle ) noexcept override;
	SIRWindowResult setPosition( uint32_t nPositionX, uint32_t nPositionY ) noexcept override;
	SIRWindowResult setSize( uint32_t nWidth, uint32_t nHeight ) noexcept override;
	SIRWindowResult pumpEvents( void ) noexcept override;

	SIRWindowResult destroyWindow( void ) noexcept;
	bool isServerLost( void ) const noexcept;
	Display *getDisplay( void ) const noexcept;

private:
	SIRWindowResult createWindowNative( const SIRNativeWindowConfiguration& config ) noexcept;
	SIRWindowResult pumpEventsNative( void ) noexcept;
	SIRWindowResult processWindowEvent( const XEvent& event ) noexcept;
	SIRWindowResult emitServerLoss( void ) noexcept;
	SIRWindowResult queueWindowEvent( SIREventType eType, uint32_t nWindowId, int32_t nData1 = 0, int32_t nData2 = 0 ) noexcept;
	SIRWindowResult queueEvent( const SIREventData& event ) noexcept;
	SIRWindowResult handleKeyboardEvent( const XKeyEvent& event, bool bDown ) noexcept;
	SIRWindowResult handleMouseEvent( const XEvent& event ) noexcept;
	SIRWindowResult flushPendingEvents( void ) noexcept;
	void markServerLost( void ) noexcept;
	bool transportAlive( void ) noexcept;
	void shutdownNative( void ) noexcept;

	Display *m_pDisplay = nullptr;
	Window m_hWindow = 0;
	Atom m_nDeleteWindow = 0;
	Atom m_nProtocols = 0;
	Window m_nRootWindow = 0;

	// At most two events are produced by an individual ConfigureNotify.
	SIREventData m_PendingEvents[2] = {};
	uint8_t m_nPendingCount = 0;
	uint32_t m_nLossPendingMask = 0;
	uintptr_t m_nLostNative = 0;
	uint32_t m_nServerWidth = 0;
	uint32_t m_nServerHeight = 0;
	uint32_t m_nWindowWidth = 0;
	uint32_t m_nWindowHeight = 0;
	int32_t m_nWindowX = 0;
	int32_t m_nWindowY = 0;
	uint64_t m_nWindowGeneration = 0;
	uint32_t m_nDisplayChangePendingMask = 0;
	uint32_t m_nMouseButtonState = 0;
	float m_fMouseX = 0.0F;
	float m_fMouseY = 0.0F;
	bool m_bMousePositionValid = false;
	bool m_bKeysDown[256] = {};
	bool m_bDetectableRepeat = false;
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
	int32_t m_nRandrEventBase = -1;
#endif
	bool m_bWindowLost = false;
	bool m_bServerLost = false;
	bool m_bHandlerRegistered = false;
};

#endif // __SIR_NATIVE_WINDOW_X11_H__
