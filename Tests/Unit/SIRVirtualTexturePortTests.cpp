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
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackReader.h>
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackFileSource.h>

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

template <typename T>
static void putRecord( std::vector<uint8_t>& bytes, uint64_t nOffset, const T& record )
{
	memcpy( bytes.data() + nOffset, &record, sizeof( record ) );
}

static void signPack( std::vector<uint8_t>& bytes )
{
	sirVirtualTexturePackHeader_t header = {};
	memcpy( &header, bytes.data(), sizeof( header ) );
	header.nContentSignature = sirVTPackHash64( bytes.data() + SIR_VT_PACK_HEADER_BYTES, bytes.size() - SIR_VT_PACK_HEADER_BYTES );
	header.nHeaderChecksum = 0;
	header.nHeaderChecksum = sirVTPackChecksum32( &header, sizeof( header ) );
	memcpy( bytes.data(), &header, sizeof( header ) );
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
	header.nPayloadDataBytes = sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC1 );
	header.nPackageBytes = nCursor + header.nPayloadDataBytes;
	header.szChannelPageCount[0] = 1;
	header.szChannelTailCount[0] = 1;
	std::vector<uint8_t> bytes( ( size_t )header.nPackageBytes, 0 );
	for ( uint32_t i = 0; i < header.nPayloadDataBytes; ++i ) {
		bytes[( size_t )header.nPayloadDataOffset + i] = ( uint8_t )( i * 3U );
	}
	sirVirtualTexturePackAllocationRecord_t allocation = {};
	allocation.nGeneration = 7;
	allocation.nContentId = 19;
	allocation.nChannelMask = 1;
	allocation.nHighestProfile = 0;
	allocation.nAvailableProfiles = 1;
	allocation.nConsumer = 1; // INSTANCE: albedo is mandatory.
	putRecord( bytes, header.nAllocationOffset, allocation );
	sirVirtualTexturePackAllocationKey_t key = { UINT64_C( 0x123456789 ), 0, 19 };
	putRecord( bytes, header.nAllocationKeyOffset, key );
	for ( uint32_t i = 0; i < SIR_VT_PACK_CHANNEL_COUNT; ++i ) {
		sirVirtualTexturePackChannelAllocation_t channel = {};
		channel.nTailIndex = UINT32_MAX;
		if ( i == 0 ) {
			channel.nBasePage = 0;
			channel.nPageCount = 1;
			channel.nTailIndex = 0;
			channel.nMip0WidthPages = 1;
			channel.nMip0HeightPages = 1;
			channel.nMipCount = 1;
		}
		putRecord( bytes, header.nChannelAllocationOffset + i * sizeof( channel ), channel );
	}
	sirVirtualTexturePackPageRecord_t page = {};
	page.nVirtualPage = 0;
	page.nParentPage = UINT32_MAX;
	page.nAllocationId = 0;
	page.nPayloadId = 0;
	page.nGeneration = 7;
	page.nContentId = 19;
	page.nChannel = 0;
	putRecord( bytes, header.nPageOffset, page );
	sirVirtualTexturePackPayloadRecord_t payload = {};
	payload.nFormat = SIR_VT_PHYSICAL_BC1;
	payload.nClusterId = 0;
	payload.nStoredBytes = header.nPayloadDataBytes;
	payload.nDecodedBytes = header.nPayloadDataBytes;
	payload.nCodec = 0;
	payload.nFlags = 1;
	payload.nChecksum = sirVTPackChecksum32( bytes.data() + header.nPayloadDataOffset, header.nPayloadDataBytes );
	payload.nContentSignature = sirVTPackHash64( bytes.data() + header.nPayloadDataOffset, header.nPayloadDataBytes );
	putRecord( bytes, header.nPayloadDirectoryOffset, payload );
	sirVirtualTexturePackClusterRecord_t cluster = {};
	cluster.nContentSignature = payload.nContentSignature;
	cluster.nStoredBytes = header.nPayloadDataBytes;
	cluster.nDecodedBytes = header.nPayloadDataBytes;
	cluster.nFirstPayload = 0;
	cluster.nPayloadCount = 1;
	cluster.nChecksum = payload.nChecksum;
	putRecord( bytes, header.nClusterDirectoryOffset, cluster );
	sirVirtualTexturePackTailRecord_t tail = {};
	tail.nContentSignature = payload.nContentSignature;
	tail.nCanonicalAllocationId = 0;
	tail.nPayloadId = 0;
	tail.nContentId = 19;
	tail.nFirstMip = 0;
	tail.nMipCount = 1;
	tail.nFirstMipWidth = 128;
	tail.nFirstMipHeight = 128;
	tail.nAtlasInteriorWidth = 128;
	tail.nAtlasInteriorHeight = 128;
	tail.nChannel = 0;
	tail.nLayout = SIR_VT_TAIL_LAYOUT_HORIZONTAL;
	putRecord( bytes, header.nTailMetadataOffset, tail );
	sirVirtualTexturePackQualityView_t quality = {};
	quality.nChannelMask = 1;
	for ( uint32_t i = 0; i < SIR_VT_PACK_CHANNEL_COUNT; ++i ) {
		quality.szBaseMip[i] = i == 0 ? 0 : UINT8_MAX;
		quality.szMaxMip[i] = i == 0 ? 0 : UINT8_MAX;
	}
	putRecord( bytes, header.nQualityViewOffset, quality );
	const uint8_t families[6] = { 0, 1, 3, 2, 0, 3 };
	const uint8_t semantics[6] = { 0, 1, 2, 3, 4, 4 };
	for ( uint32_t i = 0; i < SIR_VT_PACK_CHANNEL_COUNT; ++i ) {
		sirVirtualTexturePackFamilyRecord_t family = {};
		family.nChannel = ( uint8_t )i;
		family.nFamily = families[i];
		family.nSemantic = semantics[i];
		family.nFlags = i == 0 ? 1 : 0;
		putRecord( bytes, header.nFormatFamilyOffset + i * sizeof( family ), family );
	}
	putRecord( bytes, 0, header );
	signPack( bytes );
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
	SIR_TEST( view.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].nBytes == sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC1 ) );
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

