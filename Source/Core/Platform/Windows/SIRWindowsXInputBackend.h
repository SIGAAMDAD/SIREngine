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

#ifndef __SIRENGINE_SIRWINDOWSXINPUTBACKEND_H__
#define __SIRENGINE_SIRWINDOWSXINPUTBACKEND_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Input/SIRInputBackend.h>
#include <Core/Input/SIRInputService.h>

typedef struct sirXInputNativeSlot_s
{
	sirInputDeviceId_t id;
	uint32_t nPacket;
	bool bConnected;
	bool bForceSample;
} sirXInputNativeSlot_t;

// Native XInput compatibility backend. Future GameInput/HID backends must
// resolve physical device identity through the registry before connecting.
class SIRWindowsXInputBackend final : public SIRInputBackend
{
public:
	SIRWindowsXInputBackend( void ) noexcept;
	sirInputBackendId_t id( void ) const noexcept override;
	bool initialize( void ) noexcept;
	void sample( SIRInputService& service ) noexcept override;
	void resynchronize( SIRInputService& service ) noexcept override;
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override;
	uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept override;
	void stop( SIRInputService& service ) noexcept override;

private:
	void *m_pLibrary;
	void *m_pGetState;
	void *m_pSetState;
	sirXInputNativeSlot_t m_Slots[4];
};

#endif // __SIRENGINE_SIRWINDOWSXINPUTBACKEND_H__
