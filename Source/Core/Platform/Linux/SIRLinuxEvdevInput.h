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

#ifndef __SIRENGINE_SIRLINUXEVDEVINPUT_H__
#define __SIRENGINE_SIRLINUXEVDEVINPUT_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Core/Platform/SIRInput.h>

// Nonblocking evdev controller backend. The caller owns discovery timing and
// calls pump() from the SIRInputSystem owner thread.
class SIRLinuxEvdevInput
{
public:
	explicit SIRLinuxEvdevInput( SIRInputSystem &input ) noexcept;
	SIRLinuxEvdevInput( const SIRLinuxEvdevInput & ) = delete;
	SIRLinuxEvdevInput &operator=( const SIRLinuxEvdevInput & ) = delete;
	~SIRLinuxEvdevInput();

	bool openDevice( const char *pszPath ) noexcept;
	uint32_t discover( const char *pszDirectory = "/dev/input" ) noexcept;
	uint32_t pump( void ) noexcept;
	uint32_t getDeviceCount( void ) const noexcept;
	void shutdown( void ) noexcept;

private:
	typedef struct sirEvdevSlot_s
	{
		sirInputDeviceHandle_t hDevice;
		int nFd;
		uint64_t nDeviceId;
		bool bDesynchronized;
	} sirEvdevSlot_t;

	bool resynchronize( sirEvdevSlot_t &slot ) noexcept;
	void closeSlot( sirEvdevSlot_t &slot ) noexcept;

	SIRInputSystem &m_Input;
	sirEvdevSlot_t m_Devices[SIR_INPUT_MAX_DEVICES];
	uint32_t m_nDeviceCount;
};

#endif // __SIRENGINE_SIRLINUXEVDEVINPUT_H__
