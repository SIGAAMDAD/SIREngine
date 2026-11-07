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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREDEMANDBUNDLECODEC_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREDEMANDBUNDLECODEC_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Frozen Terrain3D baked-visibility demand sidecar, independent of the V1
// texture pack. Records are stored as 64-byte little-endian wire records.
static constexpr uint32_t SIR_VT_DEMAND_VISIBILITY_RECORD_BYTES = 64U;
static constexpr uint32_t SIR_VT_DEMAND_VISIBILITY_MAX_BUNDLE_BYTES = 1024U * 1024U;
static constexpr uint32_t SIR_VT_DEMAND_VISIBILITY_MIP_BINS = 16U;
static constexpr uint8_t SIR_VT_DEMAND_VISIBILITY_PARTIAL = 1U;
static constexpr uint8_t SIR_VT_DEMAND_VISIBILITY_NO_VISIBLE = 2U;

typedef struct sirVirtualTextureDemandVisibilityRecord_s
{
	uint64_t nStableMaterialKey;
	uint16_t szMipWeight[SIR_VT_DEMAND_VISIBILITY_MIP_BINS];
	uint32_t nSampleCount;
	uint32_t nSourceRevision;
	uint32_t nProbeCount;
	uint32_t nCoverageSampleCount;
	uint16_t nCoverageConfidence;
	uint16_t nNominalMipBase;
	uint8_t nViewBin;
	uint8_t nFlags;
} sirVirtualTextureDemandVisibilityRecord_t;

typedef struct sirVirtualTextureDemandBundle_s
{
	int32_t nX;
	int32_t nZ;
	uint64_t nOffset;
	uint64_t nHash;
	uint32_t nEncodedBytes;
	uint32_t nDecodedBytes;
	uint32_t nCodec;
	uint32_t nCount;
} sirVirtualTextureDemandBundle_t;

typedef struct sirVirtualTextureDemandBundleBuffers_s
{
	const uint8_t *pEncoded;
	uint32_t nEncodedBytes;
	uint8_t *pScratch;
	uint32_t nScratchCapacity;
	sirVirtualTextureDemandVisibilityRecord_t *pRecords;
	uint32_t nRecordCapacity;
} sirVirtualTextureDemandBundleBuffers_t;

static_assert( sizeof( sirVirtualTextureDemandBundle_t ) == 40, "Terrain3D baked demand bundle directory ABI" );

// The sidecar hash is the donor's 64-bit FNV-1a seed (distinct from the
// historical texture pack hash). A null pointer with nonzero bytes is invalid.
uint64_t sirVTDemandBundleHash( const uint8_t *pBytes, uint64_t nBytes ) noexcept;
// Decode exactly one checked baked visibility bundle into caller-owned storage.
// Codec 0 is raw; codec 1 is canonical PackBits. No allocations or I/O.
bool sirVTDemandBundleDecodeBundle( const sirVirtualTextureDemandBundle_t& bundle,
                                    const sirVirtualTextureDemandBundleBuffers_t& buffers ) noexcept;
#endif // __SIRENGINE_SIRVIRTUALTEXTUREDEMANDBUNDLECODEC_H__
