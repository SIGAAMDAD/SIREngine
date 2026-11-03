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

#ifndef __SIRENGINE_SIRPLATFORMPROCESS_H__
#define __SIRENGINE_SIRPLATFORMPROCESS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

class SIRPlatformProcess
{
public:
	SIRPlatformProcess( void ) = delete;
	~SIRPlatformProcess() = delete;

	static uint32_t getLogicalProcessorCount( void ) noexcept;
	static void cpuRelax( void ) noexcept;
	static void yieldThread( void ) noexcept;
	static void sleepThread( uint32_t nMilliseconds ) noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMPROCESS_H__
