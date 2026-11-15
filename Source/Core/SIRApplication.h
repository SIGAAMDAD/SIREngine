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
#include <Core/Platform/Input/SIRInputService.h>

// The native window owner dispatches events only between input begin/publish.
typedef bool ( *sirAppInputPump_t )( SIRInputService& input, void *pContext );

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
// Dependencies are declared after registration, without allocating memory.
// Independent systems retain registration order. Dependencies initialize
// before their consumers, and initialized systems shut down in reverse order.
class SIRApplication
{
public:
	SIRApplication( void ) noexcept;

	sirAppState_t getState( void ) const noexcept;
	sirSystemResult_t getInitializationResult( void ) const noexcept;
	const SIREngineSystem *getFailedSystem( void ) const noexcept;
	uint64_t getTickCount( void ) const noexcept;
	float getFrameDeltaSeconds( void ) const noexcept;
	double getElapsedSeconds( void ) const noexcept;
	uint32_t getSystemCount( void ) const noexcept;
	SIREventRegistry& getEventRegistry( void ) noexcept;
	const sirInputFrame_t *getInputFrame( void ) const noexcept;

	bool attachInput( SIRInputService& input, sirAppInputPump_t pPump = nullptr, void *pContext = nullptr ) noexcept;
	bool registerSystem( SIREngineSystem *pSystem ) noexcept;
	// Both systems must already be registered. Duplicate/self dependencies are rejected;
	// cycles are rejected before any system is initialized.
	bool addDependency( SIREngineSystem& consumer, SIREngineSystem& provider ) noexcept;
	bool initialize( void ) noexcept;
	bool tick( float fDeltaSeconds = 0.0f ) noexcept;
	bool tickTimed( void ) noexcept;
	bool fixedTick( float fFixedDeltaSeconds ) noexcept;
	sirSystemResult_t shutdown( void ) noexcept;

protected:
private:
	sirAppState_t m_eState;
	uint64_t m_nTicks;
	uint64_t m_nLastFrameTimestampNs;
	float m_fFrameDeltaSeconds;
	double m_dElapsedSeconds;
	uint32_t m_nSystemCount;
	uint32_t m_nInitializedSystems;
	SIREngineSystem *m_szSystems[SIR_APPLICATION_MAX_SYSTEMS] = {};
	uint32_t m_szDependencyMasks[SIR_APPLICATION_MAX_SYSTEMS] = {};
	uint32_t m_szInitializationOrder[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirSystemResult_t m_eInitializationResult;
	const SIREngineSystem *m_pFailedSystem;
	bool m_bShutdownInProgress;
	bool m_bFrameDispatchInProgress;
	SIREventRegistry m_EventRegistry;
	SIRInputService *m_pInput;
	sirAppInputPump_t m_pInputPump;
	void *m_pInputPumpContext;

	uint32_t findSystemIndex( const SIREngineSystem *pSystem ) const noexcept;
	bool resolveInitializationOrder( void ) noexcept;
	bool tickFrame( float fDeltaSeconds ) noexcept;
};

#endif // __SIRENGINE_SIRAPPLICATION_H__
