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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURERESOURCEPLAN_H__
#define __SIRENGINE_SIRVIRTUALTEXTURERESOURCEPLAN_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

// Donor P3 format-family physical storage; one 136x136 sampler2DArray bank
// per family, with the physical slot encoded by its array layer. No native
// resource handle, residency ownership or graphics-driver ABI is introduced.
// The physical bank specialization requires all pages to fit in one array.
typedef struct sirVirtualTextureResourceLimits_s
{
	uint32_t nVirtualPages;
	uint32_t nAllocations;
	uint32_t nPhysicalPageSets;
	uint32_t nMaximumArrayLayers;
	uint32_t nTailAtlasWidth;
	uint32_t nTailAtlasHeight;
	uint32_t nRuntimeAllocationBytes;
	uint32_t nRuntimePageTableBytes;
} sirVirtualTextureResourceLimits_t;

typedef struct sirVirtualTextureResourcePlan_s
{
	uint32_t nPhysicalPageSets;
	uint32_t nArrayBanks;
	uint64_t nAllocationBytes;
	uint64_t nPageTableBytes;
	uint64_t szPhysicalBytes[SIR_VT_PHYSICAL_COUNT];
	uint64_t szTailBytes[SIR_VT_PHYSICAL_COUNT];
	uint64_t nTotalBufferBytes;
	uint64_t nTotalTextureBytes;
	uint64_t nTotalBytes;
} sirVirtualTextureResourcePlan_t;

// Cold host-side budget planning from frozen donor BC1/BC7/BC5/R8/R16
// family tile sizes. Failure leaves pPlan unmodified. The tail atlas is
// independently allocated once per physical family, not once per page set.
bool sirVTComputeResourcePlan( const sirVirtualTextureResourceLimits_t& limits, sirVirtualTextureResourcePlan_t *pPlan ) noexcept;

// Transactional donor-style binary search. Shrinks physical residency only;
// allocation/page table and tail capacities never shrink with the budget.
// On failure both pFittedLimits and pPlan remain unchanged.
bool sirVTFitResourcePlanToBudget( const sirVirtualTextureResourceLimits_t& limits, uint64_t nBudgetBytes,
                                   sirVirtualTextureResourceLimits_t *pFittedLimits, sirVirtualTextureResourcePlan_t *pPlan ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTURERESOURCEPLAN_H__
