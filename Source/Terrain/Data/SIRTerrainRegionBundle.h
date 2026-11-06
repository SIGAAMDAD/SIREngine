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

#ifndef __SIRENGINE_SIRTERRAINREGIONBUNDLE_H__
#define __SIRENGINE_SIRTERRAINREGIONBUNDLE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Terrain/Data/SIRTerrainHeightfieldCodec.h>

typedef struct sirTerrainRegionBundleView_s
{
	const uint8_t *pBytes;
	size_t nBytes;
	uint32_t nRegionCount;
} sirTerrainRegionBundleView_t;

typedef enum sirTerrainBundleResult_e : uint8_t
{
	SIR_TERRAIN_BUNDLE_SUCCESS = 0,
	SIR_TERRAIN_BUNDLE_INVALID_ARGUMENT,
	SIR_TERRAIN_BUNDLE_INVALID_FORMAT,
	SIR_TERRAIN_BUNDLE_UNSUPPORTED_VERSION,
	SIR_TERRAIN_BUNDLE_BUFFER_TOO_SMALL,
	SIR_TERRAIN_BUNDLE_BAD_CHECKSUM,
	SIR_TERRAIN_BUNDLE_DUPLICATE_OR_UNSORTED,
	SIR_TERRAIN_BUNDLE_OVERLAPPING_BUFFERS,
	SIR_TERRAIN_BUNDLE_NOT_FOUND
} sirTerrainBundleResult_t;

// Versioned, uncompressed terrain-only region index. The caller owns all buffers.
// Embedded R16 regions must be sorted by signed (X,Z) in ascending order.
// A successful open validates the entire immutable backing span exactly once.
class SIRTerrainRegionBundle
{
public:
	static constexpr uint32_t HEADER_BYTES = 64;
	static constexpr uint32_t ENTRY_BYTES = 40;
	static constexpr uint32_t MAX_REGIONS = 65535;

	static sirTerrainBundleResult_t measure( const sirTerrainConstByteSpan_t *pRegions, uint32_t nCount, size_t& nBytes ) noexcept;
	static sirTerrainBundleResult_t write( const sirTerrainConstByteSpan_t *pRegions, uint32_t nCount, sirTerrainMutableByteSpan_t output,
	                                       size_t& nWritten ) noexcept;
	static sirTerrainBundleResult_t open( sirTerrainConstByteSpan_t input, sirTerrainRegionBundleView_t& view ) noexcept;
	static sirTerrainBundleResult_t find( const sirTerrainRegionBundleView_t& view, int32_t iRegionX, int32_t iRegionZ,
	                                      sirTerrainConstByteSpan_t& region ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRTERRAINREGIONBUNDLE_H__
