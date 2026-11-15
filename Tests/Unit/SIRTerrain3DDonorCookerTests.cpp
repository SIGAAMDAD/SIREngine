/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureCooker.h>
#include <Renderer/VirtualTexturing/SIRVirtualTextureRegionCooker.h>
#include <Renderer/VirtualTexturing/SIRVirtualTexturePackFileSource.h>
#include <Renderer/VirtualTexturing/SIRVirtualTextureZstdCodec.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Terrain/Editor/SIRTerrainVirtualTextureCookBridge.h>

#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"
#include "authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.h"
#include "authoring/vt/build/terrain_3d_composed_surface_cold_pipeline.h"
#include "authoring/vt/build/terrain_3d_composed_surface_reference_decode.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"
#include "authoring/vt/build/terrain_3d_vt_cooker.h"
#include "authoring/vt/build/terrain_3d_vt_reference_encoder.h"
#include "authoring/vt/io/terrain_3d_vt_pack_writer.h"
#include "data/formats/terrain_3d_vt_pack_format.h"
#include "rendering/rd/vt/io/terrain_3d_vt_pack_runtime.h"
#include "rendering/rd/vt/io/terrain_3d_vt_file_source.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define CHECK( condition )                                                                                                                 \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%u: %s\n", __FILE__, ( unsigned )__LINE__, #condition );                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static void *cookAllocate( void *, uint64_t nBytes, uint32_t )
{
	return nBytes <= SIZE_MAX ? malloc( ( size_t )nBytes ) : nullptr;
}

static void cookFree( void *, void *pMemory )
{
	free( pMemory );
}

static void *packRuntimeAllocate( void *, size_t nBytes, size_t )
{
	return malloc( nBytes );
}

static void packRuntimeFree( void *, void *pMemory )
{
	free( pMemory );
}

static bool testNativeDonorRuntime( const uint8_t *pPackage, uint64_t nBytes )
{
	t3dVTPackRuntimeAllocator_t allocator{ nullptr, &packRuntimeAllocate, &packRuntimeFree };
	t3dVTPackRuntimeStorage_t memory{};
	CHECK( t3d_vt_pack_runtime_open_cold( pPackage, nBytes, allocator, memory ) );
	CHECK( memory.Database.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK );
	CHECK( memory.Database.nV1PageCount > 0u );
	uint32_t allocation = T3D_VT_INVALID_ALLOCATION;
	CHECK( t3d_vt_pack_runtime_find_allocation_by_stable_key( memory, UINT64_C( 0xABCDEF0123 ), allocation ) );
	CHECK( allocation == 0u );
	CHECK( !t3d_vt_pack_runtime_find_allocation_by_stable_key( memory, UINT64_C( 0xABCDEF0124 ), allocation ) );
	t3dVTQualityRuntimeStats_t stats{};
	CHECK( t3d_vt_pack_runtime_apply_quality_profile(
	    memory.Database, memory.pRuntimeAllocations,
	    memory.PackViewV1.m_pHeader->m_nAllocationCount * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION, 0u, stats ) );
	CHECK( stats.nAllocationsByResolvedProfile[0] == 1u );
	t3d_vt_pack_runtime_release_cold( allocator, memory );
	CHECK( memory.pOwnedBlock == nullptr );

	static const char pszOutput[] = "sir_vt_p1_donor_runtime.vtpack";
	static const char pszStaging[] = "sir_vt_p1_donor_runtime.tmp";
	SIRPlatformFile::remove( pszOutput );
	SIRPlatformFile::remove( pszStaging );
	sirVirtualTextureCookOutput_t published{ const_cast<uint8_t *>( pPackage ), nBytes, nBytes };
	CHECK( SIRVirtualTextureCooker::publishAtomic( pszOutput, pszStaging, published ) );
	t3dVTFileSource_s nativeFile;
	std::vector<uint8_t> nativeMeta( 1024u * 1024u );
	uint64_t metaSize = 0u;
	bool opened =
	    t3d_vt_file_source_open_cold( { { pszOutput, 0u, UINT64_MAX, true }, nativeMeta.data(), nativeMeta.size() }, nativeFile, metaSize );
	CHECK( opened && metaSize > 256u );
	if ( opened ) {
		t3dVTPackBaselineView_t nativeView{};
		CHECK( t3d_vt_pack_open_metadata_view( nativeMeta.data(), metaSize, nativeFile.nFileBytes, nativeView ) );
		t3d_vt_file_source_close( nativeFile );
	}
	t3dVTPackRuntimeStorage_t fileStorage{};
	CHECK( t3d_vt_pack_runtime_open_file_cold( pszOutput, allocator, fileStorage ) );
	CHECK( fileStorage.Database.pFileSource != nullptr );
	CHECK( fileStorage.Database.pPayloadData == nullptr );
	CHECK( fileStorage.Database.nPackageContentSignature == fileStorage.PackViewV1.m_pHeader->m_nContentSignature );
	CHECK( t3d_vt_pack_runtime_find_allocation_by_stable_key( fileStorage, UINT64_C( 0xABCDEF0123 ), allocation ) );
	CHECK( allocation == 0u );
	t3d_vt_pack_runtime_release_cold( allocator, fileStorage );
	CHECK( fileStorage.pOwnedBlock == nullptr && fileStorage.pFileSource == nullptr );
	CHECK( SIRPlatformFile::remove( pszOutput ) == SIR_FILE_OK );
	CHECK( !t3d_vt_pack_runtime_open_file_cold( pszOutput, allocator, fileStorage ) );

	// A catalog may identify a V1 package inside a larger SIRPack without
	// changing the donor package's physical bytes or making another copy.
	static const char pszContainer[] = "sir_vt_p1_embedded_interval.sirpack";
	SIRPlatformFile::remove( pszContainer );
	SIRPlatformFile outer;
	CHECK( outer.open( pszContainer, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_EXCLUSIVE ) == SIR_FILE_OK );
	uint8_t prefix[37] = {};
	uint8_t suffix[19] = {};
	memset( prefix, 0xCC, sizeof( prefix ) );
	memset( suffix, 0x7B, sizeof( suffix ) );
	uint64_t transferred = 0u;
	CHECK( outer.writeAt( 0u, prefix, sizeof( prefix ), &transferred ) == SIR_FILE_OK && transferred == sizeof( prefix ) );
	CHECK( outer.writeAt( sizeof( prefix ), pPackage, nBytes, &transferred ) == SIR_FILE_OK && transferred == nBytes );
	CHECK( outer.writeAt( sizeof( prefix ) + nBytes, suffix, sizeof( suffix ), &transferred ) == SIR_FILE_OK &&
	       transferred == sizeof( suffix ) );
	CHECK( outer.flush() == SIR_FILE_OK && outer.close() == SIR_FILE_OK );
	std::vector<uint8_t> metadata( 1024u * 1024u );
	SIRVirtualTexturePackFileSource embedded;
	sirVirtualTexturePackView_t embeddedView{};
	const sirVirtualTexturePackMetadataBuffer_t metaBuffer{ metadata.data(), metadata.size() };
	CHECK( embedded.openRange( { pszContainer, sizeof( prefix ), nBytes, true }, metaBuffer, &embeddedView ) );
	CHECK( embeddedView.header.nPackageBytes == nBytes );
	CHECK( embedded.getFileBytes() == nBytes );
	embedded.close();
	t3dVTPackRuntimeStorage_t embeddedRuntime{};
	CHECK( t3d_vt_pack_runtime_open_range_cold( { pszContainer, sizeof( prefix ), nBytes }, allocator, embeddedRuntime ) );
	CHECK( t3d_vt_pack_runtime_find_allocation_by_stable_key( embeddedRuntime, UINT64_C( 0xABCDEF0123 ), allocation ) && allocation == 0u );
	t3d_vt_pack_runtime_release_cold( allocator, embeddedRuntime );
	CHECK( !t3d_vt_pack_runtime_open_range_cold( { pszContainer, sizeof( prefix ) - 1u, nBytes }, allocator, embeddedRuntime ) );
	CHECK( !embedded.openRange( { pszContainer, sizeof( prefix ) - 1u, nBytes, true }, metaBuffer, &embeddedView ) );
	CHECK( !embedded.openRange( { pszContainer, sizeof( prefix ), nBytes - 1u, true }, metaBuffer, &embeddedView ) );
	CHECK( !embedded.openRange( { pszContainer, UINT64_MAX - 8u, nBytes, true }, metaBuffer, &embeddedView ) );
	CHECK( SIRPlatformFile::remove( pszContainer ) == SIR_FILE_OK );
	return true;
}

