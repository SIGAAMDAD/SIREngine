/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREDEMANDQUEUE_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREDEMANDQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Terrain3D's four-slot bounded baked-visibility demand-bundle scheduler.
// The owner lane serializes all calls; no worker or GPU resource is owned here.
static constexpr uint32_t SIR_VT_DEMAND_BUNDLE_SLOTS = 4U;

typedef enum sirVirtualTextureDemandBundleState_e : uint8_t
{
	SIR_VT_BUNDLE_FREE = 0,
	SIR_VT_BUNDLE_QUEUED = 1,
	SIR_VT_BUNDLE_LOADING = 2,
	SIR_VT_BUNDLE_READY = 3,
	SIR_VT_BUNDLE_FAILED = 4
} sirVirtualTextureDemandBundleState_t;

typedef struct sirVirtualTextureDemandTicket_s
{
	uint32_t nSlot;
	uint32_t nBundle;
	uint64_t nEpoch;
	uint64_t nSerial;
} sirVirtualTextureDemandTicket_t;

typedef struct sirVirtualTextureDemandSlot_s
{
	uint32_t nBundle;
	uint32_t nPriority;
	uint64_t nAge;
	uint64_t nEpoch;
	uint64_t nSerial;
	sirVirtualTextureDemandBundleState_t eState;
} sirVirtualTextureDemandSlot_t;

typedef struct sirVirtualTextureDemandQueueStats_s
{
	uint64_t nCoalesced;
	uint64_t nEvictions;
	uint64_t nRejected;
	uint32_t nQueued;
	uint32_t nLoading;
	uint32_t nReady;
} sirVirtualTextureDemandQueueStats_t;

// This class isolates the donor's four-slot state selection from file access.
// Caller-owned decoded records must remain pinned while their slot is LOADING.
// Reset is deliberately rejected until all in-flight reads are retired.
class SIRVirtualTextureDemandQueue final
{
public:
	SIRVirtualTextureDemandQueue( void ) noexcept;
	~SIRVirtualTextureDemandQueue( void ) noexcept = default;

	uint64_t getEpoch( void ) const noexcept;
	bool isReady( uint32_t nBundle ) const noexcept;
	sirVirtualTextureDemandQueueStats_t getStats( void ) const noexcept;

	bool requestBundle( uint32_t nBundle, uint8_t nPriority ) noexcept;
	bool beginLoad( sirVirtualTextureDemandTicket_t *pTicket ) noexcept;
	bool finishLoad( const sirVirtualTextureDemandTicket_t& ticket, bool bSucceeded ) noexcept;
	bool resetCold( void ) noexcept;

protected:
private:
	int32_t findSlot( uint32_t nBundle ) const noexcept;
	void advanceUseClock( void ) noexcept;

	sirVirtualTextureDemandSlot_t m_szSlots[SIR_VT_DEMAND_BUNDLE_SLOTS] = {};
	uint64_t m_nEpoch = 1;
	uint64_t m_nUseClock = 0;
	uint64_t m_nNextSerial = 0;
	uint64_t m_nCoalesced = 0;
	uint64_t m_nEvictions = 0;
	uint64_t m_nRejected = 0;
};

#endif // __SIRENGINE_SIRVIRTUALTEXTUREDEMANDQUEUE_H__
