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
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackWriter.h>
#include <Renderer/VirtualTexturing/SIRVirtualTextureClusterDecode.h>

#if defined( SIR_VT_TEST_ZSTD )
#include <zstd.h>
#endif

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

static bool testNativeClusterDecode( void )
{
	std::vector<uint8_t> bytes = makeValidPack();
	const sirVirtualTexturePackInput_t input = { bytes.data(), bytes.size(), bytes.size(), false };
	sirVirtualTexturePackView_t view = {};
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	const uint32_t nDecoded = ( uint32_t )view.header.nPayloadDataBytes;
	std::vector<uint8_t> decoded( nDecoded, 0xCE );
	sirVirtualTextureClusterBuffers_t buffers = { bytes.data() + view.header.nPayloadDataOffset, nDecoded, decoded.data(), nDecoded };
	SIR_TEST( sirVTDecodePackCluster( view, 0, buffers, nullptr ) );
	SIR_TEST( memcmp( decoded.data(), buffers.pStoredBytes, nDecoded ) == 0 );
	SIR_TEST( !sirVTDecodePackCluster( view, 1, buffers, nullptr ) );
	buffers.nDecodedCapacity = nDecoded - 1;
	SIR_TEST( !sirVTDecodePackCluster( view, 0, buffers, nullptr ) );
	buffers.nDecodedCapacity = nDecoded;
	buffers.nStoredCapacity = nDecoded - 1;
	SIR_TEST( !sirVTDecodePackCluster( view, 0, buffers, nullptr ) );
	buffers.nStoredCapacity = nDecoded;
	buffers.pDecodedBytes = ( uint8_t * )buffers.pStoredBytes;
	SIR_TEST( sirVTDecodePackCluster( view, 0, buffers, nullptr ) );

	// File-backed metadata-only cluster decoding uses the same validated V1 records.
	const char *pszPath = "sir_vt_decode_native_test.pack";
	SIRPlatformFile file;
	SIR_TEST( file.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	uint64_t nWritten = 0;
	SIR_TEST( file.write( bytes.data(), bytes.size(), &nWritten ) == SIR_FILE_OK && nWritten == bytes.size() );
	SIR_TEST( file.close() == SIR_FILE_OK );
	std::vector<uint8_t> metadata( ( size_t )view.header.nPayloadDataOffset );
	SIRVirtualTexturePackFileSource source;
	sirVirtualTexturePackView_t fromDisk = {};
	SIR_TEST( source.open( pszPath, { metadata.data(), metadata.size() }, true, &fromDisk ) );
	std::vector<uint8_t> stored( nDecoded );
	buffers = { stored.data(), nDecoded, decoded.data(), nDecoded };
	SIR_TEST( source.readCluster( fromDisk, 0, buffers, nullptr ) );
	SIR_TEST( source.getReadCount() == 1 && memcmp( stored.data(), decoded.data(), nDecoded ) == 0 );
	fromDisk.header.nContentSignature++;
	SIR_TEST( !source.readCluster( fromDisk, 0, buffers, nullptr ) && source.getReadCount() == 1 );
	fromDisk.header.nContentSignature--;
	fromDisk.header.nPackageGeneration++;
	SIR_TEST( !source.readCluster( fromDisk, 0, buffers, nullptr ) && source.getReadCount() == 1 );
	source.close();
	SIR_TEST( SIRPlatformFile::remove( pszPath ) == SIR_FILE_OK );
	return true;
}

#if defined( SIR_VT_TEST_ZSTD )
static bool sirVTTestDecodeZstd( void *, const sirVirtualTextureCodecFrame_t& frame, uint32_t *pProducedBytes ) noexcept
{
	const size_t nDecoded = ZSTD_decompress( frame.pOutput, frame.nOutputCapacity, frame.pInput, frame.nInputBytes );
	if ( ZSTD_isError( nDecoded ) || nDecoded > UINT32_MAX ) {
		return false;
	}
	*pProducedBytes = ( uint32_t )nDecoded;
	return true;
}

static bool testCompressedClusterDecode( void )
{
	const std::vector<uint8_t> original = makeValidPack();
	sirVirtualTexturePackHeader_t header = {};
	memcpy( &header, original.data(), sizeof( header ) );
	const uint32_t nDecoded = ( uint32_t )header.nPayloadDataBytes;
	const size_t nBound = ZSTD_compressBound( nDecoded );
	std::vector<uint8_t> encoded( nBound );
	const size_t nStored = ZSTD_compress( encoded.data(), encoded.size(), original.data() + header.nPayloadDataOffset, nDecoded, 3 );
	SIR_TEST( !ZSTD_isError( nStored ) && nStored > 0 && nStored < nDecoded );
	encoded.resize( nStored );
	std::vector<uint8_t> bytes( ( size_t )header.nPayloadDataOffset + nStored );
	memcpy( bytes.data(), original.data(), ( size_t )header.nPayloadDataOffset );
	memcpy( bytes.data() + header.nPayloadDataOffset, encoded.data(), encoded.size() );
	header.nPayloadDataBytes = nStored;
	header.nPackageBytes = bytes.size();
	header.nFlags |= SIR_VT_PACK_FLAG_OUTER_COMPRESSION;
	sirVirtualTexturePackClusterRecord_t cluster = {};
	memcpy( &cluster, bytes.data() + header.nClusterDirectoryOffset, sizeof( cluster ) );
	cluster.nStoredBytes = nStored;
	cluster.nCodec = 1;
	cluster.nChecksum = sirVTPackChecksum32( encoded.data(), nStored );
	cluster.nContentSignature = sirVTPackHash64( encoded.data(), nStored );
	putRecord( bytes, header.nClusterDirectoryOffset, cluster );
	putRecord( bytes, 0, header );
	signPack( bytes );
	sirVirtualTexturePackView_t view = {};
	const sirVirtualTexturePackInput_t input = { bytes.data(), bytes.size(), bytes.size(), false };
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	std::vector<uint8_t> decoded( nDecoded, 0xCE );
	sirVirtualTextureClusterBuffers_t buffers = { encoded.data(), ( uint32_t )encoded.size(), decoded.data(), nDecoded };
	const sirVirtualTextureClusterCodec_t codec = { nullptr, sirVTTestDecodeZstd };
	SIR_TEST( !sirVTDecodePackCluster( view, 0, buffers, nullptr ) );
	SIR_TEST( sirVTDecodePackCluster( view, 0, buffers, &codec ) );
	SIR_TEST( memcmp( decoded.data(), original.data() + header.nPayloadDataOffset, nDecoded ) == 0 );
	buffers.pDecodedBytes = ( uint8_t * )buffers.pStoredBytes;
	SIR_TEST( !sirVTDecodePackCluster( view, 0, buffers, &codec ) );
	buffers.pDecodedBytes = decoded.data();
	encoded[3] ^= 0x7A;
	SIR_TEST( !sirVTDecodePackCluster( view, 0, buffers, &codec ) );
	encoded[3] ^= 0x7A;
	SIR_TEST( sirVTDecodePackCluster( view, 0, buffers, &codec ) );
	return true;
}
#endif

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
	// A forged package can carry valid outer hashes but a cyclic page parent.
	// Validate parent semantics before allowing metadata-only or full reads.
	bytes = original;
	sirVirtualTexturePackPageRecord_t invalidParent = {};
	memcpy( &invalidParent, bytes.data() + nPage, sizeof( invalidParent ) );
	invalidParent.nParentPage = invalidParent.nVirtualPage;
	putRecord( bytes, nPage, invalidParent );
	signPack( bytes );
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	input.bMetadataOnly = true;
	input.nAvailableBytes = pHeader->nPayloadDataOffset;
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	input.bMetadataOnly = false;
	input.nAvailableBytes = bytes.size();
	bytes = original;
	bytes[( size_t )pHeader->nPayloadDataOffset + 37] ^= 0x77;
	signPack( bytes ); // Package FNV is valid; cluster CRC is now invalid.
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	bytes = original;
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	return true;
}

// The donor writes parent links to the next coarser mip in channel-local order.
// This fixture keeps the frozen Sparse V1 disk layout and recomputes the
// package signature after every semantic corruption.
static bool testPackParentChain( void )
{
	std::vector<uint8_t> bytes = makeValidPack();
	sirVirtualTexturePackHeader_t header = {};
	memcpy( &header, bytes.data(), sizeof( header ) );
	const uint64_t nPageOffset = header.nPageOffset;
	const uint64_t nOldPayloadDirectoryOffset = header.nPayloadDirectoryOffset;
	const uint64_t nNewPayloadDirectoryOffset = align16( nPageOffset + 2 * sizeof( sirVirtualTexturePackPageRecord_t ) );
	SIR_TEST( nNewPayloadDirectoryOffset > nOldPayloadDirectoryOffset );
	const uint64_t nAdditionalBytes = nNewPayloadDirectoryOffset - nOldPayloadDirectoryOffset;
	bytes.insert( bytes.begin() + ( ptrdiff_t )nOldPayloadDirectoryOffset, ( size_t )nAdditionalBytes, 0 );
	header.nPageCount = 2;
	header.szChannelPageCount[0] = 2;
	header.nPayloadDirectoryOffset += nAdditionalBytes;
	header.nClusterDirectoryOffset += nAdditionalBytes;
	header.nTailMetadataOffset += nAdditionalBytes;
	header.nQualityViewOffset += nAdditionalBytes;
	header.nFormatFamilyOffset += nAdditionalBytes;
	header.nPayloadDataOffset += nAdditionalBytes;
	header.nPackageBytes += nAdditionalBytes;

	sirVirtualTexturePackChannelAllocation_t channel = {};
	memcpy( &channel, bytes.data() + header.nChannelAllocationOffset, sizeof( channel ) );
	channel.nPageCount = 2;
	channel.nMipCount = 2;
	putRecord( bytes, header.nChannelAllocationOffset, channel );
	sirVirtualTexturePackTailRecord_t tail = {};
	memcpy( &tail, bytes.data() + header.nTailMetadataOffset, sizeof( tail ) );
	tail.nFirstMip = 1;
	putRecord( bytes, header.nTailMetadataOffset, tail );

	sirVirtualTexturePackPageRecord_t page = {};
	memcpy( &page, bytes.data() + nPageOffset, sizeof( page ) );
	page.nParentPage = 1;
	putRecord( bytes, nPageOffset, page );
	sirVirtualTexturePackPageRecord_t root = page;
	root.nVirtualPage = 1;
	root.nParentPage = UINT32_MAX;
	root.nMip = 1;
	putRecord( bytes, nPageOffset + sizeof( root ), root );
	putRecord( bytes, 0, header );
	signPack( bytes );
	const std::vector<uint8_t> valid = bytes;
	sirVirtualTexturePackInput_t input = { bytes.data(), bytes.size(), bytes.size(), false };
	sirVirtualTexturePackView_t view = {};
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );
	input.bMetadataOnly = true;
	input.nAvailableBytes = header.nPayloadDataOffset;
	SIR_TEST( sirVTPackOpenValidated( input, &view ) );

	const uint16_t szBadParentMips[] = { 0, 2 };
	for ( uint16_t nMip : szBadParentMips ) {
		bytes = valid;
		root.nMip = nMip;
		putRecord( bytes, nPageOffset + sizeof( root ), root );
		signPack( bytes );
		SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	}
	bytes = valid;
	root.nMip = 1;
	root.nPageX = 1;
	putRecord( bytes, nPageOffset + sizeof( root ), root );
	signPack( bytes );
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	bytes = valid;
	root.nPageX = 0;
	root.nPageY = 1;
	putRecord( bytes, nPageOffset + sizeof( root ), root );
	signPack( bytes );
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
	bytes = valid;
	page.nParentPage = 0;
	putRecord( bytes, nPageOffset, page );
	signPack( bytes );
	SIR_TEST( !sirVTPackOpenValidated( input, &view ) );
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

