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
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

#include <stdio.h>
#include <vector>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static bool testFeedbackGrid( void ) noexcept
{
	sirVirtualTextureFeedbackLayout_t grid = {};
	SIR_TEST( sirVTFeedbackGridLayout( 130600, &grid ) );
	SIR_TEST( grid.nWidth == 240 && grid.nHeight == 136 && grid.nSlotBytes == 130600 && grid.nTotalBytes == 522400 );
	SIR_TEST( sirVTFeedbackGridLayout( 130599, &grid ) && grid.nWidth == 160 && grid.nHeight == 90 );
	SIR_TEST( !sirVTFeedbackGridLayout( 1, &grid ) && grid.nWidth == 0 );
	SIR_TEST( sirVTFeedbackGridLayout( 130600, &grid ) );

	sirVirtualTextureFeedbackRequest_t request = {};
	request.layout = grid;
	request.nViewportWidth = 1920;
	request.nViewportHeight = 1080;
	request.nSlot = 2;
	request.nAllocationCount = 70;
	request.nPageCount = 1024;
	request.nMaterialMask = 0x44;
	sirVirtualTextureFeedbackGridParams_t params = {};
	SIR_TEST( sirVTFeedbackGridParams( request, &params ) );
	SIR_TEST( params.nGridWidth == 240 && params.nAllocationCount == 70 && params.nFlags == 1 &&
	          params.nPredictedTerrainMaterials == 0x44 && params.nGridHeightAndViewportWidth == ( 1920U << 16U | 136U ) &&
	          params.nWriteSlotAndViewportHeight == ( 1080U << 16U | 2U ) && params.nSlotStrideWords == 32650 );
	sirVirtualTextureFeedbackStamp_t stamp = { 77, 8, 4, 15, 0x123456789ABCULL };
	sirVirtualTextureFeedbackHeader_t header = {};
	SIR_TEST( sirVTFeedbackMakeHeader( request, stamp, &header ) && header.nVersion == SIR_VT_FEEDBACK_GRID_VERSION &&
	          header.nWordCount == 240U * 136U );
	sirVirtualTextureFeedbackSlot_t slot = {};
	SIR_TEST( !sirVTFeedbackMarkDrawn( &slot ) && !sirVTFeedbackSubmitSlot( &slot, true ) );
	SIR_TEST( sirVTFeedbackPrepareSlot( &slot, header ) );
	SIR_TEST( !sirVTFeedbackPrepareSlot( &slot, header ) && sirVTFeedbackMarkDrawn( &slot ) );
	SIR_TEST( sirVTFeedbackSubmitSlot( &slot, true ) );
	sirVirtualTextureFeedbackHeader_t stale = header;
	stale.nFeedbackEpoch++;
	SIR_TEST( !sirVTFeedbackCompleteSlot( &slot, stale ) && slot.eState == SIR_VT_FEEDBACK_SLOT_READBACK_SUBMITTED );
	sirVTFeedbackInvalidateSlot( &slot );
	SIR_TEST( slot.bRetireOnCallback && !sirVTFeedbackCompleteSlot( &slot, header ) && slot.eState == SIR_VT_FEEDBACK_SLOT_FREE );
	SIR_TEST( sirVTFeedbackPrepareSlot( &slot, header ) && sirVTFeedbackMarkDrawn( &slot ) && sirVTFeedbackSubmitSlot( &slot, true ) &&
	          sirVTFeedbackCompleteSlot( &slot, header ) && slot.eState == SIR_VT_FEEDBACK_SLOT_READY );
	sirVTFeedbackInvalidateSlot( &slot );
	SIR_TEST( slot.eState == SIR_VT_FEEDBACK_SLOT_FREE );
	request.nSlot = SIR_VT_FEEDBACK_SLOTS;
	SIR_TEST( !sirVTFeedbackGridParams( request, &params ) );
	return true;
}

