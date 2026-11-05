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

#ifndef __SIRENGINE_SIRSCRIPTSCHEDULER_H__
#define __SIRENGINE_SIRSCRIPTSCHEDULER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Public/SIRScriptTypes.h>

typedef enum sirScriptPhase_e : uint8_t
{
	SIR_SCRIPT_PHASE_EARLY = 0,
	SIR_SCRIPT_PHASE_LATE = 1,
	SIR_SCRIPT_PHASE_COUNT = 2
} sirScriptPhase_t;

typedef enum sirScriptScheduledKind_e : uint8_t
{
	SIR_SCRIPT_SCHEDULED_EVENT = 0,
	SIR_SCRIPT_SCHEDULED_INVOKE = 1
} sirScriptScheduledKind_t;

typedef struct sirScriptTaskHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptTaskHandle_t;

// The allocator must be backed by an engine-owned, individually reclaimable
// memory domain. An allocation token is opaque, nonzero, and paired
// with the pointer returned by the provider. Callbacks run on the owner thread.
typedef struct sirScriptPayloadAllocation_s
{
	void *pData;
	uint64_t nToken;
} sirScriptPayloadAllocation_t;

typedef struct sirScriptPayloadAllocatorAPI_s
{
	uint32_t nABIRevision;
	void *pUser;
	uint32_t nMaximumBytes;
	uint32_t nMaximumAlignment;
	sirScriptResult_t ( *pAllocate )( void *, uint32_t, uint32_t, sirScriptPayloadAllocation_t * ) noexcept;
	void ( *pRelease )( void *, sirScriptPayloadAllocation_t ) noexcept;
} sirScriptPayloadAllocatorAPI_t;

// Small calls remain inline. Overflow bytes belong to one live allocator
// lease until cancel, complete dispatch, or shutdown. Copying the envelope
// does NOT copy ownership; only the scheduler transfers the lease.
typedef struct sirScriptScheduledCall_s
{
	sirScriptInstanceHandle_t instance;
	uint64_t nDispatchId;
	uint32_t nBytes;
	uint32_t nAlignment;
	sirScriptScheduledKind_t eKind;
	sirScriptPayloadAllocation_t allocation;
	alignas( 64 ) unsigned char szPayload[256];
} sirScriptScheduledCall_t;

typedef struct sirScriptScheduledSlot_s
{
	sirScriptScheduledCall_t call;
	uint32_t nNext;
	uint32_t nGeneration;
	uint64_t nSequence;
	bool bActive;
} sirScriptScheduledSlot_t;

// Single-owner bounded FIFO. Cancellation immediately releases capacity;
// pumping removes a task before calling a backend, so tasks enqueued by
// callbacks cannot execute until a subsequent pump of that phase.
class SIRScriptScheduler
{
public:
	SIRScriptScheduler( void ) noexcept;
	~SIRScriptScheduler( void ) noexcept;

	sirScriptResult_t setPayloadAllocator( const sirScriptPayloadAllocatorAPI_t& api ) noexcept;
	sirScriptResult_t enqueue( sirScriptPhase_t ePhase, const sirScriptScheduledCall_t& call, const void *pPayload,
	                           sirScriptTaskHandle_t *pHandle ) noexcept;
	void releasePayload( sirScriptScheduledCall_t *pCall ) noexcept;
	uint64_t allocatedPayloadBytes( void ) const noexcept;
	uint64_t peakPayloadBytes( void ) const noexcept;
	uint32_t livePayloadCount( void ) const noexcept;
	sirScriptResult_t cancel( sirScriptTaskHandle_t handle ) noexcept;
	bool dequeue( sirScriptPhase_t ePhase, uint64_t nLastSequence, sirScriptScheduledCall_t *pCall ) noexcept;
	uint64_t lastSequence( void ) const noexcept;
	void clear( void ) noexcept;
	uint32_t pendingCount( sirScriptPhase_t ePhase ) const noexcept;

private:
	static constexpr uint32_t MAX_TASKS = 64;
	static uint32_t nextGeneration( uint32_t nGeneration ) noexcept;
	void release( uint32_t nSlot ) noexcept;

	sirScriptPayloadAllocatorAPI_t m_Allocator;
	uint64_t m_nAllocatedPayloadBytes;
	uint64_t m_nPeakPayloadBytes;
	uint32_t m_nLivePayloads;
	sirScriptScheduledSlot_t m_szSlots[MAX_TASKS + 1];
	uint32_t m_szHeads[SIR_SCRIPT_PHASE_COUNT];
	uint32_t m_szTails[SIR_SCRIPT_PHASE_COUNT];
	uint32_t m_szCounts[SIR_SCRIPT_PHASE_COUNT];
	uint32_t m_nFreeHead;
	uint64_t m_nSequence;
};

#endif // __SIRENGINE_SIRSCRIPTSCHEDULER_H__
