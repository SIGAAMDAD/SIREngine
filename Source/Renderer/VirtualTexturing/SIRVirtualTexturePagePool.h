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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREPAGEPOOL_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREPAGEPOOL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

static constexpr uint32_t SIR_VT_POOL_INVALID_INDEX = UINT32_MAX;

typedef enum sirVirtualTexturePagePoolState_e : uint8_t
{
	SIR_VT_POOL_FREE = 0,
	SIR_VT_POOL_RESERVED = 1,
	SIR_VT_POOL_UPLOADING = 2,
	SIR_VT_POOL_RESIDENT = 3,
	SIR_VT_POOL_RETIRING = 4
} sirVirtualTexturePagePoolState_t;

typedef struct sirVirtualTexturePagePoolSlot_s
{
	sirGraphicsCompletionSet_t completion;
	uint64_t nTicket;
	uint64_t nAge;
	uint32_t nVirtualPage;
	uint32_t nGeneration;
	uint32_t nPhysicalLayer;
	uint8_t nFamily;
	sirVirtualTexturePagePoolState_t eState;
} sirVirtualTexturePagePoolSlot_t;

typedef struct sirVirtualTexturePagePoolReservation_s
{
	uint32_t nSlot;
	uint32_t nVirtualPage;
	uint32_t nGeneration;
	uint64_t nTicket;
} sirVirtualTexturePagePoolReservation_t;

typedef struct sirVirtualTexturePagePoolStats_s
{
	uint32_t nFree;
	uint32_t nReserved;
	uint32_t nUploading;
	uint32_t nResident;
	uint32_t nRetiring;
	uint64_t nRejected;
	uint64_t nCancelled;
} sirVirtualTexturePagePoolStats_t;

// Single-writer canonical V1 format-family residency pool. The caller owns
// slot storage and virtual-page mapping and serializes every method call.
// Submission, device ownership and fence polling belong to the existing
// graphics-driver/GRDG APIs. Slots cannot be recycled while GPU work is
// pending, including cancelled uploads and retired resident page tables.
class SIRVirtualTexturePagePool final
{
public:
	SIRVirtualTexturePagePool( void ) noexcept;
	~SIRVirtualTexturePagePool( void ) noexcept = default;

	bool init( sirVirtualTexturePagePoolSlot_t *pSlots, uint32_t nSlotCount, uint32_t nPerFamily, uint32_t *pVirtualToSlot,
	           uint32_t nVirtualPages, uint32_t nDeviceGeneration ) noexcept;
	// Only succeeds when every slot is free and all GPU work has retired.
	bool resetCold( void ) noexcept;
	bool reserve( uint32_t nVirtualPage, sirVirtualTexturePhysicalFamily_t eFamily, uint32_t nGeneration,
	              sirVirtualTexturePagePoolReservation_t *pReservation ) noexcept;
	// Upload completion tokens must include every queue using this page.
	bool commitUpload( const sirVirtualTexturePagePoolReservation_t& reservation, const sirGraphicsCompletionSet_t& completion ) noexcept;
	// No outstanding GPU use => nullptr. Otherwise retirement is fence-gated.
	bool abortUpload( const sirVirtualTexturePagePoolReservation_t& reservation, const sirGraphicsCompletionSet_t *pCompletion ) noexcept;
	// Removing a sampled page requires fence tokens covering publication users.
	bool beginRetire( uint32_t nVirtualPage, const sirGraphicsCompletionSet_t& completion ) noexcept;
	// Pass monotonically completed driver queue values from the same device gen.
	uint32_t serviceCompletions( const sirGraphicsCompletionSet_t& completed ) noexcept;
	bool findResident( uint32_t nVirtualPage, uint32_t nGeneration, uint32_t *pSlot ) const noexcept;
	// Resolve an unavailable page through its validated donor mip parent chain.
	// Missing root yields false for the caller to use the compact-tail atlas.
	bool resolveResidentAncestor( const sirVirtualTexturePackView_t& view, uint32_t nVirtualPage, uint32_t nGeneration,
	                              uint32_t *pResolvedPage, uint16_t *pResolvedMip, uint32_t *pSlot ) const noexcept;
	sirVirtualTexturePagePoolStats_t getStats( void ) const noexcept;

private:
	bool checkReservation( const sirVirtualTexturePagePoolReservation_t& reservation ) const noexcept;
	bool checkCompletion( const sirGraphicsCompletionSet_t& completion ) const noexcept;
	bool isComplete( const sirGraphicsCompletionSet_t& pending, const sirGraphicsCompletionSet_t& completed ) const noexcept;
	void releaseSlot( uint32_t nSlot ) noexcept;

	sirVirtualTexturePagePoolSlot_t *m_pSlots = nullptr;
	uint32_t *m_pVirtualToSlot = nullptr;
	uint32_t m_nSlotCount = 0;
	uint32_t m_nPerFamily = 0;
	uint32_t m_nVirtualPages = 0;
	uint32_t m_nDeviceGeneration = 0;
	uint64_t m_nNextTicket = 0;
	uint64_t m_nUseClock = 0;
	uint64_t m_nRejected = 0;
	uint64_t m_nCancelled = 0;
};

#endif // __SIRENGINE_SIRVIRTUALTEXTUREPAGEPOOL_H__