static bool testFeedbackMulti( void ) noexcept
{
	sirVirtualTextureFeedbackLayout_t layout = {};
	SIR_TEST( sirVTFeedbackMultiLayout( 1175080, &layout ) );
	SIR_TEST( layout.nWidth == 240 && layout.nHeight == 136 && layout.nSlotBytes == 1175080 && layout.nTotalBytes == 4700320 );
	SIR_TEST( sirVTFeedbackMultiLayout( 1175079, &layout ) && layout.nWidth == 160 && layout.nHeight == 90 );
	SIR_TEST( sirVTFeedbackMultiLayout( 1175080, &layout ) );
	sirVirtualTextureFeedbackRequest_t request = {};
	request.layout = layout;
	request.nViewportWidth = 1920;
	request.nViewportHeight = 1080;
	request.nSlot = 3;
	request.nPageCount = 321;
	request.nSourceLayers = 4;
	sirVirtualTextureFeedbackMultiParams_t params = {};
	SIR_TEST( sirVTFeedbackMultiParams( request, &params ) );
	SIR_TEST( params.nSourceLayers == 4 && params.nPixelLimit == 256 && params.nRecordWords == 9 &&
	          params.nSlotWords == ( 240U * 136U * 9U + 10U ) );
	sirVirtualTextureFeedbackHeader_t header = {};
	sirVirtualTextureFeedbackStamp_t stamp = { 33, 6, 8, 9, 0x887766554433ULL };
	SIR_TEST( sirVTFeedbackMakeHeader( request, stamp, &header ) && header.nVersion == SIR_VT_FEEDBACK_MULTI_VERSION &&
	          header.nWordCount == 240U * 136U * 9U );
	sirVirtualTextureFeedbackSlot_t slot = {};
	SIR_TEST( sirVTFeedbackPrepareSlot( &slot, header ) && sirVTFeedbackMarkDrawn( &slot ) );
	SIR_TEST( !sirVTFeedbackSubmitSlot( &slot, false ) && slot.eState == SIR_VT_FEEDBACK_SLOT_FREE );
	request.nSourceLayers = 9;
	SIR_TEST( !sirVTFeedbackMultiParams( request, &params ) );
	request.nSourceLayers = 4;
	request.nViewportWidth = 65000;
	request.nViewportHeight = 65000;
	SIR_TEST( !sirVTFeedbackMultiParams( request, &params ) );
	return true;
}

static uint64_t align16( uint64_t nValue ) noexcept
{
	return ( nValue + 15U ) & ~UINT64_C( 15 );
}

static std::vector<uint8_t> makeValidPack( void )
{
	sirVirtualTexturePackHeader_t header = {};
	header.nMagic = SIR_VT_PACK_MAGIC;
	header.nVersion = SIR_VT_PACK_VERSION;
	header.nHeaderBytes = SIR_VT_PACK_HEADER_BYTES;
	header.nFlags = SIR_VT_PACK_FLAG_CLUSTERED | SIR_VT_PACK_FLAG_TAILS | SIR_VT_PACK_FLAG_GPU_READY | SIR_VT_PACK_FLAG_QUALITY_VIEWS |
	                SIR_VT_PACK_FLAG_FAMILY_MAP;
	header.nAllocationCount = 1;
	header.nChannelAllocationCount = SIR_VT_PACK_CHANNEL_COUNT;
	header.nPageCount = 1;
	header.nPayloadCount = 1;
	header.nClusterCount = 1;
	header.nTailCount = 1;
	header.nChannelCount = SIR_VT_PACK_CHANNEL_COUNT;
	header.nInteriorTexels = SIR_VT_PAGE_INTERIOR;
	header.nGutterTexels = SIR_VT_PAGE_GUTTER;
	header.nPhysicalTexels = SIR_VT_PHYSICAL_TILE;
	header.nPackageGeneration = 1;
	header.nQualityViewCount = 1;
	header.nFormatFamilyCount = SIR_VT_PACK_CHANNEL_COUNT;
	header.nFullySupportedProfileMask = 1;
	uint64_t nCursor = SIR_VT_PACK_HEADER_BYTES;
	header.nAllocationOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackAllocationRecord_t ) );
	header.nAllocationKeyOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackAllocationKey_t ) );
	header.nChannelAllocationOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackChannelAllocation_t ) * SIR_VT_PACK_CHANNEL_COUNT );
	header.nPageOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackPageRecord_t ) );
	header.nPayloadDirectoryOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackPayloadRecord_t ) );
	header.nClusterDirectoryOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackClusterRecord_t ) );
	header.nTailMetadataOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackTailRecord_t ) );
	header.nQualityViewOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackQualityView_t ) );
	header.nFormatFamilyOffset = nCursor;
	nCursor = align16( nCursor + sizeof( sirVirtualTexturePackFamilyRecord_t ) * SIR_VT_PACK_CHANNEL_COUNT );
	header.nPayloadDataOffset = nCursor;
	header.nPayloadDataBytes = 32;
	header.nPackageBytes = nCursor + 32;
	std::vector<uint8_t> bytes( ( size_t )header.nPackageBytes, 0 );
	for ( uint32_t i = 0; i < 32; ++i ) {
		bytes[( size_t )header.nPayloadDataOffset + i] = ( uint8_t )( i * 3U );
	}
	header.nContentSignature = sirVTPackHash64( bytes.data() + SIR_VT_PACK_HEADER_BYTES, bytes.size() - SIR_VT_PACK_HEADER_BYTES );
	header.nHeaderChecksum = sirVTPackChecksum32( &header, sizeof( header ) );
	memcpy( bytes.data(), &header, sizeof( header ) );
	return bytes;
}