static void configureSource( t3dVTCookSparseSource_t& source, const uint8_t *pPixels, uint32_t extent )
{
	source = {};
	source.m_nStableKey = UINT64_C( 0xABCDEF0123 );
	source.m_nConsumer = T3D_VT_CONSUMER_INSTANCE;
	source.m_nAuthoredChannelMask = T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO );
	source.m_nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
	source.m_nOrmSourceLayout = T3D_VT_SURFACE_SOURCE_ORM;
	source.m_nGenerationSeed = 19u;
	source.m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = { pPixels, extent, extent, 4u, extent * 4u };
}

static bool testOriginalCookerRoundtrip( void )
{
	std::vector<uint8_t> pixels( 256u * 256u * 4u );
	for ( uint32_t y = 0u; y < 256u; ++y ) {
		for ( uint32_t x = 0u; x < 256u; ++x ) {
			size_t index = ( ( size_t )y * 256u + x ) * 4u;
			pixels[index] = ( uint8_t )x;
			pixels[index + 1u] = ( uint8_t )y;
			pixels[index + 2u] = ( uint8_t )( x ^ y );
			pixels[index + 3u] = 255u;
		}
	}
	t3dVTCookSparseSource_t source{};
	configureSource( source, pixels.data(), 256u );
	t3dVTCookerSettings_t settings{};
	t3d_vt_cooker_default_settings( settings );
	settings.m_nOuterCompression = T3D_VT_OUTER_COMPRESSION_NONE;
	t3dVTCookAllocator_t allocator{ nullptr, &cookAllocate, &cookFree };
	t3dVTEncoderInterface_t encoder{};
	t3d_vt_reference_encoder_make_interface( encoder );

	t3dVTCookedSparsePackage_t first{};
	CHECK( t3d_vt_cooker_cook_sparse_pages( settings, &source, 1u, encoder, allocator, nullptr, first ) );
	CHECK( first.m_nAllocationCount == 1u && first.m_nTailCount > 0u && first.m_nPayloadCount > 0u );
	CHECK( first.m_Stats.m_nEncodeCalls > 0u );
	t3dVTSerializedPackSource_t packageSource{};
	CHECK( t3d_vt_cooker_build_pack_source( first, packageSource ) );
	uint64_t bytes = t3d_vt_pack_compute_bytes( packageSource );
	CHECK( bytes > T3D_VT_PACK_HEADER_BYTES );
	std::vector<uint8_t> serialized( ( size_t )bytes );
	uint64_t written = 0u;
	CHECK( t3d_vt_pack_write( packageSource, serialized.data(), bytes, written ) && written == bytes );
	t3dVTPackBaselineView_t view{};
	CHECK( t3d_vt_pack_open_baseline_view( serialized.data(), serialized.size(), view ) );
	CHECK( view.m_nAllocationCount == 1u && view.m_nPayloadCount == first.m_nPayloadCount );
	CHECK( view.m_nQualityViewCount == first.m_nQualityViewCount );
	CHECK( testNativeDonorRuntime( serialized.data(), serialized.size() ) );

	// Shipping data is fully cooked; removing editor pixels cannot invalidate the package.
	pixels.clear();
	pixels.shrink_to_fit();
	source.m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO].m_pPixels = nullptr;
	CHECK( t3d_vt_pack_open_baseline_view( serialized.data(), serialized.size(), view ) );

	// The donor algorithm must produce identical bytes when given the same authored source.
	std::vector<uint8_t> pixelsAgain( 256u * 256u * 4u );
	for ( uint32_t y = 0u; y < 256u; ++y ) {
		for ( uint32_t x = 0u; x < 256u; ++x ) {
			size_t index = ( ( size_t )y * 256u + x ) * 4u;
			pixelsAgain[index] = ( uint8_t )x;
			pixelsAgain[index + 1u] = ( uint8_t )y;
			pixelsAgain[index + 2u] = ( uint8_t )( x ^ y );
			pixelsAgain[index + 3u] = 255u;
		}
	}
	configureSource( source, pixelsAgain.data(), 256u );
	t3dVTCookedSparsePackage_t second{};
	CHECK( t3d_vt_cooker_cook_sparse_pages( settings, &source, 1u, encoder, allocator, nullptr, second ) );
	t3dVTSerializedPackSource_t secondSource{};
	CHECK( t3d_vt_cooker_build_pack_source( second, secondSource ) );
	std::vector<uint8_t> secondBytes( ( size_t )t3d_vt_pack_compute_bytes( secondSource ) );
	uint64_t secondWritten = 0u;
	CHECK( t3d_vt_pack_write( secondSource, secondBytes.data(), secondBytes.size(), secondWritten ) );
	CHECK( secondWritten == written && secondBytes == serialized );
	t3d_vt_cooker_release_sparse_package( second );
	t3d_vt_cooker_release_sparse_package( first );

	// Reject payload mutations and header metadata even when the package is otherwise valid.
	serialized.back() ^= 0x5au;
	CHECK( !t3d_vt_pack_open_baseline_view( serialized.data(), serialized.size(), view ) );
	serialized.back() ^= 0x5au;
	serialized[0] ^= 1u;
	CHECK( !t3d_vt_pack_open_baseline_view( serialized.data(), serialized.size(), view ) );
	return true;
}

static bool testOriginalCookerRejectsInvalidSources( void )
{
	std::vector<uint8_t> pixels( 256u * 256u * 4u, 255u );
	t3dVTCookSparseSource_t sources[2]{};
	configureSource( sources[0], pixels.data(), 256u );
	sources[1] = sources[0];
	t3dVTCookerSettings_t settings{};
	t3d_vt_cooker_default_settings( settings );
	t3dVTCookAllocator_t allocator{ nullptr, &cookAllocate, &cookFree };
	t3dVTEncoderInterface_t encoder{};
	t3d_vt_reference_encoder_make_interface( encoder );
	t3dVTCookedSparsePackage_t package{};
	CHECK( !t3d_vt_cooker_cook_sparse_pages( settings, sources, 2u, encoder, allocator, nullptr, package ) );
	CHECK( package.m_pPayloadData == nullptr );
	sources[0].m_nStableKey = 0u;
	CHECK( !t3d_vt_cooker_cook_sparse_pages( settings, sources, 1u, encoder, allocator, nullptr, package ) );
	sources[0].m_nStableKey = 1u;
	sources[0].m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO].m_nRowBytes = 1u;
	CHECK( !t3d_vt_cooker_cook_sparse_pages( settings, sources, 1u, encoder, allocator, nullptr, package ) );
	sources[0].m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO].m_nRowBytes = 256u * 4u;
	settings.m_nPageInterior = 0u;
	CHECK( !t3d_vt_cooker_cook_sparse_pages( settings, sources, 1u, encoder, allocator, nullptr, package ) );
	return true;
}

static bool testNativeZstdCookPublication( void )
{
	CHECK( SIRVirtualTextureCooker::supportsZstd() == SIRVirtualTextureZstdCodec::available() );
	if ( !SIRVirtualTextureCooker::supportsZstd() ) {
		// Native editor must reject Zstd cooking when the native dependency is missing.
		std::vector<uint8_t> bytes( 1024u );
		sirVirtualTextureCookOutput_t output{ bytes.data(), bytes.size(), 0u };
		sirVirtualTextureCookAllocator_t alloc{ nullptr, &cookAllocate, &cookFree };
		CHECK( !SIRVirtualTextureCooker::cook( { nullptr, 0u, alloc, true }, output ) );
		return true;
	}
	std::vector<uint8_t> pixels( 256u * 256u * 4u, 220u );
	sirVirtualTextureCookSource_t source{};
	source.nStableKey = UINT64_C( 0xABCDEF01 );
	source.nConsumer = T3D_VT_CONSUMER_INSTANCE;
	source.nAuthoredChannelMask = T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO );
	source.nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = { pixels.data(), 256u, 256u, 4u, 1024u, pixels.size() };
	sirVirtualTextureCookAllocator_t allocator{ nullptr, &cookAllocate, &cookFree };
	std::vector<uint8_t> cooked( 8u * 1024u * 1024u );
	sirVirtualTextureCookOutput_t output{ cooked.data(), cooked.size(), 0u };
	const uint64_t imageBacking = source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO].nBackingBytes;
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO].nBackingBytes = imageBacking - 1u;
	CHECK( !SIRVirtualTextureCooker::cook( { &source, 1u, allocator, true }, output ) );
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO].nBackingBytes = imageBacking;
	CHECK( SIRVirtualTextureCooker::cook( { &source, 1u, allocator, true }, output ) );
	CHECK( output.nWritten > 256u && output.nWritten < output.nCapacity );
	sirVirtualTexturePackView_t direct{};
	CHECK( sirVTPackOpenValidated( { output.pBytes, output.nWritten, output.nWritten, false }, &direct ) );
	CHECK( direct.header.nClusterCount == 1u && direct.header.nAllocationCount == 1u );
	sirVirtualTexturePackClusterRecord_t cluster{};
	memcpy( &cluster, direct.szDirectories[SIR_VT_PACK_CLUSTERS].pData, sizeof( cluster ) );
	CHECK( cluster.nCodec == T3D_VT_PACK_CODEC_ZSTD && cluster.nDecodedBytes >= cluster.nStoredBytes );

	static const char pszCooked[] = "sir_vt_p1_cooked_fixture.vtpack";
	static const char pszStaging[] = "sir_vt_p1_cooked_fixture.tmp";
	SIRPlatformFile::remove( pszCooked );
	SIRPlatformFile::remove( pszStaging );
	CHECK( SIRVirtualTextureCooker::publishAtomic( pszCooked, pszStaging, output ) );
	// An existing stale staging file must block publication, preserving the cooked destination.
	SIRPlatformFile staging;
	CHECK( staging.open( pszStaging, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_EXCLUSIVE ) == SIR_FILE_OK );
	CHECK( staging.close() == SIR_FILE_OK );
	CHECK( !SIRVirtualTextureCooker::publishAtomic( pszCooked, pszStaging, output ) );
	CHECK( SIRPlatformFile::remove( pszStaging ) == SIR_FILE_OK );

	// Remove all editor input and the in-memory package; runtime reads only the published V1 bytes.
	pixels.clear();
	pixels.shrink_to_fit();
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO].pPixels = nullptr;
	cooked.clear();
	cooked.shrink_to_fit();
	std::vector<uint8_t> metadata( 1024u * 1024u );
	sirVirtualTexturePackMetadataBuffer_t buffer{ metadata.data(), metadata.size() };
	SIRVirtualTexturePackFileSource file;
	sirVirtualTexturePackView_t view{};
	CHECK( file.open( pszCooked, buffer, true, &view ) );
	CHECK( file.getFileBytes() == output.nWritten );
	std::vector<uint8_t> stored( cluster.nStoredBytes );
	std::vector<uint8_t> decoded( cluster.nDecodedBytes );
	sirVirtualTextureClusterBuffers_t data{ stored.data(), ( uint32_t )stored.size(), decoded.data(), ( uint32_t )decoded.size() };
	sirVirtualTextureClusterCodec_t codec = SIRVirtualTextureZstdCodec::makeDecoder();
	CHECK( file.readCluster( view, 0u, data, &codec ) );
	CHECK( file.getReadCount() == 1u );
	// The original pack runtime now reads the same compressed package through
	// SIREngine file handles and its single allocator-owned decode workspace.
	t3dVTPackRuntimeAllocator_t runtimeAllocator{ nullptr, &packRuntimeAllocate, &packRuntimeFree };
	t3dVTPackRuntimeStorage_t compressedRuntime{};
	CHECK( t3d_vt_pack_runtime_open_file_cold( pszCooked, runtimeAllocator, compressedRuntime ) );
	CHECK( compressedRuntime.Database.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK );
	uint32_t runtimeId = T3D_VT_INVALID_ALLOCATION;
	CHECK( t3d_vt_pack_runtime_find_allocation_by_stable_key( compressedRuntime, source.nStableKey, runtimeId ) );
	CHECK( runtimeId == 0u );
	const t3dVTTailMetadata_t& tail = compressedRuntime.pRuntimeTails[0];
	CHECK( tail.m_nAtlasX == 0u && tail.m_nAtlasY == 0u );
	std::vector<uint8_t> tailAtlas( 128u * 64u / 2u );
	CHECK( t3d_vt_pack_runtime_copy_tail_mip_chain( compressedRuntime.Database, 0u, T3D_VT_CHANNEL_ALBEDO, tailAtlas.data(),
	                                                tailAtlas.size(), 128u, 64u ) );
	CHECK( compressedRuntime.Database.pFileSource->nReadCount > 0u );
	t3d_vt_pack_runtime_release_cold( runtimeAllocator, compressedRuntime );
	CHECK( compressedRuntime.pFileSource == nullptr && compressedRuntime.pOwnedBlock == nullptr );
	CHECK( !file.readCluster( view, view.header.nClusterCount, data, &codec ) );
	file.close();
	CHECK( SIRPlatformFile::remove( pszCooked ) == SIR_FILE_OK );
	return true;
}

struct ComposedCookCounter
{
	uint32_t evaluated = 0u;
	uint32_t encoded = 0u;
};

static bool evaluateComposedPage( void *pUser, const t3dComposedSurfaceCookPage_t&, uint32_t channel, uint8_t *pRgba, uint32_t nBytes )
{
	if ( pUser == nullptr || pRgba == nullptr || nBytes != T3D_COMP_SURF_RGBA_SCRATCH_BYTES ) {
		return false;
	}
	++static_cast<ComposedCookCounter *>( pUser )->evaluated;
	for ( uint32_t i = 0u; i < nBytes; i += 4u ) {
		pRgba[i] = ( uint8_t )( 45u + channel * 13u );
		pRgba[i + 1u] = ( uint8_t )( 90u + channel * 10u );
		pRgba[i + 2u] = ( uint8_t )( 180u - channel * 5u );
		pRgba[i + 3u] = 255u;
	}
	return true;
}

static bool encodeComposedPage( void *pUser, uint32_t channel, const uint8_t *pRgba, uint32_t nRgbaBytes, uint8_t *pDestination,
                                uint32_t capacity, uint32_t& written )
{
	if ( pUser == nullptr || pRgba == nullptr || pDestination == nullptr || channel >= T3D_COMP_SURF_CHANNEL_COUNT ||
	     nRgbaBytes != T3D_COMP_SURF_RGBA_SCRATCH_BYTES ) {
		return false;
	}
	++static_cast<ComposedCookCounter *>( pUser )->encoded;
	const uint32_t logical = channel == T3D_COMP_SURF_CHANNEL_ALBEDO   ? T3D_VT_LOGICAL_CHANNEL_ALBEDO
	                         : channel == T3D_COMP_SURF_CHANNEL_NORMAL ? T3D_VT_LOGICAL_CHANNEL_NORMAL
	                                                                   : T3D_VT_LOGICAL_CHANNEL_PBR;
	uint32_t components = channel == T3D_COMP_SURF_CHANNEL_NORMAL ? 2u : 4u;
	std::vector<uint8_t> normal;
	const uint8_t *pPixels = pRgba;
	if ( components == 2u ) {
		normal.resize( T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE * 2u );
		for ( uint32_t i = 0u; i < T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE; ++i ) {
			normal[2u * i] = pRgba[4u * i];
			normal[2u * i + 1u] = pRgba[4u * i + 1u];
		}
		pPixels = normal.data();
	}
	t3dVTEncodeTileRequest_t request{};
	request.m_pSourcePixels = pPixels;
	request.m_nSourceBytes = T3D_COMP_SURF_PHYSICAL_TILE * T3D_COMP_SURF_PHYSICAL_TILE * components;
	request.m_nChannel = logical;
	request.m_nFormat = ( uint32_t )t3d_vt_logical_channel_physical_family( ( t3dVTLogicalChannel_t )logical );
	request.m_nWidth = T3D_COMP_SURF_PHYSICAL_TILE;
	request.m_nHeight = T3D_COMP_SURF_PHYSICAL_TILE;
	request.m_nComponents = components;
	request.m_nQuality = 2u;
	request.m_nContractVersion = T3D_VT_FILE_FORMAT_VERSION;
	request.m_pDestination = pDestination;
	request.m_nDestinationCapacity = capacity;
	t3dVTEncoderInterface_t encoder{};
	t3d_vt_reference_encoder_make_interface( encoder );
	return t3d_vt_cooker_encode_tile( encoder, request, written );
}

static bool testDonorIncrementalComposedSidecar( void )
{
	ComposedCookCounter counter{};
	t3dComposedSurfaceCookSource_t source{};
	source.m_pUser = &counter;
	source.m_pEvaluate = &evaluateComposedPage;
	source.m_pEncode = &encodeComposedPage;
	source.m_nSourceFingerprint = UINT64_C( 0xBEEF1042 );
	source.m_bReferenceOnly = true;
	const t3dComposedSurfaceCookPage_t page{ 123u, 17u, 0u, 0u, 0u, 8u };
	uint64_t required = 0u;
	CHECK( t3d_comp_surf_cook_total_bytes( 1u, required ) );
	std::vector<uint8_t> scratch( T3D_COMP_SURF_COOK_SCRATCH_BYTES );
	std::vector<uint8_t> first( ( size_t )required );
	uint64_t written = 0u;
	CHECK( t3d_comp_surf_cook_pages_cold( &page, 1u, source, scratch.data(), ( uint32_t )scratch.size(), first.data(), first.size(),
	                                      written ) );
	CHECK( written == required && counter.evaluated == 3u && counter.encoded == 3u );
	CHECK( t3d_comp_surf_validate_sidecar_bytes( first.data(), first.size(), true ) );
	std::vector<uint8_t> incremental( ( size_t )required );
	const uint8_t clean = 0u;
	uint64_t recooked = 0u;
	CHECK( t3d_comp_surf_recook_incremental_cold( &page, 1u, source, first.data(), first.size(), &clean, scratch.data(),
	                                              ( uint32_t )scratch.size(), incremental.data(), incremental.size(), recooked ) );
	CHECK( recooked == required && incremental == first && counter.encoded == 3u );
	const uint8_t dirty = 1u;
	CHECK( t3d_comp_surf_recook_incremental_cold( &page, 1u, source, first.data(), first.size(), &dirty, scratch.data(),
	                                              ( uint32_t )scratch.size(), incremental.data(), incremental.size(), recooked ) );
	CHECK( counter.encoded == 6u && incremental == first );
	static const char pszSidecar[] = "sir_vt_composed_p1_test.t3sc";
	static const char pszTemporary[] = "sir_vt_composed_p1_test.t3sc.t3sc.tmp";
	SIRPlatformFile::remove( pszSidecar );
	SIRPlatformFile::remove( pszTemporary );
	CHECK( t3d_comp_surf_write_sidecar_cold( pszSidecar, first.data(), first.size() ) );
	CHECK( t3d_comp_surf_write_sidecar_cold( pszSidecar, first.data(), first.size() ) );
	SIRPlatformFile published;
	CHECK( published.open( pszSidecar, SIR_FILE_OPEN_READ ) == SIR_FILE_OK );
	uint64_t fileBytes = 0u;
	CHECK( published.getSize( &fileBytes ) == SIR_FILE_OK && fileBytes == first.size() );
	std::vector<uint8_t> actual( first.size() );
	uint64_t readBytes = 0u;
	CHECK( published.readAt( 0u, actual.data(), fileBytes, &readBytes ) == SIR_FILE_OK && readBytes == fileBytes );
	CHECK( actual == first );
	CHECK( published.close() == SIR_FILE_OK );
	CHECK( SIRPlatformFile::remove( pszSidecar ) == SIR_FILE_OK );
	first.back() ^= 1u;
	CHECK( !t3d_comp_surf_validate_sidecar_bytes( first.data(), first.size(), true ) );
	CHECK( !t3d_comp_surf_write_sidecar_cold( pszSidecar, first.data(), first.size() ) );
	return true;
}

