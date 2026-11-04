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

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputTypes.h>
#include <Core/Platform/Input/SIRInputHaptics.h>
class SIRInputService;

// UE ApplicationCore-style native I/O owner. Lifetime is explicit and borrowed;
// one backend owns each native device and its hardware output endpoint.
class SIRInputBackend
{
public:
	virtual ~SIRInputBackend() = default;
	virtual sirInputBackendId_t id( void ) const noexcept = 0;
	virtual void sample( SIRInputService &service ) noexcept = 0;
	virtual void resynchronize( SIRInputService &service ) noexcept = 0;
	virtual bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept = 0;
	virtual uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept;
	virtual bool vibrateTriggers( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept;
	virtual void stop( SIRInputService &service ) noexcept = 0;
};

#endif // __SIRENGINE_SIRINPUTBACKEND_H__
