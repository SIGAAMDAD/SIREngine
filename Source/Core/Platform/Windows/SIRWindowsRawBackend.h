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

#ifndef __SIRENGINE_SIRWINDOWSRAWBACKEND_H__
#define __SIRENGINE_SIRWINDOWSRAWBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Input/SIRInputBackend.h>
#include <Core/Input/SIRInputService.h>

// Win32 window owner forwards WM_INPUT and WM_INPUT_DEVICE_CHANGE events.
// After WM_INPUT with RIM_INPUT, the window procedure must call DefWindowProc
// for the OS cleanup required by Win32.
class SIRWindowsRawBackend final : public SIRInputBackend
{
public:
	SIRWindowsRawBackend( void ) noexcept;
	sirInputBackendId_t id( void ) const noexcept override;
	void sample( SIRInputService& service ) noexcept override;
	void resynchronize( SIRInputService& service ) noexcept override;
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override;
	void stop( SIRInputService& service ) noexcept override;
	bool attachWindow( void *pWindow ) noexcept;
	bool retargetWindow( void *pWindow ) noexcept;
	bool processMessage( SIRInputService& service, uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept;

private:
	bool processRaw( SIRInputService& service, void *pHRawInput ) noexcept;
	bool drainBuffered( SIRInputService& service ) noexcept;
	void *m_pWindow;
};

#endif // __SIRENGINE_SIRWINDOWSRAWBACKEND_H__