static bool testValidatedPack( void )
{
	std::vector<uint8_t> bytes = makeValidPack();
	sirVirtualTexturePackInput_t input = { bytes.data(), bytes.size(), bytes.size(), false };
	sirVirtualTexturePackView_t view = {};
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	SIR_TEST( !view.bMetadataOnly && view.header.nPageCount == 1 );
	sirVirtualTexturePackPageRecord_t page = {};
	SIR_TEST( sirVTPackGetPage( view, 0, &page ) && page.nGeneration == 7 && page.nContentId == 19 );
	SIR_TEST( !sirVTPackGetPage( view, 1, &page ) && !sirVTPackGetPage( view, 0, nullptr ) );
	input.bMetadataOnly = true;
	input.nAvailableBytes = view.header.nPayloadDataOffset;
	SIR_TEST( sirVTPackOpenValidated( input, &view ) && view.bMetadataOnly );
	SIR_TEST( view.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].pData == nullptr );
	input.bMetadataOnly = false;
	input.nAvailableBytes = bytes.size();
	std::vector<uint8_t> unaligned( bytes.size() + 1 );
	memcpy( unaligned.data() + 1, bytes.data(), bytes.size() );
	input.pBytes = unaligned.data() + 1;
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	input.pBytes = bytes.data();
	const std::vector<uint8_t> original = bytes;
	const sirVirtualTexturePackHeader_t *pHeader = ( const sirVirtualTexturePackHeader_t * )original.data();
	const uint64_t nAllocation = pHeader->nAllocationOffset;
	const uint64_t nKey = pHeader->nAllocationKeyOffset;
	const uint64_t nChannel = pHeader->nChannelAllocationOffset;
	const uint64_t nPage = pHeader->nPageOffset;
	const uint64_t nPayload = pHeader->nPayloadDirectoryOffset;
	const uint64_t nCluster = pHeader->nClusterDirectoryOffset;
	const uint64_t nTail = pHeader->nTailMetadataOffset;
	const uint64_t nQuality = pHeader->nQualityViewOffset;
	const uint64_t nFamily = pHeader->nFormatFamilyOffset;
	const uint64_t badOffsets[] = { nAllocation + 12, nKey + 8,   nChannel + 10, nPage + 4,  nPayload + 12,
	                                nCluster + 24,    nTail + 32, nQuality + 2,  nFamily + 1 };
	for ( uint32_t i = 0; i < sizeof( badOffsets ) / sizeof( badOffsets[0] ); ++i ) {
		bytes = original;
		bytes[( size_t )badOffsets[i]] ^= 0x40;
		signPack( bytes );
		SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
		SIR_TEST( view.szDirectories[SIR_VT_PACK_ALLOCATIONS].pData == nullptr );
	}
	bytes = original;
	bytes[( size_t )pHeader->nPayloadDataOffset + 37] ^= 0x77;
	signPack( bytes ); // Package FNV is valid; cluster CRC is now invalid.
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	bytes = original;
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	return true;
}

