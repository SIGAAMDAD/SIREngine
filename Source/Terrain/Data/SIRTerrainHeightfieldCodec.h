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

#ifndef __SIRENGINE_SIRTERRAINHEIGHTFIELDCODEC_H__
#define __SIRENGINE_SIRTERRAINHEIGHTFIELDCODEC_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stddef.h>
#include <stdint.h>

typedef struct sirTerrainRegionDescriptor_s
{
	int32_t iRegionX;
	int32_t iRegionZ;
	uint32_t nResolution;
	float fMinHeight;
	float fMaxHeight;
	uint64_t nRevision;
} sirTerrainRegionDescriptor_t;

typedef struct sirTerrainConstByteSpan_s
{
	const uint8_t *pData;
	size_t nCapacity;
} sirTerrainConstByteSpan_t;

typedef struct sirTerrainMutableByteSpan_s
{
	uint8_t *pData;
	size_t nCapacity;
} sirTerrainMutableByteSpan_t;

typedef struct sirTerrainConstHeightSpan_s
{
	const float *pData;
	size_t nCapacity;
} sirTerrainConstHeightSpan_t;

typedef struct sirTerrainMutableHeightSpan_s
{
	float *pData;
	size_t nCapacity;
} sirTerrainMutableHeightSpan_t;

typedef enum sirTerrainCodecResult_e : uint8_t
{
	SIR_TERRAIN_CODEC_SUCCESS = 0,
	SIR_TERRAIN_CODEC_INVALID_ARGUMENT,
	SIR_TERRAIN_CODEC_INVALID_FORMAT,
	SIR_TERRAIN_CODEC_UNSUPPORTED_VERSION,
	SIR_TERRAIN_CODEC_BUFFER_TOO_SMALL,
	SIR_TERRAIN_CODEC_BAD_CHECKSUM,
	SIR_TERRAIN_CODEC_NONFINITE_HEIGHT,
	SIR_TERRAIN_CODEC_STALE_REVISION
} sirTerrainCodecResult_t;

// Fixed 64-byte little-endian header followed by row-major R16_UNORM heights.
// Stateless, allocation-free codec; callers own lifetime and publication.
class SIRTerrainHeightfieldCodec
{
public:
	static constexpr uint32_t HEADER_BYTES = 64;
	static constexpr uint32_t MIN_RESOLUTION = 2;
	static constexpr uint32_t MAX_RESOLUTION = 4096;

	static size_t getEncodedByteCount( uint32_t nResolution ) noexcept;

	static sirTerrainCodecResult_t encode( const sirTerrainRegionDescriptor_t& descriptor, sirTerrainConstHeightSpan_t heights,
	                                       sirTerrainMutableByteSpan_t output ) noexcept;
	static sirTerrainCodecResult_t inspect( sirTerrainConstByteSpan_t input, sirTerrainRegionDescriptor_t& descriptor,
	                                        uint64_t nMinimumRevision = 0 ) noexcept;
	static sirTerrainCodecResult_t decode( sirTerrainConstByteSpan_t input, sirTerrainRegionDescriptor_t& descriptor,
	                                       sirTerrainMutableHeightSpan_t output, uint64_t nMinimumRevision = 0 ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRTERRAINHEIGHTFIELDCODEC_H__