static bool testNativeMultiConsumerChannelAndFormatContracts( void )
{
	// Each native consumer retains the original donor's allowed/required channel map.
	static constexpr uint32_t CONSUMERS[3] = { T3D_VT_CONSUMER_CLIPMAP, T3D_VT_CONSUMER_INSTANCE, T3D_VT_CONSUMER_COMPOSED };
	static constexpr uint32_t MASKS[3] = {
	    T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR ) |
	        T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_HEIGHT ),
	    T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_NORMAL ) |
	        T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ORM ),
	    T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR ) |
	        T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_NORMAL ) };
	std::vector<uint8_t> albedo( 512u * 512u * 4u, 173u );
	std::vector<uint8_t> pbr( 256u * 256u * 4u, 81u );
	std::vector<uint8_t> height( 256u * 256u, 200u );
	std::vector<uint8_t> normal( 256u * 256u * 2u, 127u );
	std::vector<uint8_t> orm( 256u * 256u * 4u, 69u );
	const sirVirtualTextureCookImage_t views[5] = { { albedo.data(), 512u, 512u, 4u, 2048u, albedo.size() },
	                                                { pbr.data(), 256u, 256u, 4u, 1024u, pbr.size() },
	                                                { height.data(), 256u, 256u, 1u, 256u, height.size() },
	                                                { normal.data(), 256u, 256u, 2u, 512u, normal.size() },
	                                                { orm.data(), 256u, 256u, 4u, 1024u, orm.size() } };
	sirVirtualTextureCookSource_t sources[3]{};
	for ( uint32_t i = 0u; i < 3u; ++i ) {
		sources[i].nStableKey = 42000u + i;
		sources[i].nConsumer = CONSUMERS[i];
		sources[i].nAuthoredChannelMask = MASKS[i];
		sources[i].nOrmSourceLayout = T3D_VT_SURFACE_SOURCE_ORM;
		sources[i].nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
		for ( uint32_t channel = 0u; channel < 5u; ++channel ) {
			sources[i].images[channel] = views[channel];
		}
	}
	const sirVirtualTextureCookAllocator_t allocator{ nullptr, &cookAllocate, &cookFree };
	std::vector<uint8_t> package( 32u * 1024u * 1024u );
	sirVirtualTextureCookOutput_t output{ package.data(), package.size(), 0u };
	CHECK( SIRVirtualTextureCooker::cook( { sources, 3u, allocator, false }, output ) );
	CHECK( output.nWritten >= T3D_VT_PACK_HEADER_BYTES && output.nWritten <= output.nCapacity );
	t3dVTPackBaselineView_t view{};
	CHECK( t3d_vt_pack_open_baseline_view( output.pBytes, output.nWritten, view ) );
	CHECK( view.m_nAllocationCount == 3u && view.m_nFormatFamilyCount == T3D_VT_LOGICAL_CHANNEL_COUNT );
	CHECK( view.m_nChannelAllocationCount == 3u * T3D_VT_LOGICAL_CHANNEL_COUNT );
	for ( uint32_t channel = 0u; channel < 5u; ++channel ) {
		CHECK( view.m_pHeader->m_nChannelPageCount[channel] > 0u );
	}
	CHECK( view.m_pHeader->m_nChannelPageCount[T3D_VT_LOGICAL_CHANNEL_RESERVED_5] == 0u );
	for ( uint32_t i = 0u; i < 3u; ++i ) {
		CHECK( view.m_pAllocationKeys[i].m_nStableKey == sources[i].nStableKey );
		CHECK( view.m_pAllocations[i].m_nConsumer == CONSUMERS[i] );
		CHECK( view.m_pAllocations[i].m_nChannelMask == MASKS[i] );
	}
	bool seenFormats[T3D_VT_PHYSICAL_FORMAT_COUNT]{};
	for ( uint32_t i = 0u; i < view.m_nPayloadCount; ++i ) {
		const uint32_t family = view.m_pPayloads[i].m_nFormat;
		CHECK( family < T3D_VT_PHYSICAL_FORMAT_COUNT );
		seenFormats[family] = true;
	}
	CHECK( seenFormats[T3D_VT_PHYSICAL_FORMAT_BC1_UNORM] );
	CHECK( seenFormats[T3D_VT_PHYSICAL_FORMAT_BC7_UNORM] );
	CHECK( seenFormats[T3D_VT_PHYSICAL_FORMAT_BC5_UNORM] );
	CHECK( seenFormats[T3D_VT_PHYSICAL_FORMAT_R8_UNORM] );

	// No live authoring byte array is needed to reread the finished package.
	albedo.clear();
	pbr.clear();
	height.clear();
	normal.clear();
	orm.clear();
	CHECK( t3d_vt_pack_open_baseline_view( output.pBytes, output.nWritten, view ) );
	return true;
}

struct NativeColdCookState
{
	uint64_t revision = 17u;
	uint32_t controls = 0u;
};

static bool nativeColdResolvePage( void *, const t3dComposedSurfaceCookPage_t&, t3dComposedStaticFootprint_t& footprint )
{
	footprint = {};
	footprint.m_fWorldStepX[0] = 1.0f;
	footprint.m_fWorldStepY[1] = 1.0f;
	footprint.m_fControlStepX[0] = 1.0f;
	footprint.m_fControlStepY[1] = 1.0f;
	return true;
}

static bool nativeColdReadControl( void *pUser, int32_t, int32_t, uint32_t& control )
{
	++static_cast<NativeColdCookState *>( pUser )->controls;
	control = 0u;
	return true;
}

static bool nativeColdIsCurrent( void *pUser, uint64_t terrain, uint64_t material, uint64_t topology )
{
	return terrain == static_cast<NativeColdCookState *>( pUser )->revision && material == 23u && topology == 31u;
}

