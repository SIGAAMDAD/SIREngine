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

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventRegistry.h>
#include <Core/Systems/SIREngineSystem.h>
#include <Core/Input/SIRInputService.h>
#include <Core/Scheduling/SIRSimulationSchedule.h>
#include <Core/Scheduling/SIRTimelineCoordinator.h>

// Legacy input-attached pump is preserved for existing Input tests.
// The window owner uses a single unconditional pump, including headless/no-input applications.
typedef bool ( *sirAppInputPump_t )( SIRInputService& input, void *pContext );
typedef enum sirAppWindowPumpResult_e : uint8_t
{
	SIR_APP_WINDOW_CONTINUE = 0,
	SIR_APP_WINDOW_STOP = 1,
	SIR_APP_WINDOW_ERROR = 2
} sirAppWindowPumpResult_t;
typedef sirAppWindowPumpResult_t ( *sirAppWindowPump_t )( void *pContext );

typedef enum sirAppRunResult_e : uint8_t
{
	SIR_APP_RUN_OK = 0,
	SIR_APP_RUN_STOPPED = 1,
	SIR_APP_RUN_ERROR = 2
} sirAppRunResult_t;

constexpr uint32_t SIR_APPLICATION_MAX_SYSTEMS = 16;
constexpr uint32_t SIR_APPLICATION_MAX_SYSTEM_ACCESSES = 8;
constexpr uint32_t SIR_APPLICATION_MAX_SYSTEM_EDGES = SIR_APPLICATION_MAX_SYSTEMS * SIR_APPLICATION_MAX_SYSTEMS;

typedef enum sirAppState_e : uint8_t
{
	SIR_APP_UNINITIALIZED = 0,
	SIR_APP_READY = 1,
	SIR_APP_STOPPED = 2,
	SIR_APP_STARTING = 3,
	SIR_APP_STOPPING = 4
} sirAppState_t;

typedef enum sirAppScheduledState_e : uint8_t
{
	SIR_APP_SCHEDULE_IDLE = 0,
	SIR_APP_SCHEDULE_PENDING = 1,
	SIR_APP_SCHEDULE_SUCCEEDED = 2,
	SIR_APP_SCHEDULE_FAILED = 3
} sirAppScheduledState_t;

typedef struct sirAppScheduledContext_s
{
	SIREngineSystem *pSystem;
	float fDeltaSeconds;
	bool bFixed;
} sirAppScheduledContext_t;

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
	bool attachWindowPump( sirAppWindowPump_t pPump, void *pContext = nullptr ) noexcept;
	sirAppWindowPumpResult_t getWindowPumpResult( void ) const noexcept;
	bool attachTimeline( SIRTimelineCoordinator& timeline ) noexcept;
	bool attachTaskSchedule( SIRSimulationSchedule& schedule ) noexcept;
	// Opted-in systems declare their complete hazards. Unconfigured systems
	// remain ordered against every registered system in a scheduled phase.
	bool configureScheduledSystem( uint32_t iSystem, sirTaskExecutor_t eExecutor, const sirTaskGraphAccess_t *pAccesses,
	                               uint32_t nAccesses ) noexcept;
	bool configureScheduledPriority( uint32_t iSystem, sirTaskPriority_t ePriority ) noexcept;
	bool addScheduledDependency( uint32_t iBefore, uint32_t iAfter ) noexcept;
	// Nonblocking submitted phase. MAIN and zero-worker CPU tasks may be pumped
	// from pollScheduledTick; provider-affine callbacks run on their own owner.
	bool beginScheduledTick( float fDeltaSeconds, bool bFixed ) noexcept;
	sirAppScheduledState_t pollScheduledTick( void ) noexcept;
	bool cancelScheduledTick( void ) noexcept;
	bool registerSystem( SIREngineSystem *pSystem ) noexcept;
	// Both systems must already be registered. Duplicate/self dependencies are rejected;
	// cycles are rejected before any system is initialized.
	bool addDependency( SIREngineSystem& consumer, SIREngineSystem& provider ) noexcept;
	bool initialize( void ) noexcept;
	bool tick( float fDeltaSeconds = 0.0f ) noexcept;
	bool tickTimed( void ) noexcept;
	sirAppRunResult_t run( uint64_t nMaxFrames = 0 ) noexcept;
	bool fixedTick( float fFixedDeltaSeconds ) noexcept;
	sirSystemResult_t shutdown( void ) noexcept;

protected:
private:
	static sirTaskResult_t scheduledUpdateEntry( void *pContext, sirTaskHandle_t task ) noexcept;
	bool publishScheduledInput( uint64_t nNextTick, bool *pbPumped ) noexcept;
	bool openTimelineFrame( float fDeltaSeconds ) noexcept;
	bool finishTimelineFrame( void ) noexcept;

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
	sirAppWindowPump_t m_pWindowPump;
	void *m_pWindowPumpContext;
	sirAppWindowPumpResult_t m_eWindowPumpResult;
	SIRTimelineCoordinator *m_pTimeline;
	SIRSimulationSchedule *m_pSchedule;
	sirTaskExecutor_t m_szSystemExecutors[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirTaskPriority_t m_szSystemPriorities[SIR_APPLICATION_MAX_SYSTEMS] = {};
	bool m_szSystemConfigured[SIR_APPLICATION_MAX_SYSTEMS] = {};
	uint32_t m_szSystemAccessCounts[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirTaskGraphAccess_t m_szSystemAccesses[SIR_APPLICATION_MAX_SYSTEMS][SIR_APPLICATION_MAX_SYSTEM_ACCESSES] = {};
	sirAppScheduledContext_t m_szScheduledContexts[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirSimulationPassDesc_t m_szScheduledPasses[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirTaskGraphEdge_t m_szScheduledEdges[SIR_APPLICATION_MAX_SYSTEM_EDGES * 3] = {};
	sirTaskGraphEdge_t m_szExplicitEdges[SIR_APPLICATION_MAX_SYSTEM_EDGES] = {};
	uint32_t m_nExplicitEdges = 0;
	float m_fScheduledDeltaSeconds;
	bool m_bScheduledFixed;
	bool m_bScheduledActive;

	uint32_t findSystemIndex( const SIREngineSystem *pSystem ) const noexcept;
	bool resolveInitializationOrder( void ) noexcept;
	bool tickFrame( float fDeltaSeconds ) noexcept;
};

#endif // __SIRENGINE_SIRAPPLICATION_H__
