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
#include <Core/Systems/SIREngineSystem.h>

constexpr uint32_t SIR_APPLICATION_MAX_SYSTEMS = 16;

typedef enum sirAppState_e : uint8_t
{
	SIR_APP_UNINITIALIZED = 0,
	SIR_APP_READY = 1,
	SIR_APP_STOPPED = 2,
	SIR_APP_STARTING = 3,
	SIR_APP_STOPPING = 4
} sirAppState_t;

// Registered systems are borrowed and must outlive application shutdown.
// Registration order determines initialization and frame-update order;
// successfully initialized systems shut down in reverse order.
class SIRApplication
{
public:
	SIRApplication( void ) noexcept;

	sirAppState_t getState( void ) const noexcept;
	uint64_t getTickCount( void ) const noexcept;
	uint32_t getSystemCount( void ) const noexcept;
	SIREventRegistry& getEventRegistry( void ) noexcept;

	bool registerSystem( SIREngineSystem *pSystem ) noexcept;
	bool initialize( void ) noexcept;
	bool tick( float fDeltaSeconds = 0.0f ) noexcept;
	bool fixedTick( float fFixedDeltaSeconds ) noexcept;
	sirSystemResult_t shutdown( void ) noexcept;

protected:
private:
	sirAppState_t m_eState;
	uint64_t m_nTicks;
	uint32_t m_nSystemCount;
	uint32_t m_nInitializedSystems;
	SIREngineSystem *m_szSystems[SIR_APPLICATION_MAX_SYSTEMS] = {};
	SIREventRegistry m_EventRegistry;
};

#endif // __SIRENGINE_SIRAPPLICATION_H__
