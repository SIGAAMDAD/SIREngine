/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D
contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREPACKLOOKUP_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREPACKLOOKUP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackReader.h>

// Bounded, unaligned-safe record access from a validated Sparse V1 view.
// These functions do not access payload data or retain any pointers.
// The caller owns the mapped metadata for the lifetime of the view.
bool sirVTPackGetAllocation( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                             sirVirtualTexturePackAllocationRecord_t *pRecord ) noexcept;
bool sirVTPackGetAllocationKey( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                                sirVirtualTexturePackAllocationKey_t *pRecord ) noexcept;
bool sirVTPackGetChannelAllocation( const sirVirtualTexturePackView_t& view, uint32_t nAllocation, uint32_t nChannel,
                                    sirVirtualTexturePackChannelAllocation_t *pRecord ) noexcept;
bool sirVTPackGetPayload( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                          sirVirtualTexturePackPayloadRecord_t *pRecord ) noexcept;
bool sirVTPackGetCluster( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                          sirVirtualTexturePackClusterRecord_t *pRecord ) noexcept;
bool sirVTPackGetTail( const sirVirtualTexturePackView_t& view, uint32_t nIndex, sirVirtualTexturePackTailRecord_t *pRecord ) noexcept;
bool sirVTPackGetQualityView( const sirVirtualTexturePackView_t& view, uint32_t nIndex,
                              sirVirtualTexturePackQualityView_t *pRecord ) noexcept;
bool sirVTPackGetFamily( const sirVirtualTexturePackView_t& view, uint32_t nChannel,
                         sirVirtualTexturePackFamilyRecord_t *pRecord ) noexcept;

// Source-pinned Terrain3D Sparse V1 stable-key binary search. A miss returns UINT32_MAX.
bool sirVTPackFindAllocationByStableKey( const sirVirtualTexturePackView_t& view, uint64_t nStableKey, uint32_t *pAllocationId ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREPACKLOOKUP_H__