static bool nativeColdDecodeTile( void *, uint32_t family, const uint8_t *pSource, uint32_t sourceBytes, uint8_t *pDestination,
                                  uint32_t capacity )
{
	const bool decoded = t3d_comp_surf_decode_reference_tile( family, pSource, sourceBytes, pDestination, capacity );
	if ( !decoded ) {
		fprintf( stderr, "decode failed family=%u sourceBytes=%u capacity=%u\n", family, sourceBytes, capacity );
	}
	return decoded;
}

static bool testComposedColdPipelineFromActualV1( void )
{
	// The original donor's source-pack composition pipeline must consume a real
	// V1 package, rather than the stubbed parser used by donor unit fixtures.
	std::vector<uint8_t> albedo( 256u * 256u * 4u, 190u );
	std::vector<uint8_t> pbr( 128u * 128u * 4u, 91u );
	sirVirtualTextureCookSource_t source{};
	source.nStableKey = t3d_vt_terrain_material_stable_key( 0u );
	source.nConsumer = T3D_VT_CONSUMER_CLIPMAP;
	source.nAuthoredChannelMask =
	    T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR );
	source.nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = { albedo.data(), 256u, 256u, 4u, 1024u, albedo.size() };
	source.images[T3D_VT_LOGICAL_CHANNEL_PBR] = { pbr.data(), 128u, 128u, 4u, 512u, pbr.size() };
	const sirVirtualTextureCookAllocator_t allocator{ nullptr, &cookAllocate, &cookFree };
	std::vector<uint8_t> package( 8u * 1024u * 1024u );
	sirVirtualTextureCookOutput_t output{ package.data(), package.size(), 0u };
	CHECK( SIRVirtualTextureCooker::cook( { &source, 1u, allocator, false }, output ) );
	albedo.clear();
	pbr.clear();
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO].pPixels = nullptr;
	source.images[T3D_VT_LOGICAL_CHANNEL_PBR].pPixels = nullptr;

	t3dComposedStaticLayer_t authored{};
	authored.m_nAllocation = UINT32_MAX;
	authored.m_fColor[0] = 1.0f;
	authored.m_fColor[1] = 1.0f;
	authored.m_fColor[2] = 1.0f;
	authored.m_fPbr0[0] = 1.0f;
	authored.m_fPbr0[1] = 1.0f;
	authored.m_fPbr1[0] = 1.0f;
	authored.m_fPbr1[1] = 1.0f;
	t3dComposedStaticLayer_t resolved{};
	const t3dComposedSurfaceCookPage_t page{ 120u, 1u, 0u, 0u, 0u, 1u };
	uint64_t sidecarBytes = 0u;
	CHECK( t3d_comp_surf_cook_total_bytes( 1u, sidecarBytes ) );
	std::vector<uint8_t> decodedCache( 6u * T3D_COMPOSED_SURFACE_PACK_DECODED_TILE_BYTES );
	std::vector<uint8_t> scratch( T3D_COMP_SURF_COOK_SCRATCH_BYTES );
	std::vector<uint8_t> sidecar( ( size_t )sidecarBytes );
	NativeColdCookState state{};
	ComposedCookCounter encoderCounter{};
	t3dComposedSurfaceColdCookRequest_t request{};
	request.m_pSourcePackage = output.pBytes;
	request.m_nSourcePackageBytes = output.nWritten;
	request.m_pPages = &page;
	request.m_nPageCount = 1u;
	request.m_pAuthoredLayers = &authored;
	request.m_nLayerCount = 1u;
	request.m_pResolvedLayers = &resolved;
	request.m_nResolvedLayerCapacity = 1u;
	request.m_source.m_pUser = &state;
	request.m_source.m_pResolvePage = &nativeColdResolvePage;
	request.m_source.m_pReadControl = &nativeColdReadControl;
	request.m_source.m_nSamplingMode = T3D_COMP_SURF_STATIC_SAMPLE_NEAREST;
	request.m_source.m_nVtFiltering = 3u;
	request.m_source.m_fBlendSharpness = 0.5f;
	request.m_source.m_bCanonicalMaterialParityAccepted = true;
	request.m_pEncode = &encodeComposedPage;
	request.m_pEncoderUser = &encoderCounter;
	request.m_pDecode = &nativeColdDecodeTile;
	request.m_pDecodedCache = decodedCache.data();
	request.m_nCacheSlots = 6u;
	request.m_pCookScratch = scratch.data();
	request.m_nCookScratchBytes = ( uint32_t )scratch.size();
	request.m_pOutput = sidecar.data();
	request.m_nOutputCapacity = sidecar.size();
	request.m_nTerrainRevision = 17u;
	request.m_nMaterialRevision = 23u;
	request.m_nTopologyGeneration = 31u;
	request.m_pCurrent = &nativeColdIsCurrent;
	request.m_pCurrentUser = &state;
	const uint64_t fingerprint = t3d_composed_surface_source_fingerprint_cold( request );
	CHECK( fingerprint != 0u );
	t3dComposedSurfaceColdCookResult_t result{};
	const bool bColdCook = t3d_composed_surface_cook_source_pack_cold( request, result );
	if ( !bColdCook ) {
		fprintf( stderr, "Composed cook status: %u controls=%u decodes=%u rev=%llu\n", ( unsigned )result.m_status, state.controls,
		         result.m_nDecodedTiles, ( unsigned long long )state.revision );
	}
	CHECK( bColdCook );
	CHECK( result.m_status == T3D_COMPOSED_SURFACE_COLD_COOK_SUCCESS );
	CHECK( result.m_nWrittenBytes == sidecarBytes && result.m_nSourceFingerprint == fingerprint );
	CHECK( resolved.m_nAllocation == 0u && state.controls > 0u && result.m_nDecodedTiles > 0u );
	CHECK( t3d_comp_surf_validate_sidecar_bytes( sidecar.data(), sidecar.size(), true ) );
	const std::vector<uint8_t> originalSidecar = sidecar;
	const uint8_t clean = 0u;
	request.m_pPreviousSidecar = originalSidecar.data();
	request.m_nPreviousSidecarBytes = originalSidecar.size();
	request.m_pDirtyFlags = &clean;
	CHECK( t3d_composed_surface_cook_source_pack_cold( request, result ) );
	CHECK( sidecar == originalSidecar );
	++state.revision;
	CHECK( !t3d_composed_surface_cook_source_pack_cold( request, result ) );
	CHECK( result.m_status == T3D_COMPOSED_SURFACE_COLD_COOK_STALE_REVISION );
	return true;
}

