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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREPACKWRITER_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREPACKWRITER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackReader.h>

// Editor/cooker input for the *native Terrain3D Sparse V1 format*.
// Arrays are borrowed and must be valid for the duration of the call. Records
// and payloads are already cooked; this layer only serializes the frozen ABI.
typedef struct sirVirtualTexturePackSource_s
{
	const sirVirtualTexturePackAllocationRecord_t *pAllocations;
	const sirVirtualTexturePackAllocationKey_t *pAllocationKeys;
	const sirVirtualTexturePackChannelAllocation_t *pChannelAllocations;
	const sirVirtualTexturePackPageRecord_t *pPages;
	const sirVirtualTexturePackPayloadRecord_t *pPayloads;
	const sirVirtualTexturePackClusterRecord_t *pClusters;
	const sirVirtualTexturePackTailRecord_t *pTails;
	const sirVirtualTexturePackQualityView_t *pQualityViews;
	const sirVirtualTexturePackFamilyRecord_t *pFormatFamilies;
	const uint8_t *pPayloadData;
	uint64_t nPayloadDataBytes;
	uint32_t nAllocationCount;
	uint32_t nChannelAllocationCount;
	uint32_t nPageCount;
	uint32_t nPayloadCount;
	uint32_t nClusterCount;
	uint32_t nTailCount;
	uint32_t nQualityViewCount;
	uint32_t nFormatFamilyCount;
	uint32_t nPackageGeneration;
} sirVirtualTexturePackSource_t;

// Overflow-safe, deterministic layout measurement. Does not mutate input.
bool sirVTMeasurePack( const sirVirtualTexturePackSource_t& source, uint64_t *pMetadataBytes, uint64_t *pPackageBytes ) noexcept;

// Write exact Terrain3D V1 on-disk records and hashes. Reject invalid source
// semantics via the native V1 reader after writing; failed buffers are not
// publishable. Caller owns output and can use no intermediate allocation.
// Input directory memory must not overlap the destination; payload bytes may
// alias only their exact final location, as permitted by the donor writer.
bool sirVTWritePack( const sirVirtualTexturePackSource_t& source, void *pDestination, uint64_t nCapacity,
                     uint64_t *pWrittenBytes ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREPACKWRITER_H__
