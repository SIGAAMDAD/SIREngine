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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREFEEDBACK_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREFEEDBACK_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors.
// Derived from the native Terrain3D VT V1 implementation.

// Ported from Terrain3D's terrain_3d_vt_feedback_grid.h and
// terrain_3d_vt_feedback_multi.h. Native shared VT owns this contract.
// The grid stores GLOBAL virtual-page IDs, not physical cache addresses.
static constexpr uint32_t SIR_VT_FEEDBACK_GRID_MAGIC = 0x47465456U;
static constexpr uint32_t SIR_VT_FEEDBACK_GRID_VERSION = 2U;
static constexpr uint32_t SIR_VT_FEEDBACK_MULTI_VERSION = 3U;
static constexpr uint32_t SIR_VT_FEEDBACK_INVALID_PAGE = UINT32_MAX;
static constexpr uint32_t SIR_VT_FEEDBACK_SLOTS = 4U;
static constexpr uint32_t SIR_VT_FEEDBACK_HEADER_WORDS = 10U;
static constexpr uint32_t SIR_VT_FEEDBACK_MULTI_PAGES = 4U;
static constexpr uint32_t SIR_VT_FEEDBACK_MULTI_RECORD_WORDS = 9U;
static constexpr uint32_t SIR_VT_FEEDBACK_MULTI_MAX_LAYERS = 8U;
static constexpr uint32_t SIR_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL = 256U;

typedef struct sirVirtualTextureFeedbackHeader_s
{
	uint32_t nMagic;
	uint32_t nVersion;
	uint32_t nSlot;
	uint32_t nFrame;
	uint32_t nFeedbackEpoch;
	uint32_t nDatabaseGeneration;
	uint32_t nDeviceGeneration;
	uint32_t nWordCount;
	uint64_t nViewportKey;
} sirVirtualTextureFeedbackHeader_t;

typedef struct sirVirtualTextureFeedbackLayout_s
{
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nSlotBytes;
	uint32_t nTotalBytes;
} sirVirtualTextureFeedbackLayout_t;

typedef struct sirVirtualTextureFeedbackGridParams_s
{
	uint32_t nGridWidth;
	uint32_t nAllocationCount;
	uint32_t nPageCount;
	uint32_t nFlags;
	uint32_t nPredictedTerrainMaterials;
	uint32_t nGridHeightAndViewportWidth;
	uint32_t nWriteSlotAndViewportHeight;
	uint32_t nSlotStrideWords;
} sirVirtualTextureFeedbackGridParams_t;

typedef struct sirVirtualTextureFeedbackMultiRecord_s
{
	uint32_t szPages[SIR_VT_FEEDBACK_MULTI_PAGES];
	uint32_t szCounts[SIR_VT_FEEDBACK_MULTI_PAGES];
	uint32_t nOverflow;
} sirVirtualTextureFeedbackMultiRecord_t;

typedef struct sirVirtualTextureFeedbackMultiParams_s
{
	uint32_t nGridWidth;
	uint32_t nGridHeight;
	uint32_t nViewportWidth;
	uint32_t nViewportHeight;
	uint32_t nSlot;
	uint32_t nSlotStrideWords;
	uint32_t nPageCount;
	uint32_t nSourceLayers;
	uint32_t nPixelLimit;
	uint32_t nRecordWords;
	uint32_t nSlotWords;
	uint32_t nReserved;
} sirVirtualTextureFeedbackMultiParams_t;

typedef struct sirVirtualTextureFeedbackRequest_s
{
	sirVirtualTextureFeedbackLayout_t layout;
	uint32_t nViewportWidth;
	uint32_t nViewportHeight;
	uint32_t nSlot;
	uint32_t nPageCount;
	uint32_t nAllocationCount;
	uint32_t nMaterialMask;
	uint32_t nSourceLayers;
} sirVirtualTextureFeedbackRequest_t;

