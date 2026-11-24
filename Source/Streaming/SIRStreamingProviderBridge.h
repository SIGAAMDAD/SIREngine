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

#ifndef __SIR_STREAMINGPROVIDERBRIDGE_H__
#define __SIR_STREAMINGPROVIDERBRIDGE_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Streaming/SIRStreamingCoordinator.h>

// Begin callbacks only enqueue work with the canonical GRDG/Jobs/provider owner.
// Returning true means accepted, not completed. Callbacks must not re-enter
// this bridge or the coordinator. All methods and acknowledgements run on the
// streaming owner thread; workers must marshal completion to that thread.
typedef struct sirStreamingProviderCallbacks_s
{
	void *pContext;
	bool ( *pBeginPrepare )( void *pContext, sirStreamingTicket_t ticket, const sirStreamingDemand_t *pDemand ) noexcept;
	bool ( *pCancelPrepare )( void *pContext, sirStreamingTicket_t ticket ) noexcept;
	bool ( *pBeginActivate )( void *pContext, sirStreamingTicket_t ticket ) noexcept;
	bool ( *pBeginRetire )( void *pContext, sirStreamingTicket_t ticket ) noexcept;
} sirStreamingProviderCallbacks_t;

typedef struct sirStreamingBridgeStats_s
{
	uint32_t nVisited;
	uint32_t nPrepareDispatched;
	uint32_t nCancellationDispatched;
	uint32_t nActivationDispatched;
	uint32_t nRetirementDispatched;
	uint32_t nDeferred;
} sirStreamingBridgeStats_t;

typedef struct sirStreamingBridgeSlot_s sirStreamingBridgeSlot_t;

// Ticket lifecycle dispatch to a single canonical
// resource/provider owner. Never stores payload pointers or native GPU handles.
// The supplied STREAMING-domain storage must remain alive through shutdown.
class SIRStreamingProviderBridge
{
private:
	SIRStreamingCoordinator *m_pCoordinator;
	sirStreamingBridgeSlot_t *m_pSlots;
	sirStreamingProviderCallbacks_t m_Callbacks;
	uint32_t m_nCapacity;
	uint32_t m_iNextSlot;
	bool m_bInitialized;

	bool hasValidEntry( sirStreamingTicket_t ticket ) const noexcept;
	SIRStreamingResult beginRetirement( sirStreamingTicket_t ticket, sirStreamingBridgeSlot_t& slot,
	                                      sirStreamingBridgeStats_t& stats ) noexcept;
	void processSlot( sirStreamingTicket_t ticket, const sirStreamingSnapshot_t& snapshot, sirStreamingBridgeStats_t& stats ) noexcept;

public:
	SIRStreamingProviderBridge( void ) noexcept;
	SIRStreamingProviderBridge( const SIRStreamingProviderBridge& ) = delete;
	SIRStreamingProviderBridge& operator=( const SIRStreamingProviderBridge& ) = delete;

	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	SIRStreamingResult initialize( SIRStreamingCoordinator *pCoordinator, void *pStorage, uint64_t nStorageBytes,
	                                 const sirStreamingProviderCallbacks_t& callbacks ) noexcept;
	SIRStreamingResult shutdown( void ) noexcept;

	// Each pump visits at most nMaximumSlots, including vacant slots. Priority
	// ordering remains owned by the producer/Core Jobs scheduling path.
	SIRStreamingResult pump( uint32_t nMaximumSlots, sirStreamingBridgeStats_t *pStats ) noexcept;

	// Call only after the canonical provider has finished the corresponding
	// operation. Success requires validated GRDG publication/provider readiness.
	SIRStreamingResult acknowledgePrepared( sirStreamingTicket_t ticket, bool bSucceeded ) noexcept;
	SIRStreamingResult acknowledgeActivated( sirStreamingTicket_t ticket, bool bSucceeded ) noexcept;
	// Ack only after GPU/physics/audio fences, reader leases and jobs are safe.
	SIRStreamingResult acknowledgeRetired( sirStreamingTicket_t ticket ) noexcept;
};

#endif // __SIR_STREAMINGPROVIDERBRIDGE_H__
