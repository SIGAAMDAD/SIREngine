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

#ifndef __SIRENGINE_SIRTASKGRAPH_H__
#define __SIRENGINE_SIRTASKGRAPH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Jobs/SIRTaskTypes.h>

class SIRMemoryManager;
class SIRTaskScheduler;
typedef struct sirTaskGraphAccessRecord_s sirTaskGraphAccessRecord_t;
typedef struct sirTaskGraphResourceBucket_s sirTaskGraphResourceBucket_t;
typedef struct sirTaskGraphPassRecord_s sirTaskGraphPassRecord_t;

typedef enum sirTaskGraphAccessMode_e : uint8_t
{
	SIR_TASK_GRAPH_READ = 0,
	SIR_TASK_GRAPH_WRITE = 1
} sirTaskGraphAccessMode_t;

// Caller supplies a canonical 128-bit generation-checked resource identity.
// All accesses to the same logical resource must use the same identity.
// The interval is [nOffset, nOffset + nBytes), with checked arithmetic.
typedef struct sirTaskGraphResourceKey_s
{
	uint64_t nLow;
	uint64_t nHigh;
} sirTaskGraphResourceKey_t;

typedef struct sirTaskGraphAccess_s
{
	sirTaskGraphResourceKey_t keyResource;
	uint64_t nOffset;
	uint64_t nBytes;
	sirTaskGraphAccessMode_t eMode;
} sirTaskGraphAccess_t;

typedef struct sirTaskGraphPass_s
{
	sirTaskFunction_t pFunction;
	void *pContext;
	sirTaskPriority_t ePriority;
	sirTaskExecutor_t eExecutor;
} sirTaskGraphPass_t;

typedef struct sirTaskGraphEdge_s
{
	uint32_t nBefore;
	uint32_t nAfter;
} sirTaskGraphEdge_t;

typedef struct sirTaskGraphConfig_s
{
	uint32_t nPassCapacity;
	uint32_t nAccessCapacity;
	uint32_t nEdgeCapacity;
} sirTaskGraphConfig_t;

typedef enum sirTaskGraphStatus_e : uint8_t
{
	SIR_TASK_GRAPH_OK = 0,
	SIR_TASK_GRAPH_INVALID = 1,
	SIR_TASK_GRAPH_CAPACITY = 2,
	SIR_TASK_GRAPH_CYCLE = 3
} sirTaskGraphStatus_t;

// A resource publication owner may publish only after SUCCEEDED; PENDING also
// covers deferred provider work. Summaries scan live handles and are not an
// atomic snapshot across multiple concurrently completing tasks.
typedef enum sirTaskGraphExecutionState_e : uint8_t
{
	SIR_TASK_GRAPH_EXEC_INVALID = 0,
	SIR_TASK_GRAPH_EXEC_PENDING = 1,
	SIR_TASK_GRAPH_EXEC_SUCCEEDED = 2,
	SIR_TASK_GRAPH_EXEC_FAILED = 3,
	SIR_TASK_GRAPH_EXEC_CANCELLED = 4
} sirTaskGraphExecutionState_t;

typedef struct sirTaskGraphExecutionSummary_s
{
	uint32_t nTotal;
	uint32_t nPending;
	uint32_t nSucceeded;
	uint32_t nFailed;
	uint32_t nCancelled;
	sirTaskGraphExecutionState_t eState;
} sirTaskGraphExecutionSummary_t;

// Single-threaded graph builder; generated edge and task arrays are bounded.
// Hazard edges represent RAW/WAR/WAW overlaps; read/read is unrestricted.
// Active access frontiers prune obsolete fully-overwritten intervals.
// The persistent resource graph and publication authority remain in GRDG.
class SIRTaskGraph
{
public:
	SIRTaskGraph( void ) noexcept;
	~SIRTaskGraph();
	SIRTaskGraph( const SIRTaskGraph& ) = delete;
	SIRTaskGraph& operator=( const SIRTaskGraph& ) = delete;

	bool init( SIRMemoryManager *pMemory, const sirTaskGraphConfig_t& config ) noexcept;
	bool reset( void ) noexcept;
	bool addPass( const sirTaskGraphPass_t& pass, const sirTaskGraphAccess_t *pAccesses, uint32_t nAccesses,
	              uint32_t *pnPassIndex ) noexcept;
	bool addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept;
	sirTaskGraphStatus_t compile( void ) noexcept;
	uint32_t getPassCount( void ) const noexcept;
	uint32_t getEdgeCount( void ) const noexcept;
	bool getEdge( uint32_t nIndex, sirTaskGraphEdge_t *pEdge ) const noexcept;
	bool getTopologicalPass( uint32_t nIndex, uint32_t *pnPass ) const noexcept;

	// Submit is an all-or-nothing scheduler transaction. All pass contexts
	// must remain valid until releaseSubmitted() succeeds. After submission,
	// the caller pumps owner-affine tasks and/or waits for CPU completion.
	bool submit( SIRTaskScheduler *pScheduler ) noexcept;
	bool getSubmittedHandle( uint32_t nPass, sirTaskHandle_t *pHandle ) const noexcept;
	// Nonblocking observation for the GRDG writer. A failed/cancelled batch
	// remains PENDING until all callbacks, including providers, are terminal.
	bool queryExecution( sirTaskGraphExecutionSummary_t *pSummary ) const noexcept;
	// Request cancellation of all unfinished passes without invalidating their
	// contexts. Deferred providers retain ownership until acknowledgement.
	bool cancelExecution( void ) noexcept;
	bool releaseSubmitted( void ) noexcept;

private:
	bool appendEdge( uint32_t nBefore, uint32_t nAfter ) noexcept;
	bool overlaps( const sirTaskGraphAccessRecord_t& prior, const sirTaskGraphAccessRecord_t& current ) const noexcept;
	uint32_t findResourceBucket( sirTaskGraphResourceKey_t keyResource ) noexcept;

	SIRMemoryManager *m_pMemory;
	SIRTaskScheduler *m_pScheduler;
	sirTaskGraphPassRecord_t *m_pPasses;
	sirTaskGraphAccessRecord_t *m_pAccesses;
	sirTaskGraphResourceBucket_t *m_pBuckets;
	sirTaskGraphEdge_t *m_pExplicitEdges;
	sirTaskGraphEdge_t *m_pEdges;
	uint32_t *m_pFirstOut;
	uint32_t *m_pEdgeNext;
	uint32_t *m_pIndegrees;
	uint32_t *m_pQueue;
	uint32_t *m_pTopological;
	uint32_t *m_pReverse;
	sirTaskBatchTask_t *m_pBatchTasks;
	sirTaskBatchEdge_t *m_pBatchEdges;
	sirTaskHandle_t *m_pBatchHandles;
	uint32_t m_nPassCapacity;
	uint32_t m_nAccessCapacity;
	uint32_t m_nEdgeCapacity;
	uint32_t m_nBucketCapacity;
	uint32_t m_nPassCount;
	uint32_t m_nAccessCount;
	uint32_t m_nExplicitEdgeCount;
	uint32_t m_nEdgeCount;
	bool m_bInitialized;
	bool m_bCompiled;
	bool m_bSubmitted;
};

#endif // __SIRENGINE_SIRTASKGRAPH_H__