static bool testPackFileSource( void )
{
	static constexpr const char *pszPath = "sir_vt_sparse_v1_native_test.pack";
	std::vector<uint8_t> bytes = makeValidPack();
	SIRPlatformFile writer;
	SIR_TEST( writer.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	uint64_t nWritten = 0;
	SIR_TEST( writer.write( bytes.data(), bytes.size(), &nWritten ) == SIR_FILE_OK && nWritten == bytes.size() );
	SIR_TEST( writer.close() == SIR_FILE_OK );
	std::vector<uint8_t> metadata( 8192 );
	SIRVirtualTexturePackFileSource source;
	sirVirtualTexturePackView_t view = {};
	sirVirtualTexturePackMetadataBuffer_t buffer = { metadata.data(), metadata.size() };
	SIR_TEST( !source.open( pszPath, { metadata.data(), 32 }, true, &view ) && !source.isOpen() );
	SIR_TEST( source.open( pszPath, buffer, true, &view ) && source.isOpen() );
	SIR_TEST( source.getPayloadBytes() == sirVTPackPhysicalTileBytes( SIR_VT_PHYSICAL_BC1 ) );
	SIR_TEST( source.getPackageGeneration() == 1 && source.getReadCount() == 0 );
	uint8_t szPayload[64] = {};
	SIR_TEST( source.readPayload( 3, szPayload, sizeof( szPayload ) ) );
	SIR_TEST( memcmp( szPayload, bytes.data() + view.header.nPayloadDataOffset + 3, sizeof( szPayload ) ) == 0 );
	SIR_TEST( source.getReadCount() == 1 );
	SIR_TEST( !source.readPayload( source.getPayloadBytes() - 7, szPayload, sizeof( szPayload ) ) );
	SIR_TEST( !source.readPayload( 0, nullptr, 64 ) && !source.readPayload( 0, szPayload, 0 ) );
	source.close();
	SIR_TEST( !source.isOpen() && !source.readPayload( 0, szPayload, 64 ) );
	bytes.back() ^= 0x55;
	SIR_TEST( writer.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	SIR_TEST( writer.write( bytes.data(), bytes.size(), &nWritten ) == SIR_FILE_OK && nWritten == bytes.size() );
	SIR_TEST( writer.close() == SIR_FILE_OK );
	SIR_TEST( !source.open( pszPath, buffer, true, &view ) && !source.isOpen() );
	SIR_TEST( source.open( pszPath, buffer, false, &view ) && source.isOpen() );
	SIR_TEST( view.bMetadataOnly && view.szDirectories[SIR_VT_PACK_PAYLOAD_DATA].pData == nullptr );
	source.close();
	SIR_TEST( SIRPlatformFile::remove( pszPath ) == SIR_FILE_OK );
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
	if ( !testFeedbackGrid() || !testFeedbackMulti() || !testPackContract() || !testValidatedPack() || !testPackFileSource() ||
	     !testCompactTail() ) {
		return 1;
	}
	printf( "Terrain3D VT V1 native port contracts PASS\n" );
	return 0;
}
