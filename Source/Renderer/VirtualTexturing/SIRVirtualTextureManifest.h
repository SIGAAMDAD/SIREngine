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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREMANIFEST_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREMANIFEST_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTextureResidency.h>

static constexpr uint32_t SIR_VT_MANIFEST_MAGIC = 0x31545653U; // "SVT1" little-endian
static constexpr uint16_t SIR_VT_MANIFEST_VERSION = 1;
static constexpr uint16_t SIR_VT_MANIFEST_HEADER_BYTES = 80;
static constexpr uint32_t SIR_VT_MANIFEST_RECORD_BYTES = 32;
static constexpr uint32_t SIR_VT_MAX_MIPS = 32;

typedef enum sirVirtualTextureCodec_e : uint32_t
{
	SIR_VT_CODEC_RGBA8 = 1,
	SIR_VT_CODEC_BC1 = 2,
	SIR_VT_CODEC_BC3 = 3,
	SIR_VT_CODEC_BC7 = 4
} sirVirtualTextureCodec_t;

typedef enum sirVirtualTextureManifestResult_e : uint8_t
{
	SIR_VT_MANIFEST_SUCCESS = 0,
	SIR_VT_MANIFEST_INVALID_ARGUMENT = 1,
	SIR_VT_MANIFEST_OVERFLOW = 2,
	SIR_VT_MANIFEST_TOO_SMALL = 3,
	SIR_VT_MANIFEST_CORRUPT = 4,
	SIR_VT_MANIFEST_UNSUPPORTED = 5
} sirVirtualTextureManifestResult_t;

// The entire cooked texture shares a single immutable GRDG identity/revision.
// nInteriorSize excludes its nBorder texels on each side. Every tile stores
// the same padded physical extent, including tiles at mip and image edges.
typedef struct sirVirtualTextureLayout_s
{
	sirResourceKey_t source;
	uint64_t nRevision;
	uint32_t nWidth;
	uint32_t nHeight;
	uint16_t nInteriorSize;
	uint16_t nBorder;
	uint16_t nMipCount;
	uint16_t nLayerCount;
	sirVirtualTextureCodec_t eCodec;
} sirVirtualTextureLayout_t;

// Input tiles must be in canonical layer,mip,y,x order. Texture compression
// and gutter generation happen upstream; this stage stores final GPU-ready
// bytes only. Output memory must not alias any input tile data.
typedef struct sirVirtualTextureCookPage_s
{
	const uint8_t *pBytes;
	uint32_t nByteCount;
	uint32_t nX;
	uint32_t nY;
	uint16_t nMip;
	uint16_t nLayer;
} sirVirtualTextureCookPage_t;

typedef struct sirVirtualTextureMutableBuffer_s
{
	void *pBytes;
	uint64_t nCapacity;
} sirVirtualTextureMutableBuffer_t;

// One authored mip image, tightly packed or row-pitched RGBA8. Input is
// read-only; only the copied, clamped tile texels enter the cooked package.
typedef struct sirVirtualTextureImage_s
{
	const uint8_t *pBytes;
	uint64_t nByteCount;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nRowPitch;
} sirVirtualTextureImage_t;

// Zero-copy validated view. Caller retains the immutable encoded bytes.
// Metadata uses explicit endian conversion; no host structure is serialized.
typedef struct sirVirtualTextureManifestView_s
{
	const uint8_t *pBytes;
	uint64_t nByteCount;
	sirVirtualTextureLayout_t layout;
	uint64_t nPayloadOffset;
	uint32_t nPageCount;
	uint32_t nPagesPerLayer;
	uint32_t szMipPageStarts[SIR_VT_MAX_MIPS];
	uint32_t szMipColumns[SIR_VT_MAX_MIPS];
	uint32_t szMipRows[SIR_VT_MAX_MIPS];
} sirVirtualTextureManifestView_t;

typedef struct sirVirtualTexturePageView_s
{
	sirVirtualTexturePageKey_t key;
	const uint8_t *pBytes;
	uint64_t nOffset;
	uint32_t nByteCount;
	uint64_t nContentHash;
} sirVirtualTexturePageView_t;

typedef struct sirVirtualTextureSample_s
{
	sirVirtualTextureHandle_t handle;
	sirVirtualTexturePageKey_t key;
	uint16_t nFallbackLevels;
} sirVirtualTextureSample_t;

sirVirtualTextureManifestResult_t sirVTMeasureManifest( const sirVirtualTextureLayout_t& layout, uint32_t *pCount,
                                                        uint64_t *pBytes ) noexcept;
sirVirtualTextureManifestResult_t sirVTBuildRGBA8Tile( const sirVirtualTextureLayout_t& layout, const sirVirtualTexturePageKey_t& key,
                                                       const sirVirtualTextureImage_t& image,
                                                       sirVirtualTextureMutableBuffer_t output ) noexcept;
sirVirtualTextureManifestResult_t sirVTCookManifest( const sirVirtualTextureLayout_t& layout, const sirVirtualTextureCookPage_t *pPages,
                                                     uint32_t nCount, sirVirtualTextureMutableBuffer_t output ) noexcept;
sirVirtualTextureManifestResult_t sirVTParseManifest( const void *pBytes, uint64_t nBytes,
                                                      sirVirtualTextureManifestView_t *pView ) noexcept;
sirVirtualTextureManifestResult_t sirVTGetManifestPage( const sirVirtualTextureManifestView_t& view, const sirVirtualTexturePageKey_t& key,
                                                        sirVirtualTexturePageView_t *pPage ) noexcept;
sirGraphicsResult_t sirVTResolveResidentFallback( const sirVirtualTextureManifestView_t& view, const sirVirtualTexturePageKey_t& key,
                                                  const SIRVirtualTextureResidency& residency,
                                                  sirVirtualTextureSample_t *pSample ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREMANIFEST_H__
