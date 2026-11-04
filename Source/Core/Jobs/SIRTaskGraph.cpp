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

#include <Core/Jobs/SIRTaskGraph.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static constexpr uint32_t SIR_GRAPH_NONE = UINT32_MAX;

typedef struct sirTaskGraphPassRecord_s
{
	sirTaskGraphPass_t Pass;
	uint32_t nFirstAccess;
	uint32_t nAccessCount;
} sirTaskGraphPassRecord_t;

typedef struct sirTaskGraphAccessRecord_s
{
	sirTaskGraphResourceKey_t keyResource;
	uint64_t nOffset;
	uint64_t nEnd;
	uint32_t nPass;
	uint32_t nNext;
	uint32_t nPrev;
	sirTaskGraphAccessMode_t eMode;
} sirTaskGraphAccessRecord_t;

typedef struct sirTaskGraphResourceBucket_s
{
	sirTaskGraphResourceKey_t keyResource;
	uint32_t nHead;
	bool bUsed;
} sirTaskGraphResourceBucket_t;

static uint64_t sirGraphHashResource( sirTaskGraphResourceKey_t keyResource ) noexcept
{
	uint64_t nValue = keyResource.nLow ^ ( keyResource.nHigh * UINT64_C( 0x9e3779b97f4a7c15 ) );
	nValue ^= nValue >> 30;
	nValue *= UINT64_C( 0xbf58476d1ce4e5b9 );
	nValue ^= nValue >> 27;
	nValue *= UINT64_C( 0x94d049bb133111eb );
	return nValue ^ ( nValue >> 31 );
}

template <typename T>
static bool sirGraphAppendFootprint( uint64_t *pnBytes, uint32_t nCount ) noexcept
{
	const uint64_t nAlignment = alignof( T );
	if ( *pnBytes > UINT64_MAX - ( nAlignment - 1 ) ) {
		return false;
	}
	const uint64_t nAligned = ( *pnBytes + nAlignment - 1 ) & ~( nAlignment - 1 );
	if ( nCount > ( UINT64_MAX - nAligned ) / sizeof( T ) ) {
		return false;
	}
	*pnBytes = nAligned + ( uint64_t )nCount * sizeof( T );
	return true;
}

template <typename T>
static T *sirGraphCarveArray( uint8_t **ppMemory, uint32_t nCount ) noexcept
{
	const uintptr_t nAlignment = alignof( T );
	const uintptr_t nAddress = ( ( uintptr_t )*ppMemory + nAlignment - 1 ) & ~( nAlignment - 1 );
	*ppMemory = ( uint8_t * )( nAddress + ( uintptr_t )nCount * sizeof( T ) );
	return ( T * )nAddress;
}

SIRTaskGraph::SIRTaskGraph( void ) noexcept
    : m_pMemory( nullptr ), m_pScheduler( nullptr ), m_pPasses( nullptr ), m_pAccesses( nullptr ), m_pBuckets( nullptr ),
      m_pExplicitEdges( nullptr ), m_pEdges( nullptr ), m_pFirstOut( nullptr ), m_pEdgeNext( nullptr ), m_pIndegrees( nullptr ),
      m_pQueue( nullptr ), m_pTopological( nullptr ), m_pReverse( nullptr ), m_pBatchTasks( nullptr ), m_pBatchEdges( nullptr ),
      m_pBatchHandles( nullptr ), m_nPassCapacity( 0 ), m_nAccessCapacity( 0 ), m_nEdgeCapacity( 0 ), m_nBucketCapacity( 0 ),
      m_nPassCount( 0 ), m_nAccessCount( 0 ), m_nExplicitEdgeCount( 0 ), m_nEdgeCount( 0 ), m_bInitialized( false ), m_bCompiled( false ),
      m_bSubmitted( false )
{
}

SIRTaskGraph::~SIRTaskGraph()
{
	// The resource arena owns graph metadata. No destruction while jobs
	// still refer to caller-owned pass contexts or retained task handles.
	if ( m_bSubmitted ) {
		abort();
	}
}

bool SIRTaskGraph::init( SIRMemoryManager *pMemory, const sirTaskGraphConfig_t &config ) noexcept
{
	if ( m_bInitialized || pMemory == nullptr || !pMemory->isInitialized() || config.nPassCapacity == 0 || config.nAccessCapacity == 0 ||
	     config.nEdgeCapacity == 0 || config.nPassCapacity >= UINT32_MAX || config.nAccessCapacity >= UINT32_MAX ||
	     config.nEdgeCapacity >= UINT32_MAX || config.nAccessCapacity > UINT32_MAX / 2 ) {
		return false;
	}
	uint32_t nBuckets = 1;
	while ( nBuckets <= config.nAccessCapacity * 2 ) {
		if ( nBuckets > UINT32_MAX / 2 ) {
			return false;
		}
		nBuckets *= 2;
	}
	// One page-accounted metadata reservation; failed admission does not
	// leave a partially initialized graph or consume multiple arena spans.
	uint64_t nStorageBytes = 0;
	if ( !sirGraphAppendFootprint<sirTaskGraphPassRecord_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<sirTaskGraphAccessRecord_t>( &nStorageBytes, config.nAccessCapacity ) ||
	     !sirGraphAppendFootprint<sirTaskGraphResourceBucket_t>( &nStorageBytes, nBuckets ) ||
	     !sirGraphAppendFootprint<sirTaskGraphEdge_t>( &nStorageBytes, config.nEdgeCapacity ) ||
	     !sirGraphAppendFootprint<sirTaskGraphEdge_t>( &nStorageBytes, config.nEdgeCapacity ) ||
	     !sirGraphAppendFootprint<uint32_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<uint32_t>( &nStorageBytes, config.nEdgeCapacity ) ||
	     !sirGraphAppendFootprint<uint32_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<uint32_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<uint32_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<uint32_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<sirTaskBatchTask_t>( &nStorageBytes, config.nPassCapacity ) ||
	     !sirGraphAppendFootprint<sirTaskBatchEdge_t>( &nStorageBytes, config.nEdgeCapacity ) ||
	     !sirGraphAppendFootprint<sirTaskHandle_t>( &nStorageBytes, config.nPassCapacity ) ) {
		return false;
	}
	void *pStorage = pMemory->allocateMetadata( nStorageBytes, alignof( max_align_t ), SIR_MEMORY_DOMAIN_CORE );
	if ( pStorage == nullptr ) {
		return false;
	}
	uint8_t *pCursor = ( uint8_t * )pStorage;
	m_pPasses = sirGraphCarveArray<sirTaskGraphPassRecord_t>( &pCursor, config.nPassCapacity );
	m_pAccesses = sirGraphCarveArray<sirTaskGraphAccessRecord_t>( &pCursor, config.nAccessCapacity );
	m_pBuckets = sirGraphCarveArray<sirTaskGraphResourceBucket_t>( &pCursor, nBuckets );
	m_pExplicitEdges = sirGraphCarveArray<sirTaskGraphEdge_t>( &pCursor, config.nEdgeCapacity );
	m_pEdges = sirGraphCarveArray<sirTaskGraphEdge_t>( &pCursor, config.nEdgeCapacity );
	m_pFirstOut = sirGraphCarveArray<uint32_t>( &pCursor, config.nPassCapacity );
	m_pEdgeNext = sirGraphCarveArray<uint32_t>( &pCursor, config.nEdgeCapacity );
	m_pIndegrees = sirGraphCarveArray<uint32_t>( &pCursor, config.nPassCapacity );
	m_pQueue = sirGraphCarveArray<uint32_t>( &pCursor, config.nPassCapacity );
	m_pTopological = sirGraphCarveArray<uint32_t>( &pCursor, config.nPassCapacity );
	m_pReverse = sirGraphCarveArray<uint32_t>( &pCursor, config.nPassCapacity );
	m_pBatchTasks = sirGraphCarveArray<sirTaskBatchTask_t>( &pCursor, config.nPassCapacity );
	m_pBatchEdges = sirGraphCarveArray<sirTaskBatchEdge_t>( &pCursor, config.nEdgeCapacity );
	m_pBatchHandles = sirGraphCarveArray<sirTaskHandle_t>( &pCursor, config.nPassCapacity );
	m_pMemory = pMemory;
	m_nPassCapacity = config.nPassCapacity;
	m_nAccessCapacity = config.nAccessCapacity;
	m_nEdgeCapacity = config.nEdgeCapacity;
	m_nBucketCapacity = nBuckets;
	m_bInitialized = true;
	return true;
}

bool SIRTaskGraph::reset( void ) noexcept
{
	if ( !m_bInitialized || m_bSubmitted ) {
		return false;
	}
	m_nPassCount = 0;
	m_nAccessCount = 0;
	m_nExplicitEdgeCount = 0;
	m_nEdgeCount = 0;
	m_bCompiled = false;
	return true;
}

bool SIRTaskGraph::addPass( const sirTaskGraphPass_t &pass, const sirTaskGraphAccess_t *pAccesses, uint32_t nAccesses,
                            uint32_t *pnPassIndex ) noexcept
{
	if ( !m_bInitialized || m_bSubmitted || pnPassIndex == nullptr || pass.pFunction == nullptr || pass.eExecutor >= SIR_TASK_EXEC_COUNT ||
	     pass.ePriority > SIR_TASK_PRIORITY_BACKGROUND || m_nPassCount == m_nPassCapacity ||
	     nAccesses > m_nAccessCapacity - m_nAccessCount || ( nAccesses != 0 && pAccesses == nullptr ) ) {
		return false;
	}
	for ( uint32_t iAccess = 0; iAccess < nAccesses; ++iAccess ) {
		const sirTaskGraphAccess_t &access = pAccesses[iAccess];
		if ( ( access.keyResource.nLow == 0 && access.keyResource.nHigh == 0 ) || access.nBytes == 0 ||
		     access.nBytes > UINT64_MAX - access.nOffset || access.eMode > SIR_TASK_GRAPH_WRITE ) {
			return false;
		}
	}
	const uint32_t nPass = m_nPassCount;
	m_pPasses[nPass] = { pass, m_nAccessCount, nAccesses };
	for ( uint32_t iAccess = 0; iAccess < nAccesses; ++iAccess ) {
		const sirTaskGraphAccess_t &access = pAccesses[iAccess];
		m_pAccesses[m_nAccessCount + iAccess] = { access.keyResource, access.nOffset, access.nOffset + access.nBytes, nPass, SIR_GRAPH_NONE,
		                                          SIR_GRAPH_NONE,     access.eMode };
	}
	m_nAccessCount += nAccesses;
	++m_nPassCount;
	m_bCompiled = false;
	*pnPassIndex = nPass;
	return true;
}

bool SIRTaskGraph::addDependency( uint32_t nBefore, uint32_t nAfter ) noexcept
{
	if ( !m_bInitialized || m_bSubmitted || nBefore >= m_nPassCount || nAfter >= m_nPassCount || nBefore == nAfter ||
	     m_nExplicitEdgeCount == m_nEdgeCapacity ) {
		return false;
	}
	m_pExplicitEdges[m_nExplicitEdgeCount++] = { nBefore, nAfter };
	m_bCompiled = false;
	return true;
}

uint32_t SIRTaskGraph::findResourceBucket( sirTaskGraphResourceKey_t keyResource ) noexcept
{
	const uint32_t nMask = m_nBucketCapacity - 1;
	uint32_t nIndex = ( uint32_t )( sirGraphHashResource( keyResource ) ) & nMask;
	for ( ;; ) {
		sirTaskGraphResourceBucket_t &bucket = m_pBuckets[nIndex];
		if ( !bucket.bUsed ) {
			bucket.bUsed = true;
			bucket.keyResource = keyResource;
			bucket.nHead = SIR_GRAPH_NONE;
			return nIndex;
		}
		if ( ( bucket.keyResource.nLow == keyResource.nLow && bucket.keyResource.nHigh == keyResource.nHigh ) ) {
			return nIndex;
		}
		nIndex = ( nIndex + 1 ) & nMask;
	}
}

bool SIRTaskGraph::overlaps( const sirTaskGraphAccessRecord_t &prior, const sirTaskGraphAccessRecord_t &current ) const noexcept
{
	return prior.nOffset < current.nEnd && current.nOffset < prior.nEnd;
}

bool SIRTaskGraph::appendEdge( uint32_t nBefore, uint32_t nAfter ) noexcept
{
	// Read and write views within a single pass execute together.
	if ( nBefore == nAfter ) {
		return true;
	}
	for ( uint32_t nEdge = m_pFirstOut[nBefore]; nEdge != SIR_GRAPH_NONE; nEdge = m_pEdgeNext[nEdge] ) {
		if ( m_pEdges[nEdge].nAfter == nAfter ) {
			return true;
		}
	}
	if ( m_nEdgeCount == m_nEdgeCapacity ) {
		return false;
	}
	const uint32_t nEdge = m_nEdgeCount++;
	m_pEdges[nEdge] = { nBefore, nAfter };
	m_pEdgeNext[nEdge] = m_pFirstOut[nBefore];
	m_pFirstOut[nBefore] = nEdge;
	++m_pIndegrees[nAfter];
	return true;
}

sirTaskGraphStatus_t SIRTaskGraph::compile( void ) noexcept
{
	if ( !m_bInitialized || m_bSubmitted || m_nPassCount == 0 ) {
		return SIR_TASK_GRAPH_INVALID;
	}
	m_bCompiled = false;
	m_nEdgeCount = 0;
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		m_pFirstOut[iPass] = SIR_GRAPH_NONE;
		m_pIndegrees[iPass] = 0;
	}
	memset( m_pBuckets, 0, ( size_t )m_nBucketCapacity * sizeof( sirTaskGraphResourceBucket_t ) );
	for ( uint32_t i = 0; i < m_nExplicitEdgeCount; ++i ) {
		if ( !appendEdge( m_pExplicitEdges[i].nBefore, m_pExplicitEdges[i].nAfter ) ) {
			return SIR_TASK_GRAPH_CAPACITY;
		}
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		const sirTaskGraphPassRecord_t &pass = m_pPasses[iPass];
		for ( uint32_t iAccess = pass.nFirstAccess; iAccess < pass.nFirstAccess + pass.nAccessCount; ++iAccess ) {
			sirTaskGraphAccessRecord_t &access = m_pAccesses[iAccess];
			const uint32_t nBucket = findResourceBucket( access.keyResource );
			sirTaskGraphResourceBucket_t &bucket = m_pBuckets[nBucket];
			uint32_t nEarlier = bucket.nHead;
			while ( nEarlier != SIR_GRAPH_NONE ) {
				sirTaskGraphAccessRecord_t &earlier = m_pAccesses[nEarlier];
				const uint32_t nNext = earlier.nNext;
				const bool bOverlap = overlaps( earlier, access );
				if ( earlier.nPass != access.nPass && bOverlap &&
				     ( earlier.eMode == SIR_TASK_GRAPH_WRITE || access.eMode == SIR_TASK_GRAPH_WRITE ) &&
				     !appendEdge( earlier.nPass, access.nPass ) ) {
					return SIR_TASK_GRAPH_CAPACITY;
				}
				// Once a later writer covers the full old interval, future
				// accesses need only order against this writer. Partial writes
				// retain the uncovered prior interval conservatively.
				if ( access.eMode == SIR_TASK_GRAPH_WRITE && earlier.nOffset >= access.nOffset && earlier.nEnd <= access.nEnd ) {
					if ( earlier.nPrev == SIR_GRAPH_NONE ) {
						bucket.nHead = earlier.nNext;
					} else {
						m_pAccesses[earlier.nPrev].nNext = earlier.nNext;
					}
					if ( earlier.nNext != SIR_GRAPH_NONE ) {
						m_pAccesses[earlier.nNext].nPrev = earlier.nPrev;
					}
				}
				nEarlier = nNext;
			}
			access.nPrev = SIR_GRAPH_NONE;
			access.nNext = bucket.nHead;
			if ( access.nNext != SIR_GRAPH_NONE ) {
				m_pAccesses[access.nNext].nPrev = iAccess;
			}
			bucket.nHead = iAccess;
		}
	}
	// Kahn's traversal produces a deterministic topological index, rejecting
	// cycles introduced by explicit dependencies against resource hazards.
	uint32_t nHead = 0;
	uint32_t nTail = 0;
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( m_pIndegrees[iPass] == 0 ) {
			m_pQueue[nTail++] = iPass;
		}
	}
	uint32_t nProcessed = 0;
	while ( nHead < nTail ) {
		const uint32_t nPass = m_pQueue[nHead++];
		m_pTopological[nProcessed++] = nPass;
		for ( uint32_t nEdge = m_pFirstOut[nPass]; nEdge != SIR_GRAPH_NONE; nEdge = m_pEdgeNext[nEdge] ) {
			const uint32_t nNext = m_pEdges[nEdge].nAfter;
			if ( --m_pIndegrees[nNext] == 0 ) {
				m_pQueue[nTail++] = nNext;
			}
		}
	}
	if ( nProcessed != m_nPassCount ) {
		return SIR_TASK_GRAPH_CYCLE;
	}
	m_bCompiled = true;
	return SIR_TASK_GRAPH_OK;
}

