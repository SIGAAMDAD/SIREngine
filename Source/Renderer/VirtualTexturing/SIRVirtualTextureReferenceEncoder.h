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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREREFERENCEENCODER_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREREFERENCEENCODER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

// Terrain3D reference codec revisions. These are implementation revisions,
// not a change to the frozen Sparse V1 physical-family or pack format ABI.
static constexpr uint32_t SIR_VT_REFERENCE_BC1_CODEC_REVISION = 2U;
static constexpr uint32_t SIR_VT_REFERENCE_ALBEDO_CODEC_REVISION = 3U;

typedef struct sirVirtualTextureReferenceEncodeRequest_s
{
	const uint8_t *pSourcePixels;
	uint64_t nSourceBytes;
	uint8_t *pDestination;
	uint64_t nDestinationCapacity;
	sirVirtualTexturePhysicalFamily_t eFamily;
	uint32_t nComponents;
	uint8_t nAlphaThreshold;
} sirVirtualTextureReferenceEncodeRequest_t;

// Encode a 136x136 guttered raw tile to the frozen Terrain3D V1 GPU format.
// The output must not alias input, and all buffers remain caller-owned.
// On invalid input no bytes are written and written count is zeroed.
bool sirVTReferenceEncodeTile( const sirVirtualTextureReferenceEncodeRequest_t& request, uint32_t *pWrittenBytes ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREREFERENCEENCODER_H__
