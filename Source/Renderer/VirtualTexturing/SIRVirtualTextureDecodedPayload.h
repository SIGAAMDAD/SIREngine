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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREDECODEDPAYLOAD_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREDECODEDPAYLOAD_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackLookup.h>

typedef struct sirVirtualTextureDecodedCluster_s
{
	const uint8_t *pBytes;
	uint32_t nBytes;
	uint32_t nClusterId;
} sirVirtualTextureDecodedCluster_t;

typedef struct sirVirtualTextureDecodedPayload_s
{
	const uint8_t *pBytes;
	uint32_t nBytes;
	sirVirtualTexturePhysicalFamily_t eFormat;
	uint32_t nFlags;
} sirVirtualTextureDecodedPayload_t;

// Input must be a successfully decoded Terrain3D Sparse V1 cluster from the
// supplied package view. The view, decoded cluster and returned tile are all
// caller-owned; no payload bytes are copied or allocated by this function.
bool sirVTGetDecodedPayload( const sirVirtualTexturePackView_t& view, uint32_t nPayloadId, const sirVirtualTextureDecodedCluster_t& decoded,
                             sirVirtualTextureDecodedPayload_t *pOutput ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREDECODEDPAYLOAD_H__
