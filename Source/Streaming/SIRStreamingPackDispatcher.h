/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRSTREAMINGPACKDISPATCHER_H__
#define __SIRENGINE_SIRSTREAMINGPACKDISPATCHER_H__

#include <Resources/SIRPackAsyncResourceLoad.h>
#include <Streaming/SIRStreamingCoordinator.h>

// Immutable mapping emitted by the world/asset cooker. A streaming unit is
// only a demand key; GRDG continues to own its canonical ResourceKey.
typedef struct sirStreamingPackBinding_s
{
	sirStreamingUnitKey_t keyUnit;
	SIRResourceKey keyResource;
	uint64_t nExpectedRevision;
} sirStreamingPackBinding_t;

typedef struct sirStreamingPackTicketState_s
{
	uint32_t nGeneration;
	uint32_t iLoad;
	bool bSubscribed;
} sirStreamingPackTicketState_t;

typedef struct sirStreamingPackBudgetDemand_s
{
	SIRResourceKey key;
	uint64_t nRevision;
	uint32_t nPriority;
	bool bMandatory;
} sirStreamingPackBudgetDemand_t;

// The global prediction/budget owner provides the reservation callbacks;
// Streaming does not import its budgeter or invent physical allocations.
typedef bool ( *sirStreamingPackBudgetReserve_t )( void *pContext, const sirStreamingPackBudgetDemand_t& demand,
                                                   uint64_t *pReservation ) noexcept;
typedef void ( *sirStreamingPackBudgetRelease_t )( void *pContext, uint64_t nReservation ) noexcept;

typedef struct sirStreamingPackLoadState_s
{
	SIRPackAsyncResourceLoad loader;
	uint64_t nBudgetReservation = 0;
} sirStreamingPackLoadState_t;

typedef struct sirStreamingPackConfig_s
{
	SIRStreamingCoordinator *pCoordinator;
	SIRResourceGraph *pGraph;
	SIRAsyncIOService *pIO;
	const SIRPackCatalog *pCatalog;
	const sirIOSourceHandle_t *pSources;
	const sirStreamingPackBinding_t *pBindings;
	const sirPackAsyncWorkspace_t *pWorkspaces;
	sirStreamingPackLoadState_t *pLoads;
	sirStreamingPackTicketState_t *pTickets;
	uint32_t nSources;
	uint32_t nBindings;
	uint32_t nLoads;
	uint32_t nTicketCapacity;
	void *pBudgetContext;
	sirStreamingPackBudgetReserve_t pfnReserveBudget;
	sirStreamingPackBudgetRelease_t pfnReleaseBudget;
} sirStreamingPackConfig_t;

typedef struct sirStreamingPackStats_s
{
	uint32_t nStarted;
	uint32_t nShared;
	uint32_t nCacheHits;
	uint32_t nDeferred;
	uint32_t nCompleted;
	uint32_t nFailed;
	uint32_t nCancelled;
} sirStreamingPackStats_t;

// An owner-thread consumer of existing streaming tickets, not a second
// scheduler or registry. Directly borrows SIRPack, native I/O and GRDG.
// Caller retains every configured source/buffer/mount until shutdown, and
// forwards IO completion on the authorized GRDG writer thread.
class SIRStreamingPackDispatcher final
{
private:
	sirStreamingPackConfig_t m_Config;
	bool m_bInitialized;

	const sirStreamingPackBinding_t *resolveBinding( const sirStreamingUnitKey_t& key ) const noexcept;
	bool dispatchTicket( sirStreamingTicket_t ticket, const sirStreamingSnapshot_t& snapshot, sirStreamingPackStats_t& stats ) noexcept;
	void processCancellations( sirStreamingPackStats_t& stats ) noexcept;
	void finishTicket( sirStreamingTicket_t ticket, bool bSucceeded, bool bCancelled ) noexcept;

public:
	SIRStreamingPackDispatcher( void ) noexcept;
	sirStreamingResult_t initialize( const sirStreamingPackConfig_t& config ) noexcept;
	// Requests the highest-priority queued demand first. Exactly one physical
	// loader is shared for equal canonical resource/revision identities.
	sirStreamingResult_t pump( uint32_t nMaxDispatches, sirStreamingPackStats_t *pStats ) noexcept;
	bool ownsCompletion( const sirIOCompletion_t& completion ) const noexcept;
	sirStreamingResult_t complete( const sirIOCompletion_t& completion, SIRResourceHandle *pPublished ) noexcept;
	// The actual provider/World owner calls this after confirming its safe
	// retirement point. The dispatcher must not fake ACTIVE readiness.
	sirStreamingResult_t retirePrepared( sirStreamingTicket_t ticket ) noexcept;
	uint32_t getActiveLoadCount( void ) const noexcept;
	sirStreamingResult_t shutdown( void ) noexcept;
};

#endif // __SIRENGINE_SIRSTREAMINGPACKDISPATCHER_H__
