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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURETILECOOKER_H__
#define __SIRENGINE_SIRVIRTUALTEXTURETILECOOKER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

static constexpr uint32_t SIR_VT_COOK_MAX_MIPS = 16U;

typedef enum sirVirtualTextureCookWrap_e : uint32_t
{
	SIR_VT_COOK_WRAP_REPEAT = 0,
	SIR_VT_COOK_WRAP_CLAMP = 1
} sirVirtualTextureCookWrap_t;

typedef struct sirVirtualTextureCookMipLevel_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint64_t nOffset;
	uint64_t nBytes;
} sirVirtualTextureCookMipLevel_t;

typedef struct sirVirtualTextureCookMipLayout_s
{
	sirVirtualTextureCookMipLevel_t szLevels[SIR_VT_COOK_MAX_MIPS];
	uint32_t nMipCount;
	uint32_t nComponents;
	uint64_t nTotalBytes;
} sirVirtualTextureCookMipLayout_t;

typedef struct sirVirtualTextureCookTileRequest_s
{
	const uint8_t *pMipPixels;
	uint64_t nMipBytes;
	uint32_t nMipWidth;
	uint32_t nMipHeight;
	uint32_t nComponents;
	uint32_t nPageX;
	uint32_t nPageY;
	sirVirtualTextureCookWrap_t eWrapMode;
} sirVirtualTextureCookTileRequest_t;

// Exact Terrain3D Sparse V1 CPU cooker primitives; the caller owns all memory.
// Mip dimensions use floor division, with a minimum dimension of one.
uint32_t sirVTCountCookedPages( uint32_t nDimension ) noexcept;
bool sirVTMeasureCookedMips( uint32_t nWidth, uint32_t nHeight, uint32_t nComponents, sirVirtualTextureCookMipLayout_t *pLayout ) noexcept;
bool sirVTBuildCookedMips( const sirVirtualTextureCookMipLayout_t& layout, uint8_t *pImageBytes, uint64_t nImageBytes ) noexcept;
bool sirVTBuildCookedTile( const sirVirtualTextureCookTileRequest_t& request, uint8_t *pDestination, uint64_t nDestinationBytes ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTURETILECOOKER_H__
