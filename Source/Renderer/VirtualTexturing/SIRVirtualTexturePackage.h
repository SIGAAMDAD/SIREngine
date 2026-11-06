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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREPACKAGE_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREPACKAGE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors.
// Derived from the native Terrain3D VT V1 implementation.

// Native source port of Terrain3D Sparse VT V1 package format. This is the
// Terrain3D pack layout (not the discarded SVT1 manifest). No old-format
// decoders or alternate residency model are provided.
static constexpr uint32_t SIR_VT_PACK_MAGIC = 0x56543154U;
static constexpr uint32_t SIR_VT_PACK_VERSION = 1U;
static constexpr uint32_t SIR_VT_PACK_HEADER_BYTES = 256U;
static constexpr uint32_t SIR_VT_PACK_ALIGNMENT = 16U;
static constexpr uint32_t SIR_VT_PACK_CHANNEL_COUNT = 6U;
static constexpr uint32_t SIR_VT_PACK_QUALITY_PROFILES = 6U;
static constexpr uint32_t SIR_VT_PAGE_INTERIOR = 128U;
static constexpr uint32_t SIR_VT_PAGE_GUTTER = 4U;
static constexpr uint32_t SIR_VT_PHYSICAL_TILE = 136U;
static constexpr uint32_t SIR_VT_TAIL_LAYOUT_HORIZONTAL = 1U;
static constexpr uint32_t SIR_VT_PACK_FLAG_CLUSTERED = 1U << 0;
static constexpr uint32_t SIR_VT_PACK_FLAG_TAILS = 1U << 1;
static constexpr uint32_t SIR_VT_PACK_FLAG_GPU_READY = 1U << 2;
static constexpr uint32_t SIR_VT_PACK_FLAG_OUTER_COMPRESSION = 1U << 3;
static constexpr uint32_t SIR_VT_PACK_FLAG_QUALITY_VIEWS = 1U << 4;
static constexpr uint32_t SIR_VT_PACK_FLAG_FAMILY_MAP = 1U << 5;

typedef enum sirVirtualTexturePhysicalFamily_e : uint32_t
{
	SIR_VT_PHYSICAL_BC1 = 0,
	SIR_VT_PHYSICAL_BC7 = 1,
	SIR_VT_PHYSICAL_BC5 = 2,
	SIR_VT_PHYSICAL_R8 = 3,
	SIR_VT_PHYSICAL_R16 = 4,
	SIR_VT_PHYSICAL_COUNT = 5
} sirVirtualTexturePhysicalFamily_t;

typedef struct sirVirtualTexturePackHeader_s
{
	uint32_t nMagic;
	uint32_t nVersion;
	uint32_t nHeaderBytes;
	uint32_t nFlags;
	uint32_t nAllocationCount;
	uint32_t nChannelAllocationCount;
	uint32_t nPageCount;
	uint32_t nPayloadCount;
	uint32_t nClusterCount;
	uint32_t nTailCount;
	uint32_t nChannelCount;
	uint32_t nInteriorTexels;
	uint32_t nGutterTexels;
	uint32_t nPhysicalTexels;
	uint32_t nPackageGeneration;
	uint32_t nHeaderChecksum;
	uint32_t nQualityViewCount;
	uint32_t szChannelPageCount[SIR_VT_PACK_CHANNEL_COUNT];
	uint32_t szChannelTailCount[SIR_VT_PACK_CHANNEL_COUNT];
	uint32_t nFormatFamilyCount;
	uint32_t nFullySupportedProfileMask;
	uint64_t nAllocationOffset;
	uint64_t nAllocationKeyOffset;
	uint64_t nChannelAllocationOffset;
	uint64_t nPageOffset;
	uint64_t nPayloadDirectoryOffset;
	uint64_t nClusterDirectoryOffset;
	uint64_t nTailMetadataOffset;
	uint64_t nQualityViewOffset;
	uint64_t nFormatFamilyOffset;
	uint64_t nPayloadDataOffset;
	uint64_t nPayloadDataBytes;
	uint64_t nContentSignature;
	uint64_t nPackageBytes;
	uint64_t szReserved[3];
} sirVirtualTexturePackHeader_t;

