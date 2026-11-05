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

#include <Renderer/VirtualTexturing/SIRVirtualTextureFeedback.h>

static bool sirVTFeedbackSelectLayout( uint64_t nMaxSlotBytes, uint32_t nRecordBytes, sirVirtualTextureFeedbackLayout_t *pLayout ) noexcept
{
	if ( pLayout == nullptr ) {
		return false;
	}
	static constexpr uint32_t SIR_VT_GRID_DIMENSIONS[4][2] = { { 240U, 136U }, { 160U, 90U }, { 128U, 72U }, { 32U, 24U } };
	for ( uint32_t i = 0; i < 4; ++i ) {
		const uint64_t nSlot = sizeof( sirVirtualTextureFeedbackHeader_t ) +
		                       ( uint64_t )SIR_VT_GRID_DIMENSIONS[i][0] * SIR_VT_GRID_DIMENSIONS[i][1] * nRecordBytes;
		if ( nSlot <= nMaxSlotBytes && nSlot <= UINT32_MAX / SIR_VT_FEEDBACK_SLOTS ) {
			*pLayout = { SIR_VT_GRID_DIMENSIONS[i][0], SIR_VT_GRID_DIMENSIONS[i][1], ( uint32_t )nSlot,
			             ( uint32_t )( nSlot * SIR_VT_FEEDBACK_SLOTS ) };
			return true;
		}
	}
	*pLayout = {};
	return false;
}

bool sirVTFeedbackGridLayout( uint64_t nMaxSlotBytes, sirVirtualTextureFeedbackLayout_t *pLayout ) noexcept
{
	return sirVTFeedbackSelectLayout( nMaxSlotBytes, sizeof( uint32_t ), pLayout );
}

bool sirVTFeedbackMultiLayout( uint64_t nMaxSlotBytes, sirVirtualTextureFeedbackLayout_t *pLayout ) noexcept
{
	return sirVTFeedbackSelectLayout( nMaxSlotBytes, sizeof( sirVirtualTextureFeedbackMultiRecord_t ), pLayout );
}

bool sirVTFeedbackGridParams( const sirVirtualTextureFeedbackRequest_t& request, sirVirtualTextureFeedbackGridParams_t *pParams ) noexcept
{
	const sirVirtualTextureFeedbackLayout_t& layout = request.layout;
	const uint64_t nExpectedSlot = sizeof( sirVirtualTextureFeedbackHeader_t ) + ( uint64_t )layout.nWidth * layout.nHeight * 4U;
	if ( pParams == nullptr || layout.nWidth == 0 || layout.nHeight == 0 || layout.nHeight > UINT16_MAX || request.nViewportWidth == 0 ||
	     request.nViewportWidth > UINT16_MAX || request.nViewportHeight == 0 || request.nViewportHeight > UINT16_MAX ||
	     request.nSlot >= SIR_VT_FEEDBACK_SLOTS || request.nAllocationCount == 0 || request.nPageCount == 0 ||
	     nExpectedSlot != layout.nSlotBytes || ( uint64_t )layout.nTotalBytes != ( uint64_t )layout.nSlotBytes * SIR_VT_FEEDBACK_SLOTS ) {
		return false;
	}
	*pParams = { layout.nWidth,
	             request.nAllocationCount,
	             request.nPageCount,
	             1U,
	             request.nMaterialMask,
	             ( request.nViewportWidth << 16U ) | layout.nHeight,
	             ( request.nViewportHeight << 16U ) | request.nSlot,
	             layout.nSlotBytes / 4U };
	return true;
}

bool sirVTFeedbackMultiParams( const sirVirtualTextureFeedbackRequest_t& request, sirVirtualTextureFeedbackMultiParams_t *pParams ) noexcept
{
	const sirVirtualTextureFeedbackLayout_t& layout = request.layout;
	const uint64_t nExpectedWords =
	    ( uint64_t )layout.nWidth * layout.nHeight * SIR_VT_FEEDBACK_MULTI_RECORD_WORDS + SIR_VT_FEEDBACK_HEADER_WORDS;
	if ( pParams == nullptr || layout.nWidth == 0 || layout.nHeight == 0 || layout.nWidth > 240 || layout.nHeight > 136 ||
	     request.nViewportWidth == 0 || request.nViewportHeight == 0 || request.nViewportWidth > UINT16_MAX ||
	     request.nViewportHeight > UINT16_MAX || request.nSlot >= SIR_VT_FEEDBACK_SLOTS || request.nPageCount == 0 ||
	     request.nSourceLayers == 0 || request.nSourceLayers > SIR_VT_FEEDBACK_MULTI_MAX_LAYERS ||
	     ( uint64_t )( ( request.nViewportWidth + layout.nWidth - 1U ) / layout.nWidth ) *
	             ( ( request.nViewportHeight + layout.nHeight - 1U ) / layout.nHeight ) >
	         SIR_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL ||
	     nExpectedWords > UINT32_MAX || nExpectedWords * 4U != layout.nSlotBytes ||
	     ( uint64_t )layout.nTotalBytes != ( uint64_t )layout.nSlotBytes * SIR_VT_FEEDBACK_SLOTS ) {
		return false;
	}
	*pParams = { layout.nWidth,
	             layout.nHeight,
	             request.nViewportWidth,
	             request.nViewportHeight,
	             request.nSlot,
	             ( uint32_t )nExpectedWords,
	             request.nPageCount,
	             request.nSourceLayers,
	             SIR_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL,
	             SIR_VT_FEEDBACK_MULTI_RECORD_WORDS,
	             ( uint32_t )nExpectedWords,
	             0U };
	return true;
}

