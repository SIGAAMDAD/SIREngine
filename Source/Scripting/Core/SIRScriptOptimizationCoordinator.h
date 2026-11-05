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

#ifndef __SIRENGINE_SIRSCRIPTOPTIMIZATIONCOORDINATOR_H__
#define __SIRENGINE_SIRSCRIPTOPTIMIZATIONCOORDINATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptOptimizationProtocol.h>
#include <Scripting/Core/SIRScriptCachedProgramSource.h>
#include <thread>

// Transport owns its process and full-framed IPC. Each receive returns either
// one complete 200-byte reply, BUSY for no reply, or a terminal error. Sending
// is likewise atomic for one complete 160-byte request. Callbacks are invoked
// only by the scripting owner lane, never concurrently or recursively.
static constexpr uint32_t SIR_SCRIPT_OPT_TRANSPORT_ABI_REVISION = 1;
typedef struct sirScriptOptimizationTransportAPI_s
{
	uint32_t nABIRevision;
	void *pUser;
	sirScriptResult_t ( *pStart )( void *pUser ) noexcept;
	void ( *pStop )( void *pUser ) noexcept;
	sirScriptResult_t ( *pSend )( void *pUser, const uint8_t *pWire, uint32_t nBytes ) noexcept;
	sirScriptResult_t ( *pReceive )( void *pUser, uint8_t *pWire, uint32_t nCapacity, uint32_t *pReceived ) noexcept;
} sirScriptOptimizationTransportAPI_t;

typedef enum sirScriptOptimizationJobState_e : uint8_t
{
	SIR_SCRIPT_JOB_EMPTY = 0,
	SIR_SCRIPT_JOB_QUEUED = 1,
	SIR_SCRIPT_JOB_IN_FLIGHT = 2,
	SIR_SCRIPT_JOB_COMPLETED = 3,
	SIR_SCRIPT_JOB_FAILED = 4,
	SIR_SCRIPT_JOB_CANCELED = 5
} sirScriptOptimizationJobState_t;

typedef struct sirScriptOptimizationJobHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptOptimizationJobHandle_t;

typedef struct sirScriptOptimizationJobInfo_s
{
	uint64_t nJobId;
	sirScriptOptimizationJobState_t eState;
	sirScriptResult_t eResult;
	sirScriptCodeHandle_t code;
	sirScriptCachedEntryHandle_t entry;
	bool bAwaitingReply;
} sirScriptOptimizationJobInfo_t;

typedef struct sirScriptOptimizationJobSlot_s
{
	sirScriptOptimizationRequest_t request;
	sirScriptOptimizationJobInfo_t info;
	uint64_t nSequence;
	uint32_t nGeneration;
	bool bOccupied;
	bool bActivate;
} sirScriptOptimizationJobSlot_t;

typedef struct sirScriptOptimizationPumpStats_s
{
	uint32_t nSent;
	uint32_t nReceived;
	uint32_t nCompleted;
	uint32_t nFailed;
	uint32_t nIgnoredCanceled;
} sirScriptOptimizationPumpStats_t;

// Bounded owner-thread control plane for a separately hosted compiler/optimizer.
// Transport callbacks handle real child process/pipes; this layer owns job
// identity, state transitions, validation, and cache-to-runtime activation.
class SIRScriptOptimizationCoordinator
{
public:
	SIRScriptOptimizationCoordinator( void ) noexcept;
	SIRScriptOptimizationCoordinator( const SIRScriptOptimizationCoordinator& ) = delete;
	~SIRScriptOptimizationCoordinator( void ) noexcept;

	SIRScriptOptimizationCoordinator& operator=( const SIRScriptOptimizationCoordinator& ) = delete;

	sirScriptResult_t configure( const sirScriptOptimizationTransportAPI_t& transport, SIRScriptCachedProgramSource *pSource,
	                             SIRScriptCodeManager *pManager ) noexcept;
	sirScriptResult_t start( void ) noexcept;
	sirScriptResult_t submit( const sirScriptOptimizationRequest_t& request, bool bActivate,
	                          sirScriptOptimizationJobHandle_t *pHandle ) noexcept;
	sirScriptResult_t cancel( sirScriptOptimizationJobHandle_t handle ) noexcept;
	sirScriptResult_t query( sirScriptOptimizationJobHandle_t handle, sirScriptOptimizationJobInfo_t *pInfo ) const noexcept;
	sirScriptResult_t release( sirScriptOptimizationJobHandle_t handle ) noexcept;
	sirScriptResult_t pump( uint32_t nSendBudget, uint32_t nReceiveBudget, sirScriptOptimizationPumpStats_t *pStats ) noexcept;
	sirScriptResult_t workerFailed( void ) noexcept;
	// Retry only after the crashed worker has stopped; keep the public handle,
	// but issue a new wire ID so stale replies cannot match the retried job.
	sirScriptResult_t retryFailed( sirScriptOptimizationJobHandle_t handle ) noexcept;
	sirScriptResult_t shutdown( void ) noexcept;

protected:
private:
	static constexpr uint32_t MAX_JOBS = 64;
	static uint32_t nextGeneration( uint32_t nGeneration ) noexcept;
	bool isOwnerThread( void ) const noexcept;
	int32_t resolve( sirScriptOptimizationJobHandle_t handle ) const noexcept;
	int32_t findByJobId( uint64_t nJobId ) const noexcept;
	void failWorker( void ) noexcept;
	void processReply( const sirScriptOptimizationReply_t& reply, sirScriptOptimizationPumpStats_t *pStats ) noexcept;
	void failJob( sirScriptOptimizationJobSlot_t& slot, sirScriptResult_t eResult ) noexcept;

	sirScriptOptimizationTransportAPI_t m_Transport;
	SIRScriptCachedProgramSource *m_pSource;
	SIRScriptCodeManager *m_pManager;
	const std::thread::id m_OwnerThread;
	uint64_t m_nNextJobId;
	uint64_t m_nNextSequence;
	sirScriptOptimizationJobSlot_t m_szJobs[MAX_JOBS];
	bool m_bConfigured;
	bool m_bRunning;
	bool m_bInCallback;
};

#endif // __SIRENGINE_SIRSCRIPTOPTIMIZATIONCOORDINATOR_H__
