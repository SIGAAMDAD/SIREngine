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

#ifndef __SIRENGINE_SIRWINDOWSXINPUT_H__
#define __SIRENGINE_SIRWINDOWSXINPUT_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Platform/SIRInputBackend.h>

typedef struct sirXInputSlot_s
{
	sirInputDeviceHandle_t hDevice;
	uint32_t nPacket;
	bool bActive;
} sirXInputSlot_t;

// Compatibility backend for XInput controllers. A future GameInput/HID owner
// must arbitrate physical devices before enabling both sources.
class SIRWindowsXInput : public SIRInputBackend
{
public:
	sirInputBackendKind_t getBackendKind( void ) const noexcept override
	{
		return SIR_INPUT_BACKEND_WINDOWS_XINPUT;
	}
	uint32_t pump( void ) noexcept override
	{
		return poll();
	}
	bool ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept override;
	bool setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept override;
	explicit SIRWindowsXInput( SIRInputSystem &input ) noexcept;
	SIRWindowsXInput( const SIRWindowsXInput & ) = delete;
	SIRWindowsXInput &operator=( const SIRWindowsXInput & ) = delete;
	~SIRWindowsXInput();

	bool initialize( void ) noexcept;
	uint32_t poll( void ) noexcept;
	bool setVibration( sirInputDeviceHandle_t hDevice, uint16_t nLeftMotor, uint16_t nRightMotor ) noexcept;
	void shutdown( void ) noexcept override;

private:
	SIRInputSystem &m_Input;
	void *m_pLibrary;
	void *m_pGetState;
	void *m_pSetState;
	sirXInputSlot_t m_Devices[4];
};

#endif // __SIRENGINE_SIRWINDOWSXINPUT_H__
