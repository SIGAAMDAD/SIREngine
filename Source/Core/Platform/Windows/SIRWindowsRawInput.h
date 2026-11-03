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

#ifndef __SIRENGINE_SIRWINDOWSRAWINPUT_H__
#define __SIRENGINE_SIRWINDOWSRAWINPUT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRInputBackend.h>

// Win32 WndProc forwards WM_INPUT and WM_INPUT_DEVICE_CHANGE to this adapter.
// It does not create a window or install its own message loop.
class SIRWindowsRawInput : public SIRInputBackend
{
public:
	sirInputBackendKind_t getBackendKind( void ) const noexcept override
	{
		return SIR_INPUT_BACKEND_WINDOWS_RAW;
	}
	uint32_t pump( void ) noexcept override
	{
		return 0;
	} // WndProc calls processMessage().
	bool ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept override;
	explicit SIRWindowsRawInput( SIRInputSystem &input ) noexcept;
	SIRWindowsRawInput( const SIRWindowsRawInput & ) = delete;
	SIRWindowsRawInput &operator=( const SIRWindowsRawInput & ) = delete;
	~SIRWindowsRawInput();

	bool initialize( void *pWindow ) noexcept;
	bool processMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept;
	void shutdown( void ) noexcept override;

private:
	bool processRawInput( void *pRawInput ) noexcept;
	bool forgetDevice( uint64_t nNativeId ) noexcept;

	SIRInputSystem &m_Input;
	void *m_pWindow;
};

#endif // __SIRENGINE_SIRWINDOWSRAWINPUT_H__
