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

#ifndef __SIRENGINE_SIRTERRAINHEIGHTFIELDVIEW_H__
#define __SIRENGINE_SIRTERRAINHEIGHTFIELDVIEW_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Terrain/Data/SIRTerrainHeightfieldCodec.h>

typedef struct sirTerrainR16View_s
{
	sirTerrainRegionDescriptor_t descriptor;
	const uint8_t *pSamples;
	size_t nSampleBytes;
} sirTerrainR16View_t;

typedef enum sirTerrainSampleResult_e : uint8_t
{
	SIR_TERRAIN_SAMPLE_SUCCESS = 0,
	SIR_TERRAIN_SAMPLE_INVALID_VIEW,
	SIR_TERRAIN_SAMPLE_INVALID_ARGUMENT,
	SIR_TERRAIN_SAMPLE_OUT_OF_BOUNDS
} sirTerrainSampleResult_t;

// Open once inside the owner's GRDG read scope, then sample without decoding a
// duplicate float array. The borrowed pointer must remain immutable and alive.
class SIRTerrainHeightfieldView
{
public:
	static sirTerrainCodecResult_t open( sirTerrainConstByteSpan_t input, sirTerrainR16View_t& view,
	                                     uint64_t nMinimumRevision = 0 ) noexcept;
	static sirTerrainSampleResult_t read( const sirTerrainR16View_t& view, uint32_t nX, uint32_t nZ, float& fHeight ) noexcept;
	static sirTerrainSampleResult_t sampleBilinear( const sirTerrainR16View_t& view, double fCellX, double fCellZ,
	                                                float& fHeight ) noexcept;

protected:
private:
	static bool isValidView( const sirTerrainR16View_t& view ) noexcept;
	static float decodeSample( const sirTerrainR16View_t& view, uint32_t nX, uint32_t nZ ) noexcept;
};

#endif // __SIRENGINE_SIRTERRAINHEIGHTFIELDVIEW_H__
