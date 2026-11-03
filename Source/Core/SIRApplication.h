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

#ifndef __SIRENGINE_SIRAPPLICATION_H__
#define __SIRENGINE_SIRAPPLICATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventRegistry.h>

typedef enum sirAppState_e : uint8_t
{
	SIR_APP_UNINITIALIZED = 0,
	SIR_APP_READY = 1,
	SIR_APP_STOPPED = 2
} sirAppState_t;

class SIRApplication
{
private:
	sirAppState_t m_eState;
	uint64_t m_nTicks;
	SIREventRegistry m_EventRegistry;

public:
	SIRApplication( void ) noexcept;
	bool initialize( void ) noexcept;
	bool tick( void ) noexcept;
	void shutdown( void ) noexcept;
	sirAppState_t getState( void ) const noexcept;
	uint64_t getTickCount( void ) const noexcept;
	SIREventRegistry &getEventRegistry( void ) noexcept;
};

#endif // __SIRENGINE_SIRAPPLICATION_H__