bool sirVTFeedbackMakeHeader( const sirVirtualTextureFeedbackRequest_t& request, const sirVirtualTextureFeedbackStamp_t& stamp,
                              sirVirtualTextureFeedbackHeader_t *pHeader ) noexcept
{
	if ( pHeader == nullptr ) {
		return false;
	}
	if ( request.nSourceLayers != 0 ) {
		sirVirtualTextureFeedbackMultiParams_t params = {};
		if ( !sirVTFeedbackMultiParams( request, &params ) ) {
			return false;
		}
		*pHeader = { SIR_VT_FEEDBACK_GRID_MAGIC,
		             SIR_VT_FEEDBACK_MULTI_VERSION,
		             request.nSlot,
		             stamp.nFrame,
		             stamp.nFeedbackEpoch,
		             stamp.nDatabaseGeneration,
		             stamp.nDeviceGeneration,
		             ( uint32_t )( ( uint64_t )params.nGridWidth * params.nGridHeight * SIR_VT_FEEDBACK_MULTI_RECORD_WORDS ),
		             stamp.nViewportKey };
		return true;
	}
	sirVirtualTextureFeedbackGridParams_t params = {};
	if ( !sirVTFeedbackGridParams( request, &params ) ) {
		return false;
	}
	*pHeader = {
	    SIR_VT_FEEDBACK_GRID_MAGIC, SIR_VT_FEEDBACK_GRID_VERSION, request.nSlot,           stamp.nFrame,
	    stamp.nFeedbackEpoch,       stamp.nDatabaseGeneration,    stamp.nDeviceGeneration, request.layout.nWidth * request.layout.nHeight,
	    stamp.nViewportKey };
	return true;
}

bool sirVTFeedbackHeaderMatches( const sirVirtualTextureFeedbackHeader_t& expected,
                                 const sirVirtualTextureFeedbackHeader_t& received ) noexcept
{
	return expected.nMagic == SIR_VT_FEEDBACK_GRID_MAGIC && expected.nMagic == received.nMagic &&
	       ( expected.nVersion == SIR_VT_FEEDBACK_GRID_VERSION || expected.nVersion == SIR_VT_FEEDBACK_MULTI_VERSION ) &&
	       expected.nVersion == received.nVersion && expected.nSlot == received.nSlot && expected.nFrame == received.nFrame &&
	       expected.nFeedbackEpoch == received.nFeedbackEpoch && expected.nDatabaseGeneration == received.nDatabaseGeneration &&
	       expected.nDeviceGeneration == received.nDeviceGeneration && expected.nWordCount == received.nWordCount &&
	       expected.nViewportKey == received.nViewportKey;
}

void sirVTFeedbackResetSlot( sirVirtualTextureFeedbackSlot_t *pSlot ) noexcept
{
	if ( pSlot == nullptr ) {
		return;
	}
	*pSlot = {};
}

void sirVTFeedbackInvalidateSlot( sirVirtualTextureFeedbackSlot_t *pSlot ) noexcept
{
	if ( pSlot == nullptr ) {
		return;
	}
	if ( pSlot->eState == SIR_VT_FEEDBACK_SLOT_READBACK_SUBMITTED ) {
		pSlot->bRetireOnCallback = true;
	} else {
		sirVTFeedbackResetSlot( pSlot );
	}
}

bool sirVTFeedbackPrepareSlot( sirVirtualTextureFeedbackSlot_t *pSlot, const sirVirtualTextureFeedbackHeader_t& header ) noexcept
{
	if ( pSlot == nullptr || pSlot->eState != SIR_VT_FEEDBACK_SLOT_FREE || header.nMagic != SIR_VT_FEEDBACK_GRID_MAGIC ||
	     ( header.nVersion != SIR_VT_FEEDBACK_GRID_VERSION && header.nVersion != SIR_VT_FEEDBACK_MULTI_VERSION ) ||
	     header.nSlot >= SIR_VT_FEEDBACK_SLOTS || header.nWordCount == 0 ) {
		return false;
	}
	pSlot->header = header;
	pSlot->eState = SIR_VT_FEEDBACK_SLOT_PREPARED;
	pSlot->bRetireOnCallback = false;
	return true;
}

bool sirVTFeedbackMarkDrawn( sirVirtualTextureFeedbackSlot_t *pSlot ) noexcept
{
	if ( pSlot == nullptr || pSlot->eState != SIR_VT_FEEDBACK_SLOT_PREPARED ) {
		return false;
	}
	pSlot->eState = SIR_VT_FEEDBACK_SLOT_DRAWN;
	return true;
}

bool sirVTFeedbackSubmitSlot( sirVirtualTextureFeedbackSlot_t *pSlot, bool bAccepted ) noexcept
{
	if ( pSlot == nullptr || pSlot->eState != SIR_VT_FEEDBACK_SLOT_DRAWN ) {
		return false;
	}
	if ( !bAccepted ) {
		sirVTFeedbackResetSlot( pSlot );
		return false;
	}
	pSlot->eState = SIR_VT_FEEDBACK_SLOT_READBACK_SUBMITTED;
	return true;
}

bool sirVTFeedbackCompleteSlot( sirVirtualTextureFeedbackSlot_t *pSlot, const sirVirtualTextureFeedbackHeader_t& received ) noexcept
{
	if ( pSlot == nullptr || pSlot->eState != SIR_VT_FEEDBACK_SLOT_READBACK_SUBMITTED ||
	     !sirVTFeedbackHeaderMatches( pSlot->header, received ) ) {
		return false;
	}
	if ( pSlot->bRetireOnCallback ) {
		sirVTFeedbackResetSlot( pSlot );
		return false;
	}
	pSlot->eState = SIR_VT_FEEDBACK_SLOT_READY;
	return true;
}
