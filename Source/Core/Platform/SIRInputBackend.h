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

#ifndef __SIRENGINE_SIRINPUTBACKEND_H__
#define __SIRENGINE_SIRINPUTBACKEND_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Platform/SIRInput.h>

typedef enum sirInputBackendKind_e : uint8_t
{
	SIR_INPUT_BACKEND_WINDOWS_RAW = 1,
	SIR_INPUT_BACKEND_WINDOWS_XINPUT = 2,
	SIR_INPUT_BACKEND_WINDOWS_GAMEINPUT = 3,
	SIR_INPUT_BACKEND_WINDOWS_HID = 4,
	SIR_INPUT_BACKEND_LINUX_X11 = 5,
	SIR_INPUT_BACKEND_LINUX_WAYLAND = 6,
	SIR_INPUT_BACKEND_LINUX_EVDEV = 7,
	SIR_INPUT_BACKEND_LINUX_HIDRAW = 8,
	SIR_INPUT_BACKEND_SDL3 = 9
} sirInputBackendKind_t;

// ApplicationCore-style interface: one collector owns its native resources
// and delivers events to the owner-thread SIRInputSystem. The coordinator owns
// registration/lifecycle order, not backend object allocation.
// A zero physical key means identity is unknown and must not be fabricated.
class SIRInputBackend
{
public:
	virtual ~SIRInputBackend() = default;
	virtual sirInputBackendKind_t getBackendKind( void ) const noexcept = 0;
	virtual uint32_t pump( void ) noexcept = 0;
	virtual bool ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept = 0;
	virtual uint64_t getPhysicalKey( sirInputDeviceHandle_t hDevice ) const noexcept;
	virtual bool setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept;
	virtual void shutdown( void ) noexcept = 0;
};

#endif // __SIRENGINE_SIRINPUTBACKEND_H__
