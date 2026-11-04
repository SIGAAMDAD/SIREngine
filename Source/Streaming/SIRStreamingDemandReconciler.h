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

#ifndef __SIRENGINE_SIRSTREAMINGDEMANDRECONCILER_H__
#define __SIRENGINE_SIRSTREAMINGDEMANDRECONCILER_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Streaming/SIRStreamingCoordinator.h>

typedef struct sirStreamingReconcileStats_s
{
	uint32_t nTracked;
	uint32_t nSubmitted;
	uint32_t nUpdated;
	uint32_t nCancelled;
	uint32_t nDeferred;
	uint32_t nRetirePending;
	uint32_t nRequestEpoch;
} sirStreamingReconcileStats_t;

struct sirStreamingReconcileEntry_s;

// Single-thread owner of one consumer's desired demands and coordinator tickets.
// Uses SIRMemory STREAMING-domain caller storage. This is a control plane, not
// a resource cache: providers/GRDG own the lifetime of physical resources.
class SIRStreamingDemandReconciler
{
private:
	SIRStreamingCoordinator *m_pCoordinator;
	sirStreamingReconcileEntry_s *m_pEntries;
	uint64_t m_nConsumerId;
	uint32_t m_nConsumerGeneration;
	uint32_t m_nCapacity;
	uint32_t m_nLastEpoch;
	bool m_bInitialized;

	uint32_t findEntry( const sirStreamingUnitKey_t &key ) const noexcept;
	void clearEntry( uint32_t iEntry ) noexcept;
	void pruneReleased( void ) noexcept;
	sirStreamingResult_t validateSet( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount, uint32_t nRequestEpoch ) const noexcept;
	void retireMissing( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount, sirStreamingReconcileStats_t &stats ) noexcept;
	void refreshCurrent( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount, sirStreamingReconcileStats_t &stats ) noexcept;
	void submitMissing( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount, sirStreamingReconcileStats_t &stats ) noexcept;

public:
	SIRStreamingDemandReconciler( void ) noexcept;
	SIRStreamingDemandReconciler( const SIRStreamingDemandReconciler & ) = delete;
	SIRStreamingDemandReconciler &operator=( const SIRStreamingDemandReconciler & ) = delete;

	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	sirStreamingResult_t initialize( SIRStreamingCoordinator *pCoordinator, void *pStorage, uint64_t nStorageBytes, uint32_t nCapacity,
	                                 uint64_t nConsumerId, uint32_t nConsumerGeneration ) noexcept;
	// Refuse destruction of live, possibly provider-pinned tickets.
	sirStreamingResult_t shutdown( void ) noexcept;

	// An epoch contains the entire desired set for this consumer. Duplicate keys,
	// invalid demand, capacity errors and stale epochs are rejected before edits.
	// A provider-pinned cancellation is deferred until the owner retires it.
	// Every nonfatal deferred operation is counted; subsequent epochs retry it.
	sirStreamingResult_t reconcile( const sirStreamingDemand_t *pDemands, uint32_t nDemandCount, uint32_t nRequestEpoch,
	                                sirStreamingReconcileStats_t *pStats ) noexcept;

	// Producer/GRDG adapter uses this to obtain a ticket and query the actual
	// coordinator stage. No provider completion is inferred by this class.
	sirStreamingResult_t findTicket( const sirStreamingUnitKey_t &key, sirStreamingTicket_t *pTicket ) const noexcept;
	uint32_t getTrackedCount( void ) const noexcept;
};

#endif // __SIRENGINE_SIRSTREAMINGDEMANDRECONCILER_H__
