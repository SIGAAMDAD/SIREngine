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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURECLUSTERDECODE_H__
#define __SIRENGINE_SIRVIRTUALTEXTURECLUSTERDECODE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackReader.h>

// Terrain3D Sparse V1 stores outer Zstd compression on a whole cluster,
// while payload records remain uncompressed physical GPU tile descriptors.
// No second texture package ABI, resident-cache model, or heap staging.
typedef struct sirVirtualTextureCodecFrame_s
{
	const uint8_t *pInput;
	uint32_t nInputBytes;
	uint8_t *pOutput;
	uint32_t nOutputCapacity;
} sirVirtualTextureCodecFrame_t;

// Same caller-provided codec model as Terrain3D's cooked pack encoder.
// The codec owner supplies a genuine Zstd implementation; this module only
// owns frozen package layout, encoded checksums and decoded tile validation.
typedef struct sirVirtualTextureClusterCodec_s
{
	void *pContext;
	bool ( *pfnDecodeZstd )( void *pContext, const sirVirtualTextureCodecFrame_t& frame, uint32_t *pProducedBytes ) noexcept;
} sirVirtualTextureClusterCodec_t;

typedef struct sirVirtualTextureClusterBuffers_s
{
	uint8_t *pStoredBytes;
	uint32_t nStoredCapacity;
	uint8_t *pDecodedBytes;
	uint32_t nDecodedCapacity;
} sirVirtualTextureClusterBuffers_t;

// Requires a previously validated native Sparse V1 view. Caller supplies
// the exact stored cluster bytes and bounded decoded workspace. All checks
// and output publication happen synchronously; no pointers are retained.
// Failure after a decode starts clears the expected decoded byte interval.
bool sirVTDecodePackCluster( const sirVirtualTexturePackView_t& view, uint32_t nClusterId, const sirVirtualTextureClusterBuffers_t& buffers,
                             const sirVirtualTextureClusterCodec_t *pCodec ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTURECLUSTERDECODE_H__
