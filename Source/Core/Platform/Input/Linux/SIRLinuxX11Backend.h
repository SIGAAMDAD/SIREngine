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

#ifndef __SIRENGINE_SIRLINUXX11BACKEND_H__
#define __SIRENGINE_SIRLINUXX11BACKEND_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Platform/Input/SIRInputBackend.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <X11/Xlib.h>

// Native X11 window owner feeds XEvents here; Xlib connection is borrowed.
class SIRLinuxX11Backend final : public SIRInputBackend
{
public:
	explicit SIRLinuxX11Backend( void ) noexcept;
	sirInputBackendId_t id( void ) const noexcept override;
	void sample( SIRInputService& service ) noexcept override;
	void resynchronize( SIRInputService& service ) noexcept override;
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override;
	void stop( SIRInputService& service ) noexcept override;
	bool attachWindow( SIRInputService& service, Window hWindow ) noexcept;
	bool focusWindow( SIRInputService& service, Window hWindow ) noexcept;
	bool enableRawPointer( Display *pDisplay ) noexcept;
	bool processEvent( SIRInputService& service, const XEvent& event ) noexcept;
	static long requiredEventMask( void ) noexcept;
	static bool enableDetectableRepeat( Display *pDisplay ) noexcept;

private:
	Window m_hWindow;
	sirInputDeviceId_t m_hKeyboard;
	sirInputDeviceId_t m_hPointer;
	int32_t m_nRootX;
	int32_t m_nRootY;
	bool m_bAttached;
	bool m_bFocused;
	bool m_bHaveMotion;
	bool m_bRawPointer;
	int m_nXiOpcode;
	double m_fMotionRemainderX;
	double m_fMotionRemainderY;
};

#endif // __SIRENGINE_SIRLINUXX11BACKEND_H__