typedef struct sirVirtualTextureFeedbackStamp_s
{
	uint32_t nFrame;
	uint32_t nFeedbackEpoch;
	uint32_t nDatabaseGeneration;
	uint32_t nDeviceGeneration;
	uint64_t nViewportKey;
} sirVirtualTextureFeedbackStamp_t;

typedef enum sirVirtualTextureFeedbackSlotState_e : uint8_t
{
	SIR_VT_FEEDBACK_SLOT_FREE = 0,
	SIR_VT_FEEDBACK_SLOT_PREPARED = 1,
	SIR_VT_FEEDBACK_SLOT_DRAWN = 2,
	SIR_VT_FEEDBACK_SLOT_READBACK_SUBMITTED = 3,
	SIR_VT_FEEDBACK_SLOT_READY = 4
} sirVirtualTextureFeedbackSlotState_t;

typedef struct sirVirtualTextureFeedbackSlot_s
{
	sirVirtualTextureFeedbackHeader_t header;
	sirVirtualTextureFeedbackSlotState_t eState;
	bool bRetireOnCallback;
} sirVirtualTextureFeedbackSlot_t;

static_assert( sizeof( sirVirtualTextureFeedbackHeader_t ) == 40, "Terrain3D feedback header ABI" );
static_assert( offsetof( sirVirtualTextureFeedbackHeader_t, nViewportKey ) == 32, "Terrain3D viewport ABI" );
static_assert( sizeof( sirVirtualTextureFeedbackGridParams_t ) == 32, "Terrain3D grid descriptor ABI" );
static_assert( sizeof( sirVirtualTextureFeedbackMultiRecord_t ) == 36, "Terrain3D multi record ABI" );
static_assert( sizeof( sirVirtualTextureFeedbackMultiParams_t ) == 48, "Terrain3D multi descriptor ABI" );

bool sirVTFeedbackGridLayout( uint64_t nMaxSlotBytes, sirVirtualTextureFeedbackLayout_t *pLayout ) noexcept;
bool sirVTFeedbackMultiLayout( uint64_t nMaxSlotBytes, sirVirtualTextureFeedbackLayout_t *pLayout ) noexcept;
bool sirVTFeedbackGridParams( const sirVirtualTextureFeedbackRequest_t& request, sirVirtualTextureFeedbackGridParams_t *pParams ) noexcept;
bool sirVTFeedbackMultiParams( const sirVirtualTextureFeedbackRequest_t& request,
                               sirVirtualTextureFeedbackMultiParams_t *pParams ) noexcept;
bool sirVTFeedbackMakeHeader( const sirVirtualTextureFeedbackRequest_t& request, const sirVirtualTextureFeedbackStamp_t& stamp,
                              sirVirtualTextureFeedbackHeader_t *pHeader ) noexcept;
bool sirVTFeedbackHeaderMatches( const sirVirtualTextureFeedbackHeader_t& expected,
                                 const sirVirtualTextureFeedbackHeader_t& received ) noexcept;
void sirVTFeedbackResetSlot( sirVirtualTextureFeedbackSlot_t *pSlot ) noexcept;
void sirVTFeedbackInvalidateSlot( sirVirtualTextureFeedbackSlot_t *pSlot ) noexcept;
bool sirVTFeedbackPrepareSlot( sirVirtualTextureFeedbackSlot_t *pSlot, const sirVirtualTextureFeedbackHeader_t& header ) noexcept;
bool sirVTFeedbackMarkDrawn( sirVirtualTextureFeedbackSlot_t *pSlot ) noexcept;
bool sirVTFeedbackSubmitSlot( sirVirtualTextureFeedbackSlot_t *pSlot, bool bAccepted ) noexcept;
bool sirVTFeedbackCompleteSlot( sirVirtualTextureFeedbackSlot_t *pSlot, const sirVirtualTextureFeedbackHeader_t& received ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREFEEDBACK_H__
