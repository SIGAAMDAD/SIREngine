/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_APPLICATION_H__
#define __SIR_APPLICATION_H__

#include "Core/Memory/Pointers/SIRSharedPtr.h"
#include "Core/Platform/Shared/Window/SIRNativeWindowDriver.h"
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
#include <Core/Containers/SIRStaticVector.h>

// Legacy input-attached pump is preserved for existing Input tests.
// The window owner uses a single unconditional pump, including headless/no-input applications.
typedef bool ( *sirAppInputPump_t )( SIRInputService& input, void *pContext );
enum class SIRAppWindowPumpResult : uint8_t
{
	SIR_APP_WINDOW_CONTINUE = 0,
	SIR_APP_WINDOW_STOP = 1,
	SIR_APP_WINDOW_ERROR = 2
};
typedef SIRAppWindowPumpResult ( *sirAppWindowPump_t )( void *pContext );

enum class SIRAppRunResult : uint8_t
{
	SIR_APP_RUN_OK = 0,
	SIR_APP_RUN_STOPPED = 1,
	SIR_APP_RUN_ERROR = 2
};

constexpr uint32_t SIR_APPLICATION_MAX_SYSTEMS = 16;
constexpr uint32_t SIR_APPLICATION_MAX_SYSTEM_ACCESSES = 8;
constexpr uint32_t SIR_APPLICATION_MAX_SYSTEM_EDGES = SIR_APPLICATION_MAX_SYSTEMS * SIR_APPLICATION_MAX_SYSTEMS;

enum class SIRAppState : uint8_t
{
	SIR_APP_UNINITIALIZED = 0,
	SIR_APP_READY = 1,
	SIR_APP_STOPPED = 2,
	SIR_APP_STARTING = 3,
	SIR_APP_STOPPING = 4
};

enum class SIRAppScheduledState : uint8_t
{
	SIR_APP_SCHEDULE_IDLE = 0,
	SIR_APP_SCHEDULE_PENDING = 1,
	SIR_APP_SCHEDULE_SUCCEEDED = 2,
	SIR_APP_SCHEDULE_FAILED = 3
};

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

	SIRAppState getState( void ) const noexcept;
	SIRSystemResult getInitializationResult( void ) const noexcept;
	const SIREngineSystem *getFailedSystem( void ) const noexcept;
	uint64_t getTickCount( void ) const noexcept;
	float getFrameDeltaSeconds( void ) const noexcept;
	double getElapsedSeconds( void ) const noexcept;
	uint32_t getSystemCount( void ) const noexcept;
	SIREventRegistry& getEventRegistry( void ) noexcept;
	const sirInputFrame_t *getInputFrame( void ) const noexcept;

	bool attachInput( SIRInputService& input, sirAppInputPump_t pPump = nullptr, void *pContext = nullptr ) noexcept;
	bool attachWindowPump( sirAppWindowPump_t pPump, void *pContext = nullptr ) noexcept;
	SIRAppWindowPumpResult getWindowPumpResult( void ) const noexcept;
	bool attachTimeline( SIRTimelineCoordinator& timeline ) noexcept;
	bool attachTaskSchedule( SIRSimulationSchedule& schedule ) noexcept;
	// Opted-in systems declare their complete hazards. Unconfigured systems
	// remain ordered against every registered system in a scheduled phase.
	bool configureScheduledSystem( uint32_t iSystem, SIRTaskExecutor eExecutor, const sirTaskGraphAccess_t *pAccesses, uint32_t nAccesses ) noexcept;
	bool configureScheduledPriority( uint32_t iSystem, SIRTaskPriority ePriority ) noexcept;
	bool addScheduledDependency( uint32_t iBefore, uint32_t iAfter ) noexcept;
	// Nonblocking submitted phase. MAIN and zero-worker CPU tasks may be pumped
	// from pollScheduledTick; provider-affine callbacks run on their own owner.
	bool beginScheduledTick( float fDeltaSeconds, bool bFixed ) noexcept;
	SIRAppScheduledState pollScheduledTick( void ) noexcept;
	bool cancelScheduledTick( void ) noexcept;
	bool registerSystem( SIREngineSystem *pSystem ) noexcept;
	// Both systems must already be registered. Duplicate/self dependencies are rejected;
	// cycles are rejected before any system is initialized.
	bool addDependency( SIREngineSystem& consumer, SIREngineSystem& provider ) noexcept;
	bool initialize( void ) noexcept;
	bool tick( float fDeltaSeconds = 0.0f ) noexcept;
	bool tickTimed( void ) noexcept;
	SIRAppRunResult run( uint64_t nMaxFrames = 0 ) noexcept;
	bool fixedTick( float fFixedDeltaSeconds ) noexcept;
	SIRSystemResult shutdown( void ) noexcept;

