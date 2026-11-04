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

#ifndef __SIRENGINE_SIRSTREAMINGGRDGADAPTER_H__
#define __SIRENGINE_SIRSTREAMINGGRDGADAPTER_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Streaming/SIRStreamingProviderBridge.h>
#include <Resources/SIRResourceFileLoad.h>

// In-memory cooked-directory projection, not a serialized SIRPack ABI.
// Paths and dependency arrays must remain valid until the adapter shuts down.
// The file loader publishes through the only GRDG writer, never this adapter.
typedef struct sirStreamingGRDGBinding_s
{
	sirStreamingUnitKey_t keyUnit;
	const char *pszFilePath;
	sirResourceFileLoadRequest_t request;
} sirStreamingGRDGBinding_t;

// A GRDG handle does not identify an immutable revision by itself. A provider
// must validate the revision on every borrowed read scope before consuming it.
typedef struct sirStreamingGRDGResident_s
{
	SIRResourceHandle hResource;
	uint64_t nRevision;
} sirStreamingGRDGResident_t;

// Activation and retirement belong to the real backend owner. A true return
// accepts asynchronous work; only acknowledge* can advance the ticket.
typedef struct sirStreamingGRDGProviderCallbacks_s
{
	void *pContext;
	bool ( *pBeginActivate )( void *pContext, sirStreamingTicket_t ticket, const sirStreamingGRDGResident_t *pResident ) noexcept;
	bool ( *pBeginRetire )( void *pContext, sirStreamingTicket_t ticket, const sirStreamingGRDGResident_t *pResident ) noexcept;
} sirStreamingGRDGProviderCallbacks_t;

typedef struct sirStreamingGRDGConfig_s
{
	SIRResourceGraph *pGraph;
	SIRTaskScheduler *pScheduler;
	SIRStreamingCoordinator *pCoordinator;
	const sirStreamingGRDGBinding_t *pBindings;
	void *pStorage;
	uint64_t nStorageBytes;
	uint32_t nBindingCount;
	uint32_t nCapacity;
	uint32_t nProviderId;
	sirStreamingGRDGProviderCallbacks_t callbacks;
} sirStreamingGRDGConfig_t;

typedef struct sirStreamingGRDGStats_s
{
	uint32_t nVisited;
	uint32_t nStarted;
	uint32_t nWaiting;
	uint32_t nPrepared;
	uint32_t nFailed;
} sirStreamingGRDGStats_t;

typedef struct sirStreamingGRDGSlot_s sirStreamingGRDGSlot_t;

// Owner-thread adapter for the actual GRDG/Jobs single-file load path.
// Does not allocate payloads, duplicate GRDG ownership, or invent provider
// completion. The caller supplies stable, aligned STREAMING-domain storage.
class SIRStreamingGRDGAdapter
{
public:
	SIRStreamingGRDGAdapter( void ) noexcept;
	SIRStreamingGRDGAdapter( const SIRStreamingGRDGAdapter & ) = delete;
	~SIRStreamingGRDGAdapter( void ) noexcept;

	SIRStreamingGRDGAdapter &operator=( const SIRStreamingGRDGAdapter & ) = delete;

	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	sirStreamingResult_t initialize( const sirStreamingGRDGConfig_t &config ) noexcept;
	sirStreamingResult_t bindBridge( SIRStreamingProviderBridge *pBridge ) noexcept;
	sirStreamingResult_t shutdown( void ) noexcept;

	sirStreamingProviderCallbacks_t getBridgeCallbacks( void ) noexcept;
	sirStreamingResult_t pump( uint32_t nMaximumSlots, sirStreamingGRDGStats_t *pStats ) noexcept;
	sirStreamingResult_t queryResource( sirStreamingTicket_t ticket, sirStreamingGRDGResident_t *pResident ) const noexcept;
	sirStreamingResult_t acknowledgeActivated( sirStreamingTicket_t ticket, bool bSucceeded ) noexcept;
	sirStreamingResult_t acknowledgeRetired( sirStreamingTicket_t ticket ) noexcept;

protected:
private:
	static bool beginPrepareCallback( void *pContext, sirStreamingTicket_t ticket, const sirStreamingDemand_t *pDemand ) noexcept;
	static bool cancelPrepareCallback( void *pContext, sirStreamingTicket_t ticket ) noexcept;
	static bool beginActivateCallback( void *pContext, sirStreamingTicket_t ticket ) noexcept;
	static bool beginRetireCallback( void *pContext, sirStreamingTicket_t ticket ) noexcept;

	const sirStreamingGRDGBinding_t *findBinding( const sirStreamingUnitKey_t &key ) const noexcept;
	bool isOtherLoadActive( uint32_t iExclude, const SIRResourceKey &key ) const noexcept;
	bool isCurrent( sirStreamingTicket_t ticket ) const noexcept;
	bool beginPrepare( sirStreamingTicket_t ticket, const sirStreamingDemand_t *pDemand ) noexcept;
	bool cancelPrepare( sirStreamingTicket_t ticket ) noexcept;
	bool beginActivate( sirStreamingTicket_t ticket ) noexcept;
	bool beginRetire( sirStreamingTicket_t ticket ) noexcept;
	SIRResourceResult startLoad( sirStreamingGRDGSlot_t &slot ) noexcept;
	bool checkPublishedRevision( sirStreamingGRDGSlot_t &slot ) noexcept;
	uint8_t probeResident( sirStreamingGRDGSlot_t &slot ) noexcept;
	void processSlot( uint32_t iSlot, sirStreamingGRDGStats_t &stats ) noexcept;

	SIRResourceGraph *m_pGraph;
	SIRTaskScheduler *m_pScheduler;
	SIRStreamingCoordinator *m_pCoordinator;
	SIRStreamingProviderBridge *m_pBridge;
	const sirStreamingGRDGBinding_t *m_pBindings;
	sirStreamingGRDGSlot_t *m_pSlots;
	sirStreamingGRDGProviderCallbacks_t m_Callbacks;
	uint32_t m_nBindingCount;
	uint32_t m_nCapacity;
	uint32_t m_nProviderId;
	uint32_t m_iNextSlot;
	bool m_bInitialized;
};

#endif // __SIRENGINE_SIRSTREAMINGGRDGADAPTER_H__