static bool testPackContract( void )
{
	SIR_TEST( sirVTPackHash64( nullptr, 0 ) == UINT64_C( 1469598103934665603 ) );
	SIR_TEST( sirVTPackHash64( nullptr, 2 ) == 0 );
	SIR_TEST( sirVTPackChecksum32( "123456789", 9 ) == 0xCBF43926U );
	SIR_TEST( sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC1 ) == 9248 );
	SIR_TEST( sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC7 ) == 18496 );
	SIR_TEST( sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC5 ) == 18496 );
	SIR_TEST( sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_R8 ) == 18496 );
	SIR_TEST( sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_R16 ) == 36992 );
	SIR_TEST( sirVTPackPhysicalTileBytes( ( sirVirtualTexturePhysicalFamily_t )99 ) == 0 );
	std::vector<uint8_t> bytes = makeValidPack();
	sirVirtualTexturePackView_t view = {};
	sirVirtualTexturePackInput_t input = { bytes.data(), bytes.size(), bytes.size(), false };
	SIR_TEST( sirVTPackOpenDirectories( input, &view ) );
	SIR_TEST( !view.bMetadataOnly && view.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].pData != nullptr );
	SIR_TEST( view.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].nBytes == 32 );
	sirVirtualTexturePackView_t metadata = {};
	input.bMetadataOnly = true;
	input.nAvailableBytes = view.header.nPayloadDataOffset;
	SIR_TEST( sirVTPackOpenDirectories( input, &metadata ) && metadata.bMetadataOnly &&
	          metadata.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].pData == nullptr );
	input.nAvailableBytes--;
	SIR_TEST( !sirVTPackOpenDirectories( input, &metadata ) );
	input.bMetadataOnly = false;
	input.nAvailableBytes = bytes.size();
	const auto restore = bytes;
	bytes[0] = 'S';
	SIR_TEST( !sirVTPackOpenDirectories( input, &view ) );
	bytes = restore;
	bytes.back() ^= 0x11;
	SIR_TEST( !sirVTPackOpenDirectories( input, &view ) );
	bytes = restore;
	bytes[128] ^= 0x11;
	SIR_TEST( !sirVTPackOpenDirectories( input, &view ) );
	bytes = restore;
	sirVirtualTexturePackHeader_t invalid = {};
	memcpy( &invalid, bytes.data(), sizeof( invalid ) );
	invalid.nPayloadDataOffset = invalid.nFormatFamilyOffset;
	invalid.nHeaderChecksum = 0;
	invalid.nHeaderChecksum = sirVTPackChecksum32( &invalid, sizeof( invalid ) );
	memcpy( bytes.data(), &invalid, sizeof( invalid ) );
	SIR_TEST( !sirVTPackOpenDirectories( input, &view ) );
	bytes = restore;
	input.nAvailableBytes = bytes.size() - 1;
	SIR_TEST( !sirVTPackOpenDirectories( input, &view ) );
	return true;
}

static bool testCompactTail( void ) noexcept
{
	sirVirtualTextureTailRequest_t request = { 0, 4, 128, 64, true };
	sirVirtualTextureTailPlacement_t result = {};
	SIR_TEST( sirVTPlaceCompactTail( request, &result ) && result.nAtlasX == 0 && result.nAtlasY == 0 && result.nNextHalf == 1 );
	request.nNextHalf = result.nNextHalf;
	SIR_TEST( sirVTPlaceCompactTail( request, &result ) && result.nAtlasX == 64 && result.nAtlasY == 0 && result.nNextHalf == 2 );
	request.nNextHalf = result.nNextHalf;
	request.bCompact = false;
	SIR_TEST( sirVTPlaceCompactTail( request, &result ) && result.nAtlasX == 128 && result.nAtlasY == 0 && result.nNextHalf == 4 );
	request.nNextHalf = 7;
	SIR_TEST( sirVTPlaceCompactTail( request, &result ) && result.nAtlasX == 0 && result.nAtlasY == 64 && result.nNextHalf == 10 );
	request.nNextHalf = UINT32_MAX;
	SIR_TEST( !sirVTPlaceCompactTail( request, &result ) );
	request.nNextHalf = 0;
	request.nSlotWidth = 3;
	SIR_TEST( !sirVTPlaceCompactTail( request, &result ) );
	return true;
}

int main( void )
{
	if ( !testFeedbackGrid() || !testFeedbackMulti() || !testPackContract() || !testCompactTail() ) {
		return 1;
	}
	printf( "Terrain3D VT V1 native port contracts PASS\n" );
	return 0;
}