protected:
private:
	static SIRTaskResult scheduledUpdateEntry( void *pContext, sirTaskHandle_t task ) noexcept;
	bool publishScheduledInput( uint64_t nNextTick, bool *pbPumped ) noexcept;
	bool openTimelineFrame( float fDeltaSeconds ) noexcept;
	bool finishTimelineFrame( void ) noexcept;

	SIRLocalSharedPtr<SIRNativeWindowDriver> m_WindowDriver;

	SIRAppState m_eState;
	uint64_t m_nTicks;
	uint64_t m_nLastFrameTimestampNs;
	float m_fFrameDeltaSeconds;
	double m_dElapsedSeconds;
	uint32_t m_nSystemCount;
	uint32_t m_nInitializedSystems;
	SIREngineSystem *m_szSystems[SIR_APPLICATION_MAX_SYSTEMS] = {};
	uint32_t m_szDependencyMasks[SIR_APPLICATION_MAX_SYSTEMS] = {};
	uint32_t m_szInitializationOrder[SIR_APPLICATION_MAX_SYSTEMS] = {};
	SIRSystemResult m_eInitializationResult;
	const SIREngineSystem *m_pFailedSystem;
	bool m_bShutdownInProgress;
	bool m_bFrameDispatchInProgress;
	SIREventRegistry m_EventRegistry;
	SIRInputService *m_pInput;
	sirAppInputPump_t m_pInputPump;
	void *m_pInputPumpContext;
	sirAppWindowPump_t m_pWindowPump;
	void *m_pWindowPumpContext;
	SIRAppWindowPumpResult m_eWindowPumpResult;
	SIRTimelineCoordinator *m_pTimeline;
	SIRSimulationSchedule *m_pSchedule;
	SIRTaskExecutor m_szSystemExecutors[SIR_APPLICATION_MAX_SYSTEMS] = {};
	SIRTaskPriority m_szSystemPriorities[SIR_APPLICATION_MAX_SYSTEMS] = {};
	bool m_szSystemConfigured[SIR_APPLICATION_MAX_SYSTEMS] = {};
	uint32_t m_szSystemAccessCounts[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirTaskGraphAccess_t m_szSystemAccesses[SIR_APPLICATION_MAX_SYSTEMS][SIR_APPLICATION_MAX_SYSTEM_ACCESSES] = {};
	sirAppScheduledContext_t m_szScheduledContexts[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirSimulationPassDesc_t m_szScheduledPasses[SIR_APPLICATION_MAX_SYSTEMS] = {};
	sirTaskGraphEdge_t m_szScheduledEdges[SIR_APPLICATION_MAX_SYSTEM_EDGES * 3] = {};
	// Bounded, append-only dependencies. Size is owned by the container.
	SIRStaticVector<sirTaskGraphEdge_t, SIR_APPLICATION_MAX_SYSTEM_EDGES> m_ExplicitEdges;
	float m_fScheduledDeltaSeconds;
	bool m_bScheduledFixed;
	bool m_bScheduledActive;

	uint32_t findSystemIndex( const SIREngineSystem *pSystem ) const noexcept;
	bool resolveInitializationOrder( void ) noexcept;
	bool tickFrame( float fDeltaSeconds ) noexcept;
};

#endif // __SIR_APPLICATION_H__
