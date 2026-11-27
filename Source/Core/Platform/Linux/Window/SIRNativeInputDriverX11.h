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

#ifndef __SIR_NATIVE_INPUT_DRIVER_X11_H__
#define __SIR_NATIVE_INPUT_DRIVER_X11_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRNativeInputDriver.h>
#include <Core/Containers/SIRArray.h>
#include <Core/Containers/SIRVector.h>

#include "SIRNativeInputEvdevGamepadDriver.h"

struct _XDisplay;
union _XEvent;

class SIRNativeInputDriverX11 final : public SIRNativeInputDriver
{
public:
	explicit SIRNativeInputDriverX11( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept;
	~SIRNativeInputDriverX11() noexcept;
	SIRNativeInputDriverX11( const SIRNativeInputDriverX11& ) = delete;
	SIRNativeInputDriverX11& operator=( const SIRNativeInputDriverX11& ) = delete;

	// Borrow the window driver's connection. The platform owner must
	// route X11 input events here and detach before window destruction.
	bool attachWindow( _XDisplay *pDisplay, unsigned long hWindow, uint32_t nWindowId ) noexcept;
	void detachWindow( bool bConnectionLost = false ) noexcept;
	static void eventSink( const _XEvent& eventData, void *pContext ) noexcept;

	SIRNativeInputBackend getBackend( void ) const noexcept override;
	void pumpEvents( void ) noexcept override;
	uint32_t droppedEventCount( void ) const noexcept;
	bool isKeyPressed( SIRKeycode eKey ) const noexcept override;
	bool isMouseButtonPressed( SIRMouseButton eButton ) const noexcept override;

private:
	void processEvent( const _XEvent& eventData ) noexcept;
	void processMouseButtonEvent( const _XEvent& eventData ) noexcept;
	void updateKeyboardMap( void ) noexcept;
	void keyEvent( unsigned int nCode, bool bDown, unsigned int nState ) noexcept;
	void buttonEvent( SIRMouseButton eButton, bool bDown ) noexcept;
	void releaseInput( void ) noexcept;
	void extensionEvent( const _XEvent& eventData ) noexcept;
	void publish( SIREventData& eventData ) noexcept;

	SIRArray<SIRScancode, 256> m_szScancodes{};
	SIRArray<SIRKeyboardEvent, 256> m_szKeys{};

	SIRVector<SIRTouchFingerEvent> m_Contacts;

	uint32_t m_nDroppedEvents = 0;
	SIRNativeInputEvdevGamepadDriver m_Gamepads;
	_XDisplay *m_pDisplay = nullptr;
	unsigned long m_hWindow = 0;
	unsigned long m_hRoot = 0;
	long m_nAddedCoreMask = 0;
	uint32_t m_nWindowId = 0;

	uint16_t m_nModifiers = 0;
	unsigned int m_nNumMask = 0;
	unsigned int m_nScrollMask = 0;
	unsigned int m_nModeMask = 0;
	unsigned int m_nLevel5Mask = 0;
	uint32_t m_nButtons = 0;
	float m_fX = 0;
	float m_fY = 0;
	bool m_bHavePosition = false;
	bool m_bFocused = false;
	bool m_bDetectableRepeat = false;
	bool m_bRestoreRepeat = false;
	bool m_bRawMotion = false;
	int m_nXIOpcode = -1;
	uint32_t m_nAddedRootXI = 0;
	uint32_t m_nAddedWindowXI = 0;
	int m_nWidth = 0;
	int m_nHeight = 0;
};

#endif // __SIR_NATIVE_INPUT_DRIVER_X11_H__