typedef struct sirVirtualTexturePackAllocationRecord_s
{
	uint32_t nGeneration;
	uint32_t nContentId;
	uint16_t nFlags;
	uint16_t nChannelMask;
	uint8_t nHighestProfile;
	uint8_t nAvailableProfiles;
	uint8_t nConsumer;
	uint8_t nReserved;
} sirVirtualTexturePackAllocationRecord_t;

typedef struct sirVirtualTexturePackAllocationKey_s
{
	uint64_t nStableKey;
	uint32_t nAllocationId;
	uint32_t nContentId;
} sirVirtualTexturePackAllocationKey_t;

typedef struct sirVirtualTexturePackChannelAllocation_s
{
	uint32_t nBasePage;
	uint32_t nPageCount;
	uint32_t nTailIndex;
	uint16_t nMip0WidthPages;
	uint16_t nMip0HeightPages;
	uint16_t nMipCount;
	uint16_t nFlags;
} sirVirtualTexturePackChannelAllocation_t;

typedef struct sirVirtualTexturePackPageRecord_s
{
	uint32_t nVirtualPage;
	uint32_t nParentPage;
	uint32_t nAllocationId;
	uint32_t nPayloadId;
	uint32_t nGeneration;
	uint32_t nContentId;
	uint16_t nMip;
	uint16_t nFlags;
	uint16_t nPageX;
	uint16_t nPageY;
	uint16_t nChannel;
	uint16_t nReserved;
} sirVirtualTexturePackPageRecord_t;

typedef struct sirVirtualTexturePackPayloadRecord_s
{
	uint64_t nContentSignature;
	uint32_t nFormat;
	uint32_t nClusterId;
	uint32_t nClusterOffset;
	uint32_t nStoredBytes;
	uint32_t nDecodedBytes;
	uint32_t nCodec;
	uint32_t nChecksum;
	uint32_t nFlags;
	uint32_t nReserved;
} sirVirtualTexturePackPayloadRecord_t;

typedef struct sirVirtualTexturePackClusterRecord_s
{
	uint64_t nPayloadDataOffset;
	uint64_t nContentSignature;
	uint32_t nStoredBytes;
	uint32_t nDecodedBytes;
	uint32_t nFirstPayload;
	uint32_t nPayloadCount;
	uint32_t nCodec;
	uint32_t nChecksum;
} sirVirtualTexturePackClusterRecord_t;

typedef struct sirVirtualTexturePackTailRecord_s
{
	uint64_t nContentSignature;
	uint32_t nCanonicalAllocationId;
	uint32_t nPayloadId;
	uint32_t nContentId;
	uint16_t nFirstMip;
	uint16_t nMipCount;
	uint16_t nFirstMipWidth;
	uint16_t nFirstMipHeight;
	uint16_t nAtlasInteriorWidth;
	uint16_t nAtlasInteriorHeight;
	uint16_t nChannel;
	uint16_t nFlags;
	uint32_t nLayout;
} sirVirtualTexturePackTailRecord_t;

typedef struct sirVirtualTexturePackQualityView_s
{
	uint32_t nAllocationId;
	uint8_t nProfile;
	uint8_t nChannelMask;
	uint8_t szBaseMip[SIR_VT_PACK_CHANNEL_COUNT];
	uint8_t szMaxMip[SIR_VT_PACK_CHANNEL_COUNT];
} sirVirtualTexturePackQualityView_t;

typedef struct sirVirtualTexturePackFamilyRecord_s
{
	uint8_t nChannel;
	uint8_t nFamily;
	uint8_t nSemantic;
	uint8_t nFlags;
	uint32_t nReserved;
} sirVirtualTexturePackFamilyRecord_t;

typedef struct sirVirtualTexturePackSpan_s
{
	const uint8_t *pData;
	uint64_t nBytes;
	uint64_t nFileOffset;
} sirVirtualTexturePackSpan_t;