static bool testNativeIncrementalMegaVTRegion( void )
{
	// Exact Terrain3D 3x3 bilinear/linear-to-sRGB and PBR recipe, borrowed by
	// the SIREngine terrain editor. No Godot Image or PackedByteArray involved.
	constexpr uint32_t nExtent = 256u;
	constexpr uint32_t nSourceSide = 64u;
	std::vector<uint8_t> center( nSourceSide * nSourceSide * 4u, 0u );
	std::vector<uint8_t> east( nSourceSide * nSourceSide * 4u, 0u );
	for ( uint32_t i = 0u; i < nSourceSide * nSourceSide; ++i ) {
		center[4u * i + 0u] = 128u;
		center[4u * i + 1u] = 64u;
		center[4u * i + 2u] = 0u;
		center[4u * i + 3u] = 100u;
		east[4u * i + 0u] = 255u;
		east[4u * i + 1u] = 0u;
		east[4u * i + 2u] = 0u;
		east[4u * i + 3u] = 200u;
	}
	sirVirtualTextureRegionCookRequest_t region{};
	region.nExtent = nExtent;
	region.nRegionIndex = 42u;
	region.nGeneration = 8u;
	region.neighbors[4] = { center.data(), nSourceSide, nSourceSide, 4u, nSourceSide * 4u, center.size() };
	region.neighbors[5] = { east.data(), nSourceSide, nSourceSide, 4u, nSourceSide * 4u, east.size() };
	std::vector<uint8_t> albedo( nExtent * nExtent * 4u, 0u );
	std::vector<uint8_t> pbr( ( nExtent / 2u ) * ( nExtent / 2u ) * 4u, 0u );
	const sirVirtualTextureRegionSurfaces_t surfaces{ albedo.data(), albedo.size(), pbr.data(), pbr.size() };
	CHECK( SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	CHECK( albedo[0] > 185u && albedo[0] < 190u );
	CHECK( albedo[1] > 130u && albedo[1] < 140u );
	CHECK( albedo[3] == 255u );
	CHECK( pbr[0] == 128u && pbr[1] == 128u && pbr[2] == 100u && pbr[3] == 255u );
	CHECK( albedo[( ( uint64_t )128u * nExtent + 255u ) * 4u] > albedo[0] );
	// Cached pixels outside the single dirty 128x128 mip0 page remain stable.
	const uint8_t oldBottomRight = albedo[( ( uint64_t )255u * nExtent + 255u ) * 4u];
	const uint8_t oldTopLeft = albedo[0];
	for ( uint32_t i = 0u; i < nSourceSide * nSourceSide; ++i ) {
		center[4u * i] = 255u;
	}
	region.bReuseSurfaces = true;
	region.nPreviousRegionIndex = region.nRegionIndex;
	region.nPreviousExtent = region.nExtent;
	region.nDirtyMip0Mask = 1u;
	CHECK( SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	CHECK( albedo[0] > oldTopLeft );
	region.nPreviousRegionIndex = region.nRegionIndex + 1u;
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	region.nPreviousRegionIndex = region.nRegionIndex;
	CHECK( albedo[( ( uint64_t )255u * nExtent + 255u ) * 4u] == oldBottomRight );
	const sirVirtualTextureCookAllocator_t allocator{ nullptr, &cookAllocate, &cookFree };
	std::vector<uint8_t> package( 12u * 1024u * 1024u );
	sirVirtualTextureCookOutput_t output{ package.data(), package.size(), 0u };
	const sirVirtualTextureRegionPackRequest_t pack{ region, surfaces, allocator, false };
	CHECK( SIRVirtualTextureRegionCooker::cook( pack, output ) );
	t3dVTPackBaselineView_t view{};
	CHECK( t3d_vt_pack_open_baseline_view( output.pBytes, output.nWritten, view ) );
	CHECK( view.m_nAllocationCount == 1u );
	CHECK( view.m_pAllocationKeys[0].m_nStableKey == t3d_vt_terrain_region_stable_key( 42u ) );
	CHECK( view.m_pAllocations[0].m_nChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR ) );
	// The cooked allocation is self-contained after dropping source snapshots.
	region.neighbors[4] = {};
	region.neighbors[5] = {};
	center.clear();
	east.clear();
	CHECK( t3d_vt_pack_open_baseline_view( output.pBytes, output.nWritten, view ) );
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	region.neighbors[4] = { albedo.data(), 64u, 64u, 4u, 64u * 4u, 16u };
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	region.neighbors[4].nBackingBytes = albedo.size();
	region.nExtent = 123u;
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	// Never allow destination surfaces to alias source images or one another.
	region.nExtent = 256u;
	region.neighbors[4] = { albedo.data(), 64u, 64u, 4u, 256u, albedo.size() };
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	region.neighbors[4] = { pbr.data(), 64u, 64u, 4u, 256u, pbr.size() };
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	region.neighbors[4] = { albedo.data(), UINT32_MAX, 1u, 4u, 0u, albedo.size() };
	CHECK( !SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	return true;
}

static bool testNativeMegaVTRegion512( void )
{
	std::vector<uint8_t> authored( 64u * 64u * 4u, 90u );
	std::vector<uint8_t> albedo( 512u * 512u * 4u );
	std::vector<uint8_t> pbr( 256u * 256u * 4u );
	sirVirtualTextureRegionCookRequest_t region{};
	region.nExtent = 512u;
	region.nRegionIndex = 19u;
	region.nGeneration = 3u;
	region.neighbors[4] = { authored.data(), 64u, 64u, 4u, 256u, authored.size() };
	const sirVirtualTextureRegionSurfaces_t surfaces{ albedo.data(), albedo.size(), pbr.data(), pbr.size() };
	CHECK( SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	const uint8_t topLeft = albedo[0];
	const uint8_t bottomRight = albedo[( ( uint64_t )511u * 512u + 511u ) * 4u];
	for ( uint32_t i = 0; i < authored.size() / 4u; ++i ) {
		authored[4u * i] = 220u;
	}
	region.bReuseSurfaces = true;
	region.nPreviousRegionIndex = 19u;
	region.nPreviousExtent = 512u;
	region.nDirtyMip0Mask = 1u << 15u;
	CHECK( SIRVirtualTextureRegionCooker::compose( region, surfaces ) );
	CHECK( albedo[0] == topLeft );
	CHECK( albedo[( ( uint64_t )511u * 512u + 511u ) * 4u] > bottomRight );
	return true;
}

struct NativeRegionRevisionState
{
	uint32_t nCalls = 0u;
	uint32_t nFailOnCall = UINT32_MAX;
};

static bool nativeRegionCurrent( void *pUser, uint32_t nRegionIndex, uint32_t nGeneration )
{
	NativeRegionRevisionState& state = *static_cast<NativeRegionRevisionState *>( pUser );
	return state.nCalls++ != state.nFailOnCall && nRegionIndex < 1024u && nGeneration == 17u;
}

static bool testNativeMegaVTMultiRegionBatch( void )
{
	// Two staged terrain regions share one donor V1 package and allocator,
	// matching the donor's bounded dirty-region batch publication behavior.
	std::vector<uint8_t> sourceA( 64u * 64u * 4u, 90u );
	std::vector<uint8_t> sourceB( 64u * 64u * 4u, 180u );
	std::vector<uint8_t> albedoA( 256u * 256u * 4u );
	std::vector<uint8_t> albedoB( 256u * 256u * 4u );
	std::vector<uint8_t> pbrA( 128u * 128u * 4u );
	std::vector<uint8_t> pbrB( 128u * 128u * 4u );
	sirVirtualTextureRegionCookRequest_t regions[2]{};
	sirVirtualTextureRegionSurfaces_t surfaces[2]{};
	regions[0].nExtent = regions[1].nExtent = 256u;
	regions[0].nGeneration = regions[1].nGeneration = 17u;
	regions[0].nRegionIndex = 12u;
	regions[1].nRegionIndex = 13u;
	regions[0].neighbors[4] = { sourceA.data(), 64u, 64u, 4u, 256u, sourceA.size() };
	regions[1].neighbors[4] = { sourceB.data(), 64u, 64u, 4u, 256u, sourceB.size() };
	surfaces[0] = { albedoA.data(), albedoA.size(), pbrA.data(), pbrA.size() };
	surfaces[1] = { albedoB.data(), albedoB.size(), pbrB.data(), pbrB.size() };
	sirVirtualTextureCookSource_t sourceScratch[2]{};
	NativeRegionRevisionState revision{};
	sirVirtualTextureRegionBatchRequest_t batch{};
	batch.pRegions = regions;
	batch.pSurfaces = surfaces;
	batch.pCookScratch = sourceScratch;
	batch.nRegionCount = 2u;
	batch.nSourceCapacity = 2u;
	batch.allocator = { nullptr, &cookAllocate, &cookFree };
	batch.pfnIsCurrent = &nativeRegionCurrent;
	batch.pCurrentUser = &revision;
	std::vector<uint8_t> package( 16u * 1024u * 1024u );
	sirVirtualTextureCookOutput_t output{ package.data(), package.size(), 0u };
	CHECK( SIRVirtualTextureRegionCooker::cookBatch( batch, output ) );
	t3dVTPackBaselineView_t view{};
	CHECK( t3d_vt_pack_open_baseline_view( output.pBytes, output.nWritten, view ) );
	CHECK( view.m_nAllocationCount == 2u );
	CHECK( view.m_pAllocationKeys[0].m_nStableKey == t3d_vt_terrain_region_stable_key( 12u ) );
	CHECK( view.m_pAllocationKeys[1].m_nStableKey == t3d_vt_terrain_region_stable_key( 13u ) );
	CHECK( revision.nCalls == 4u );
	// Reject changed source generations *after* a successful cook, without
	// accepting those cooked bytes as a publication-ready package.
	revision.nCalls = 0u;
	revision.nFailOnCall = 3u;
	CHECK( !SIRVirtualTextureRegionCooker::cookBatch( batch, output ) );
	CHECK( output.nWritten == 0u );
	revision.nFailOnCall = UINT32_MAX;
	revision.nCalls = 0u;
	regions[1].nRegionIndex = regions[0].nRegionIndex;
	CHECK( !SIRVirtualTextureRegionCooker::cookBatch( batch, output ) );
	CHECK( output.nWritten == 0u );
	regions[1].nRegionIndex = 13u;
	batch.pfnIsCurrent = nullptr;
	CHECK( !SIRVirtualTextureRegionCooker::cookBatch( batch, output ) );
	CHECK( output.nWritten == 0u );
	batch.pfnIsCurrent = &nativeRegionCurrent;
	batch.nSourceCapacity = 1u;
	CHECK( !SIRVirtualTextureRegionCooker::cookBatch( batch, output ) );
	CHECK( output.nWritten == 0u );
	batch.nSourceCapacity = 2u;
	batch.pfnIsCurrent = &nativeRegionCurrent;
	revision.nCalls = 0u;
	revision.nFailOnCall = UINT32_MAX;
	static const char pszPublished[] = "sir_terrain_p1_native_editor.vtpack";
	static const char pszStaged[] = "sir_terrain_p1_native_editor.tmp";
	SIRPlatformFile::remove( pszPublished );
	SIRPlatformFile::remove( pszStaged );
	const sirTerrainVTCookPublication_t publication{ batch, &output, pszPublished, pszStaged };
	CHECK( SIRTerrainVirtualTextureCookBridge::cookAndPublish( publication ) == SIR_TERRAIN_VT_COOK_SUCCESS );
	CHECK( output.nWritten > T3D_VT_PACK_HEADER_BYTES );
	CHECK( revision.nCalls == 8u );
	// Native SIREngine Terrain owns cooked publication; runtime no longer
	// needs its editable source image buffers after successful publication.
	sourceA.clear();
	sourceB.clear();
	regions[0].neighbors[4] = {};
	regions[1].neighbors[4] = {};
	t3dVTPackRuntimeAllocator_t runtimeAllocator{ nullptr, &packRuntimeAllocate, &packRuntimeFree };
	t3dVTPackRuntimeStorage_t runtime{};
	CHECK( t3d_vt_pack_runtime_open_file_cold( pszPublished, runtimeAllocator, runtime ) );
	CHECK( runtime.Database.nRuntimeKind == T3D_VT_RUNTIME_DATABASE_PACK );
	uint32_t allocation = T3D_VT_INVALID_ALLOCATION;
	CHECK( t3d_vt_pack_runtime_find_allocation_by_stable_key( runtime, t3d_vt_terrain_region_stable_key( 12u ), allocation ) );
	CHECK( allocation == 0u );
	CHECK( t3d_vt_pack_runtime_find_allocation_by_stable_key( runtime, t3d_vt_terrain_region_stable_key( 13u ), allocation ) );
	CHECK( allocation == 1u );
	t3d_vt_pack_runtime_release_cold( runtimeAllocator, runtime );
	CHECK( SIRPlatformFile::remove( pszPublished ) == SIR_FILE_OK );
	// Stale edited inputs must not replace an earlier cooked package.
	revision.nCalls = 0u;
	revision.nFailOnCall = 0u;
	CHECK( SIRTerrainVirtualTextureCookBridge::cookAndPublish( publication ) == SIR_TERRAIN_VT_COOK_STALE_REVISION );
	CHECK( output.nWritten == 0u );
	CHECK( !t3d_vt_pack_runtime_open_file_cold( pszPublished, runtimeAllocator, runtime ) );
	return true;
}

int main( void )
{
	if ( !testOriginalCookerRoundtrip() || !testOriginalCookerRejectsInvalidSources() || !testNativeZstdCookPublication() ||
	     !testDonorIncrementalComposedSidecar() || !testNativeMultiConsumerChannelAndFormatContracts() ||
	     !testComposedColdPipelineFromActualV1() || !testNativeIncrementalMegaVTRegion() || !testNativeMegaVTMultiRegionBatch() ||
	     !testNativeMegaVTRegion512() ) {
		return 1;
	}
	puts( "donor native Sparse V1 cooker roundtrip and negative tests passed" );
	return 0;
}