uint32_t SIRTaskGraph::getPassCount( void ) const noexcept
{
	return m_nPassCount;
}

uint32_t SIRTaskGraph::getEdgeCount( void ) const noexcept
{
	return m_bCompiled ? m_nEdgeCount : 0;
}

bool SIRTaskGraph::getEdge( uint32_t nIndex, sirTaskGraphEdge_t *pEdge ) const noexcept
{
	if ( !m_bCompiled || nIndex >= m_nEdgeCount || pEdge == nullptr ) {
		return false;
	}
	*pEdge = m_pEdges[nIndex];
	return true;
}

bool SIRTaskGraph::getTopologicalPass( uint32_t nIndex, uint32_t *pnPass ) const noexcept
{
	if ( !m_bCompiled || nIndex >= m_nPassCount || pnPass == nullptr ) {
		return false;
	}
	*pnPass = m_pTopological[nIndex];
	return true;
}

bool SIRTaskGraph::submit( SIRTaskScheduler *pScheduler ) noexcept
{
	if ( !m_bCompiled || m_bSubmitted || pScheduler == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nPassCount; ++i ) {
		const uint32_t nPass = m_pTopological[i];
		const sirTaskGraphPass_t &pass = m_pPasses[nPass].Pass;
		m_pReverse[nPass] = i;
		m_pBatchTasks[i] = { pass.pFunction, pass.pContext, pass.ePriority, pass.eExecutor };
	}
	for ( uint32_t i = 0; i < m_nEdgeCount; ++i ) {
		m_pBatchEdges[i] = { m_pReverse[m_pEdges[i].nBefore], m_pReverse[m_pEdges[i].nAfter] };
	}
	if ( !pScheduler->submitBatch( m_pBatchTasks, m_nPassCount, m_pBatchEdges, m_nEdgeCount, m_pBatchHandles ) ) {
		return false;
	}
	m_pScheduler = pScheduler;
	m_bSubmitted = true;
	return true;
}

