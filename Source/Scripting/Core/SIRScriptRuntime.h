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

#ifndef __SIRENGINE_SIRSCRIPTRUNTIME_H__
#define __SIRENGINE_SIRSCRIPTRUNTIME_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Core/SIRScriptBindingRegistry.h>
#include <Scripting/Core/SIRScriptIngress.h>
#include <Scripting/Core/SIRScriptScheduler.h>

#include <thread>

typedef struct sirScriptBackendSlot_s
{
	uint64_t nBackendId;
	sirScriptBackendAPI_t api;
	bool bRegistered;
	bool bStarted;
} sirScriptBackendSlot_t;

typedef struct sirScriptProgramSlot_s
{
	uint64_t nProgramId;
	uint64_t nRevision;
	uint64_t nBackendToken;
	uint64_t nLeaseToken;
	uint32_t nGeneration;
	uint32_t nReferences;
	uint8_t iBackend;
	bool bActive;
} sirScriptProgramSlot_t;

typedef struct sirScriptInstanceSlot_s
{
	uint64_t nBackendToken;
	uint64_t nOwnerId;
	uint32_t nGeneration;
	uint32_t iProgram;
	bool bActive;
	bool bInvoking;
} sirScriptInstanceSlot_t;

typedef struct sirScriptRuntimeFootprint_s
{
	uint64_t nRuntimeBytes;
	uint64_t nSchedulerBytes;
	uint64_t nIngressBytes;
	uint64_t nExternalPayloadBytes;
	uint64_t nPeakExternalPayloadBytes;
	uint32_t nLiveExternalPayloads;
	uint32_t nScheduledCalls;
	uint32_t nIngressCalls;
} sirScriptRuntimeFootprint_t;

typedef struct sirScriptSubscriptionSlot_s
{
	sirScriptInstanceHandle_t instance;
	uint64_t nEventTypeId;
	uint64_t nMethodId;
	uint32_t nGeneration;
	bool bActive;
} sirScriptSubscriptionSlot_t;

// Single-owner scripting control plane. Backends own their private VM heaps,
// stacks and managed roots. The fixed arrays are allocation-free bootstrap
// capacity, not a game-wide scripting budget or a concurrency claim.
class SIRScriptRuntime
{
public:
	explicit SIRScriptRuntime( const SIRScriptBindingRegistry *pRegistry ) noexcept;

	sirScriptResult_t setProgramSource( const sirScriptProgramSourceAPI_t &api ) noexcept;
	sirScriptResult_t clearProgramSource( void ) noexcept;
	sirScriptResult_t registerBackend( uint64_t nBackendId, const sirScriptBackendAPI_t &api ) noexcept;
	sirScriptResult_t startBackend( uint64_t nBackendId ) noexcept;
	sirScriptResult_t stopBackend( uint64_t nBackendId ) noexcept;
	sirScriptResult_t unregisterBackend( uint64_t nBackendId ) noexcept;
	sirScriptResult_t shutdownAll( void ) noexcept;
	sirScriptResult_t loadProgram( uint64_t nBackendId, uint64_t nProgramId, uint64_t nRevision,
	                               sirScriptProgramHandle_t *pHandle ) noexcept;
	sirScriptResult_t unloadProgram( sirScriptProgramHandle_t handle ) noexcept;
	sirScriptResult_t createInstance( sirScriptProgramHandle_t program, uint64_t nOwnerId, sirScriptInstanceHandle_t *pHandle ) noexcept;
	sirScriptResult_t destroyInstance( sirScriptInstanceHandle_t handle ) noexcept;
	sirScriptResult_t invoke( sirScriptInstanceHandle_t handle, uint64_t nMethodId, sirScriptReadFrame_t arguments,
	                          sirScriptWriteFrame_t result ) noexcept;
	sirScriptResult_t subscribe( sirScriptInstanceHandle_t instance, uint64_t nEventTypeId, uint64_t nMethodId,
	                             sirScriptSubscriptionHandle_t *pHandle ) noexcept;
	sirScriptResult_t unsubscribe( sirScriptSubscriptionHandle_t handle ) noexcept;
	sirScriptResult_t publishEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload ) noexcept;
	sirScriptResult_t enqueueEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload, sirScriptPhase_t ePhase,
	                                sirScriptTaskHandle_t *pHandle ) noexcept;
	sirScriptResult_t enqueueInvoke( sirScriptInstanceHandle_t instance, uint64_t nMethodId, uint64_t nArgumentsTypeId,
	                                 sirScriptReadFrame_t arguments, sirScriptPhase_t ePhase, sirScriptTaskHandle_t *pHandle ) noexcept;
	sirScriptResult_t cancelScheduled( sirScriptTaskHandle_t handle ) noexcept;
	sirScriptResult_t setPayloadAllocator( const sirScriptPayloadAllocatorAPI_t &api ) noexcept;
	// Cross-thread: published registry and runtime must outlive every producer.
	sirScriptResult_t postEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload, sirScriptPhase_t ePhase ) noexcept;
	sirScriptResult_t pumpIngress( sirScriptPhase_t ePhase, uint32_t nBudget, uint32_t *pProcessed ) noexcept;
	sirScriptRuntimeFootprint_t queryMemoryFootprint( void ) const noexcept;
	sirScriptResult_t pumpPhase( sirScriptPhase_t ePhase, uint32_t nBudget, uint32_t *pProcessed, uint32_t *pStale ) noexcept;
	uint32_t pendingCount( sirScriptPhase_t ePhase ) const noexcept;

private:
	static constexpr uint32_t MAX_BACKENDS = 8;
	static constexpr uint32_t MAX_PROGRAMS = 64;
	static constexpr uint32_t MAX_INSTANCES = 128;
	static constexpr uint32_t MAX_SUBSCRIPTIONS = 256;

	bool isOwnerThread( void ) const noexcept;
	static uint32_t nextGeneration( uint32_t nGeneration ) noexcept;
	static bool validFrame( const void *pData, uint32_t nBytes, uint32_t nAlignment ) noexcept;
	static bool validAPI( const sirScriptBackendAPI_t &api ) noexcept;
	static bool validProgramView( sirScriptProgramView_t view, uint64_t nProgramId, uint64_t nRevision ) noexcept;
	int32_t findBackend( uint64_t nBackendId ) const noexcept;
	int32_t resolveProgram( sirScriptProgramHandle_t handle ) const noexcept;
	int32_t resolveInstance( sirScriptInstanceHandle_t handle ) const noexcept;
	bool hasLivePrograms( uint32_t iBackend ) const noexcept;
	void invalidateSubscriptions( sirScriptInstanceHandle_t handle ) noexcept;

	const SIRScriptBindingRegistry *m_pRegistry;
	sirScriptProgramSourceAPI_t m_ProgramSource;
	const std::thread::id m_OwnerThread;
	SIRScriptScheduler m_Scheduler;
	SIRScriptIngress m_Ingress;
	sirScriptBackendSlot_t m_szBackends[MAX_BACKENDS];
	sirScriptProgramSlot_t m_szPrograms[MAX_PROGRAMS];
	sirScriptInstanceSlot_t m_szInstances[MAX_INSTANCES];
	sirScriptSubscriptionSlot_t m_szSubscriptions[MAX_SUBSCRIPTIONS];
	bool m_bDispatching;
	bool m_bCallingBackend;
	bool m_bPumping;
	bool m_bShuttingDown;
};

#endif // __SIRENGINE_SIRSCRIPTRUNTIME_H__