typedef enum sirVirtualTexturePackDirectory_e : uint32_t
{
	SIR_VT_PACK_ALLOCATIONS = 0,
	SIR_VT_PACK_ALLOCATION_KEYS,
	SIR_VT_PACK_CHANNEL_ALLOCATIONS,
	SIR_VT_PACK_PAGES,
	SIR_VT_PACK_PAYLOADS,
	SIR_VT_PACK_CLUSTERS,
	SIR_VT_PACK_TAILS,
	SIR_VT_PACK_QUALITY_VIEWS,
	SIR_VT_PACK_FAMILIES,
	SIR_VT_PACK_PAYLOAD_DATA,
	SIR_VT_PACK_DIRECTORY_COUNT
} sirVirtualTexturePackDirectory_t;

typedef struct sirVirtualTexturePackInput_s
{
	const void *pBytes;
	uint64_t nAvailableBytes;
	uint64_t nFileBytes;
	bool bMetadataOnly;
} sirVirtualTexturePackInput_t;

typedef struct sirVirtualTexturePackView_s
{
	sirVirtualTexturePackHeader_t header;
	sirVirtualTexturePackSpan_t szDirectories[SIR_VT_PACK_DIRECTORY_COUNT];
	bool bMetadataOnly;
} sirVirtualTexturePackView_t;

typedef struct sirVirtualTextureTailRequest_s
{
	uint32_t nNextHalf;
	uint32_t nSlotsPerRow;
	uint32_t nSlotWidth;
	uint32_t nSlotHeight;
	bool bCompact;
} sirVirtualTextureTailRequest_t;

typedef struct sirVirtualTextureTailPlacement_s
{
	uint32_t nNextHalf;
	uint32_t nAtlasX;
	uint32_t nAtlasY;
} sirVirtualTextureTailPlacement_t;

static_assert( sizeof( sirVirtualTexturePackHeader_t ) == 256, "Terrain3D Sparse V1 pack header ABI" );
static_assert( sizeof( sirVirtualTexturePackAllocationRecord_t ) == 16, "Terrain3D V1 allocation ABI" );
static_assert( sizeof( sirVirtualTexturePackAllocationKey_t ) == 16, "Terrain3D V1 stable key ABI" );
static_assert( sizeof( sirVirtualTexturePackChannelAllocation_t ) == 20, "Terrain3D V1 channel allocation ABI" );
static_assert( sizeof( sirVirtualTexturePackPageRecord_t ) == 36, "Terrain3D V1 page record ABI" );
static_assert( sizeof( sirVirtualTexturePackPayloadRecord_t ) == 48, "Terrain3D V1 payload ABI" );
static_assert( sizeof( sirVirtualTexturePackClusterRecord_t ) == 40, "Terrain3D V1 cluster ABI" );
static_assert( sizeof( sirVirtualTexturePackTailRecord_t ) == 40, "Terrain3D V1 tail ABI" );
static_assert( sizeof( sirVirtualTexturePackQualityView_t ) == 20, "Terrain3D V1 quality ABI" );
static_assert( sizeof( sirVirtualTexturePackFamilyRecord_t ) == 8, "Terrain3D V1 family map ABI" );

uint64_t sirVTPackHash64( const void *pBytes, uint64_t nBytes ) noexcept;
uint32_t sirVTPackChecksum32( const void *pBytes, uint64_t nBytes ) noexcept;
uint32_t sirVTPackPhysicalTileBytes( sirVirtualTexturePhysicalFamily_t eFamily ) noexcept;
// Structural inspection only; per-record semantic validation is a separate port stage.
// Borrowed views require the package bytes to remain alive; metadata-only views
// expose only the directories, never a payload-data pointer.
bool sirVTPackOpenDirectories( const sirVirtualTexturePackInput_t& input, sirVirtualTexturePackView_t *pView ) noexcept;
// Ported verbatim in behavior from terrain_3d_vt_compact_tail_layout.h.
bool sirVTPlaceCompactTail( const sirVirtualTextureTailRequest_t& request, sirVirtualTextureTailPlacement_t *pPlacement ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREPACKAGE_H__