bool SIRTaskGraph::getSubmittedHandle( uint32_t nPass, sirTaskHandle_t *pHandle ) const noexcept
{
	if ( !m_bSubmitted || pHandle == nullptr || nPass >= m_nPassCount ) {
		return false;
	}
	*pHandle = m_pBatchHandles[m_pReverse[nPass]];
	return true;
}

bool SIRTaskGraph::queryExecution( sirTaskGraphExecutionSummary_t *pSummary ) const noexcept
{
	if ( !m_bSubmitted || m_pScheduler == nullptr || pSummary == nullptr ) {
		return false;
	}
	sirTaskGraphExecutionSummary_t summary = {};
	summary.nTotal = m_nPassCount;
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		const sirTaskHandle_t task = m_pBatchHandles[iPass];
		switch ( m_pScheduler->getState( task ) ) {
		case SIR_TASK_COMPLETED:
			++summary.nSucceeded;
			break;
		case SIR_TASK_FAILED:
			++summary.nFailed;
			break;
		case SIR_TASK_CANCELLED:
			++summary.nCancelled;
			break;
		case SIR_TASK_BLOCKED:
		case SIR_TASK_QUEUED:
		case SIR_TASK_RUNNING:
		case SIR_TASK_AWAITING_PROVIDER:
			++summary.nPending;
			break;
		default:
			return false;
		}
	}
	if ( summary.nPending != 0 ) {
		summary.eState = SIR_TASK_GRAPH_EXEC_PENDING;
	} else if ( summary.nFailed != 0 ) {
		summary.eState = SIR_TASK_GRAPH_EXEC_FAILED;
	} else if ( summary.nCancelled != 0 ) {
		summary.eState = SIR_TASK_GRAPH_EXEC_CANCELLED;
	} else {
		summary.eState = SIR_TASK_GRAPH_EXEC_SUCCEEDED;
	}
	*pSummary = summary;
	return true;
}

bool SIRTaskGraph::cancelExecution( void ) noexcept
{
	if ( !m_bSubmitted || m_pScheduler == nullptr ) {
		return false;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		const sirTaskHandle_t task = m_pBatchHandles[iPass];
		const sirTaskState_t eState = m_pScheduler->getState( task );
		if ( eState == SIR_TASK_COMPLETED || eState == SIR_TASK_FAILED || eState == SIR_TASK_CANCELLED ) {
			continue;
		}
		if ( eState == SIR_TASK_INVALID ) {
			return false;
		}
		if ( !m_pScheduler->cancel( task ) ) {
			// An already-finished task may race with the cancellation request.
			const sirTaskState_t eNow = m_pScheduler->getState( task );
			if ( eNow != SIR_TASK_COMPLETED && eNow != SIR_TASK_FAILED && eNow != SIR_TASK_CANCELLED ) {
				return false;
			}
		}
	}
	return true;
}

bool SIRTaskGraph::releaseSubmitted( void ) noexcept
{
	if ( !m_bSubmitted || m_pScheduler == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nPassCount; ++i ) {
		const sirTaskHandle_t task = m_pBatchHandles[i];
		if ( task.nGeneration == 0 ) {
			continue;
		}
		const sirTaskState_t eState = m_pScheduler->getState( task );
		if ( eState != SIR_TASK_COMPLETED && eState != SIR_TASK_FAILED && eState != SIR_TASK_CANCELLED ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < m_nPassCount; ++i ) {
		sirTaskHandle_t &task = m_pBatchHandles[i];
		if ( task.nGeneration != 0 ) {
			if ( !m_pScheduler->releaseTask( task ) ) {
				return false;
			}
			task = { SIR_GRAPH_NONE, 0 };
		}
	}
	m_pScheduler = nullptr;
	m_bSubmitted = false;
	return true;
}
