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

#ifndef __SIRENGINE_SIRSTREAMINGCOORDINATOR_H__
#define __SIRENGINE_SIRSTREAMINGCOORDINATOR_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Streaming/SIRStreamingTypes.h>

struct sirStreamingSlot_s;

// Single owner thread. Work producers must hand demand to that owner through
// Core Jobs' bounded queues once the GRDG/IO integration is available.
// Owns intent and stage accounting only: never owns cooked bytes or GPU/physics
// resources, and never treats a provider callback as completed publication.
class SIRStreamingCoordinator
{
private:
	sirStreamingSlot_s *m_pSlots;
	uint32_t m_nCapacity;
	uint32_t m_nOccupied;
	bool m_bInitialized;
	bool m_bStopped;

	bool isValid( sirStreamingTicket_t ticket ) const noexcept;
	void releaseSlot( uint32_t iSlot ) noexcept;

public:
	SIRStreamingCoordinator( void ) noexcept;
	SIRStreamingCoordinator( const SIRStreamingCoordinator & ) = delete;
	SIRStreamingCoordinator &operator=( const SIRStreamingCoordinator & ) = delete;

	// Caller supplies aligned, lifetime-stable storage from SIRMemory's STREAMING
	// domain in the native runtime. No heap allocation, copy or implicit fallback.
	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	sirStreamingResult_t initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nCapacity ) noexcept;
	// Stops only after every provider has acknowledged retirement.
	sirStreamingResult_t shutdown( void ) noexcept;

	// Each consumer submits its own demand. The future GRDG coordinator merges
	// identical physical resource requests without creating duplicate payloads.
	sirStreamingResult_t submit( const sirStreamingDemand_t &demand, sirStreamingTicket_t *pTicket ) noexcept;
	// Refresh an existing ticket without restarting its provider work. Cannot
	// revive cancellation or demote already-active provider resources.
	sirStreamingResult_t updateDemand( sirStreamingTicket_t ticket, const sirStreamingDemand_t &demand ) noexcept;
	sirStreamingResult_t query( sirStreamingTicket_t ticket, sirStreamingSnapshot_t *pSnapshot ) const noexcept;
	sirStreamingResult_t advance( sirStreamingTicket_t ticket, sirStreamingStage_t eNextStage ) noexcept;
	sirStreamingResult_t cancel( sirStreamingTicket_t ticket ) noexcept;
	sirStreamingStats_t getStats( void ) const noexcept;
};

#endif // __SIRENGINE_SIRSTREAMINGCOORDINATOR_H__