template <typename T>
static std::vector<T> copyDirectory( const std::vector<uint8_t>& bytes, uint64_t nOffset, uint32_t nCount )
{
	std::vector<T> records( nCount );
	memcpy( records.data(), bytes.data() + nOffset, ( size_t )nCount * sizeof( T ) );
	return records;
}

static bool testNativePackWriter( void )
{
	const std::vector<uint8_t> bytes = makeValidPack();
	sirVirtualTexturePackHeader_t header = {};
	memcpy( &header, bytes.data(), sizeof( header ) );
	const auto allocations =
	    copyDirectory<sirVirtualTexturePackAllocationRecord_t>( bytes, header.nAllocationOffset, header.nAllocationCount );
	const auto keys = copyDirectory<sirVirtualTexturePackAllocationKey_t>( bytes, header.nAllocationKeyOffset, header.nAllocationCount );
	const auto channels =
	    copyDirectory<sirVirtualTexturePackChannelAllocation_t>( bytes, header.nChannelAllocationOffset, header.nChannelAllocationCount );
	const auto pages = copyDirectory<sirVirtualTexturePackPageRecord_t>( bytes, header.nPageOffset, header.nPageCount );
	const auto payloads =
	    copyDirectory<sirVirtualTexturePackPayloadRecord_t>( bytes, header.nPayloadDirectoryOffset, header.nPayloadCount );
	const auto clusters =
	    copyDirectory<sirVirtualTexturePackClusterRecord_t>( bytes, header.nClusterDirectoryOffset, header.nClusterCount );
	const auto tails = copyDirectory<sirVirtualTexturePackTailRecord_t>( bytes, header.nTailMetadataOffset, header.nTailCount );
	const auto quality = copyDirectory<sirVirtualTexturePackQualityView_t>( bytes, header.nQualityViewOffset, header.nQualityViewCount );
	const auto families =
	    copyDirectory<sirVirtualTexturePackFamilyRecord_t>( bytes, header.nFormatFamilyOffset, header.nFormatFamilyCount );
	sirVirtualTexturePackSource_t source = {};
	source.pAllocations = allocations.data();
	source.pAllocationKeys = keys.data();
	source.pChannelAllocations = channels.data();
	source.pPages = pages.data();
	source.pPayloads = payloads.data();
	source.pClusters = clusters.data();
	source.pTails = tails.data();
	source.pQualityViews = quality.data();
	source.pFormatFamilies = families.data();
	source.pPayloadData = bytes.data() + header.nPayloadDataOffset;
	source.nPayloadDataBytes = header.nPayloadDataBytes;
	source.nAllocationCount = header.nAllocationCount;
	source.nChannelAllocationCount = header.nChannelAllocationCount;
	source.nPageCount = header.nPageCount;
	source.nPayloadCount = header.nPayloadCount;
	source.nClusterCount = header.nClusterCount;
	source.nTailCount = header.nTailCount;
	source.nQualityViewCount = header.nQualityViewCount;
	source.nFormatFamilyCount = header.nFormatFamilyCount;
	source.nPackageGeneration = header.nPackageGeneration;

	uint64_t nMetadata = 0;
	uint64_t nTotal = 0;
	SIR_TEST( sirVTMeasurePack( source, &nMetadata, &nTotal ) );
	SIR_TEST( nTotal == bytes.size() && nMetadata == header.nPayloadDataOffset );
	std::vector<uint8_t> written( bytes.size(), 0xCC );
	uint64_t nWritten = UINT64_MAX;
	SIR_TEST( sirVTWritePack( source, written.data(), written.size(), &nWritten ) );
	SIR_TEST( nWritten == bytes.size() && written == bytes );

	// The donor permits the already-cooked payload at its final output offset.
	std::vector<uint8_t> alias( bytes.size(), 0xCC );
	memcpy( alias.data() + nMetadata, source.pPayloadData, source.nPayloadDataBytes );
	sirVirtualTexturePackSource_t aliased = source;
	aliased.pPayloadData = alias.data() + nMetadata;
	SIR_TEST( sirVTWritePack( aliased, alias.data(), alias.size(), &nWritten ) && alias == bytes );

	SIR_TEST( !sirVTWritePack( source, written.data(), written.size() - 1U, &nWritten ) && nWritten == 0 );
	SIR_TEST( !sirVTWritePack( source, written.data(), written.size(), nullptr ) );
	SIR_TEST( !sirVTMeasurePack( source, nullptr, &nTotal ) );
	sirVirtualTexturePackSource_t invalid = source;
	invalid.pPayloadData = nullptr;
	SIR_TEST( !sirVTWritePack( invalid, written.data(), written.size(), &nWritten ) && nWritten == 0 );
	invalid = source;
	invalid.nPayloadDataBytes = UINT64_MAX;
	SIR_TEST( !sirVTMeasurePack( invalid, &nMetadata, &nTotal ) && nMetadata == 0 && nTotal == 0 );
	invalid = source;
	invalid.pAllocations = ( const sirVirtualTexturePackAllocationRecord_t * )written.data();
	SIR_TEST( !sirVTWritePack( invalid, written.data(), written.size(), &nWritten ) );
	invalid = source;
	invalid.nChannelAllocationCount++;
	SIR_TEST( !sirVTMeasurePack( invalid, &nMetadata, &nTotal ) );
	invalid = source;
	sirVirtualTexturePackPageRecord_t badPage = pages[0];
	badPage.nAllocationId = UINT32_MAX;
	invalid.pPages = &badPage;
	SIR_TEST( !sirVTWritePack( invalid, written.data(), written.size(), &nWritten ) && nWritten == 0 );
	invalid = source;
	invalid.nPackageGeneration++;
	SIR_TEST( sirVTWritePack( invalid, written.data(), written.size(), &nWritten ) );
	SIR_TEST( written != bytes );
	sirVirtualTexturePackView_t validated = {};
	const sirVirtualTexturePackInput_t encoded = { written.data(), written.size(), written.size(), false };
	SIR_TEST( sirVTPackOpenValidated( encoded, &validated ) && validated.header.nPackageGeneration == invalid.nPackageGeneration );
	return true;
}

int main( void )
{
	if ( !testFeedbackGrid() || !testFeedbackMulti() || !testPackContract() || !testValidatedPack() || !testPackParentChain() ||
	     !testPackFileSource() || !testCompactTail() || !testNativePackWriter() || !testNativeClusterDecode() ) {
		return 1;
	}
#if defined( SIR_VT_TEST_ZSTD )
	if ( !testCompressedClusterDecode() ) {
		return 1;
	}
#endif
	printf( "Terrain3D VT V1 native port contracts PASS\n" );
	return 0;
}
