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

#ifndef __SIRENGINE_SIRLINUXX11INPUT_H__
#define __SIRENGINE_SIRLINUXX11INPUT_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Core/Platform/SIRInput.h>
#include <X11/Xlib.h>

// The window owner pumps XEvents and routes them to this adapter. It also
// selects requiredEventMask() on its window; this class never owns Display.
// Core X11 motion is accelerated; XI2 raw/relative motion is a later backend.
class SIRLinuxX11Input
{
public:
	explicit SIRLinuxX11Input( SIRInputSystem &input ) noexcept;
	SIRLinuxX11Input( const SIRLinuxX11Input & ) = delete;
	SIRLinuxX11Input &operator=( const SIRLinuxX11Input & ) = delete;
	~SIRLinuxX11Input();

	static long requiredEventMask( void ) noexcept;
	// Request detectable repeat at display initialization; false means the
	// window owner must filter synthetic X11 release/press repeat pairs.
	static bool enableDetectableAutoRepeat( Display *pDisplay ) noexcept;
	bool bind( Window hWindow, uint64_t nKeyboardNativeId, uint64_t nMouseNativeId ) noexcept;
	bool handleEvent( const XEvent &event ) noexcept;
	void shutdown( void ) noexcept;

private:
	bool loseFocus( void ) noexcept;
	bool handleButton( const XButtonEvent &event, bool bPressed ) noexcept;
	bool handleMotion( const XMotionEvent &event ) noexcept;

	SIRInputSystem &m_Input;
	Window m_hWindow;
	sirInputDeviceHandle_t m_hKeyboard;
	sirInputDeviceHandle_t m_hMouse;
	int32_t m_nLastRootX;
	int32_t m_nLastRootY;
	bool m_bAttached;
	bool m_bFocused;
	bool m_bHavePointerPosition;
};

#endif // __SIRENGINE_SIRLINUXX11INPUT_H__
