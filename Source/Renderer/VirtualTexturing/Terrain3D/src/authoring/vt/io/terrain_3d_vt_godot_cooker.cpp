// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "Terrain3DTypes.h"

#include "authoring/vt/io/terrain_3d_vt_godot_cooker.h"
#include <EASTL/sort.h>

#include "authoring/vt/build/terrain_3d_vt_reference_encoder.h"
#include "authoring/vt/build/terrain_3d_composed_surface_godot_decode.h"
#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"
#include "rendering/rd/vt/runtime/terrain_3d_composed_surface_cache.h"
#include "assets/resources/terrain_3d_mesh_asset.h"
#include "assets/resources/terrain_3d_texture_asset.h"
#include "memory/core/terrain_3d_memory.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include "data/persistence/terrain_3d_file_publication.h"
#include "core/utility/terrain_3d_hash_bytes.h"
#include "core/utility/terrain_3d_checked_math.h"
#include "core/utility/terrain_3d_numeric.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include <stdint.h>
#include <math.h>
#include <string.h>

using namespace godot;

static bool t3d_vt_godot_extract_texture( const Ref<Texture2D>& rTexture, const Image::Format eFormat, PackedByteArray& rBytes,
                                          t3dVTCookImageView_t& rView )
{
	memset( &rView, 0, sizeof( rView ) );
	rBytes.clear();
	if ( rTexture.is_null() ) {
		return true;
	}
	Ref<Image> pImage = rTexture->get_image();
	if ( pImage.is_null() || pImage->is_empty() ) {
		return false;
	}
	pImage = pImage->duplicate();
	if ( pImage.is_null() ) {
		return false;
	}
	if ( pImage->is_compressed() && pImage->decompress() != OK ) {
		return false;
	}
	if ( pImage->has_mipmaps() ) {
		pImage->clear_mipmaps();
	}
	pImage->convert( eFormat );
	if ( pImage->get_format() != eFormat ) {
		return false;
	}
	rBytes = pImage->get_data();
	if ( rBytes.is_empty() ) {
		return false;
	}
	uint32_t nComponents = 0u;
	switch ( eFormat ) {
	case Image::FORMAT_R8:
		nComponents = 1u;
		break;
	case Image::FORMAT_RG8:
		nComponents = 2u;
		break;
	case Image::FORMAT_RGBA8:
		nComponents = 4u;
		break;
	default:
		return false;
	}
	const uint64_t nRequiredBytes = ( uint64_t )pImage->get_width() * pImage->get_height() * nComponents;
	if ( nRequiredBytes != ( uint64_t )rBytes.size() ) {
		return false;
	}
	rView.m_pPixels = rBytes.ptr();
	rView.m_nWidth = ( uint32_t )pImage->get_width();
	rView.m_nHeight = ( uint32_t )pImage->get_height();
	rView.m_nComponents = nComponents;
	rView.m_nRowBytes = rView.m_nWidth * nComponents;
	return true;
}
static bool t3d_vt_godot_tile_has_cutout_alpha( const t3dVTEncodeTileRequest_t& rRequest )
{
	if ( rRequest.m_nComponents < 4u ) {
		return false;
	}
	const uint32_t nPixels = rRequest.m_nWidth * rRequest.m_nHeight;
	for ( uint32_t iPixel = 0u; iPixel < nPixels; ++iPixel ) {
		if ( rRequest.m_pSourcePixels[iPixel * rRequest.m_nComponents + 3u] < 128u ) {
			return true;
		}
	}
	return false;
}

static bool t3d_vt_godot_reference_fallback( const t3dVTEncodeTileRequest_t& rRequest, uint32_t& rWrittenBytes )
{
	t3dVTEncoderInterface_t ReferenceEncoder;
	t3d_vt_reference_encoder_make_interface( ReferenceEncoder );
	return ReferenceEncoder.m_pEncodeTile( ReferenceEncoder.m_pUser, rRequest, rWrittenBytes );
}

static bool t3d_vt_godot_encode_tile( void *pUser, const t3dVTEncodeTileRequest_t& rRequest, uint32_t& rWrittenBytes )
{
	( void )pUser;
	rWrittenBytes = 0u;
	const bool bV1 = rRequest.m_nContractVersion == T3D_VT_FILE_FORMAT_VERSION;
	// Quality 0 favors deterministic reference encoding over the Godot compressor.
	if ( rRequest.m_nQuality == 0u ) {
		return t3d_vt_godot_reference_fallback( rRequest, rWrittenBytes );
	}
	const uint32_t nBc1Format = bV1 ? T3D_VT_PHYSICAL_FORMAT_BC1_UNORM : T3D_VT_FORMAT_ALBEDO_BC1_SRGB;
	const uint32_t nBc7Format = bV1 ? T3D_VT_PHYSICAL_FORMAT_BC7_UNORM : T3D_VT_FORMAT_SURFACE_BC7_UNORM;
	const uint32_t nBc5Format = bV1 ? T3D_VT_PHYSICAL_FORMAT_BC5_UNORM : T3D_VT_FORMAT_NORMAL_BC5_UNORM;
	const uint32_t nHeightFormat = bV1 ? T3D_VT_PHYSICAL_FORMAT_R8_UNORM : T3D_VT_FORMAT_HEIGHT_R8_UNORM;
	if ( rRequest.m_nFormat == nHeightFormat || ( bV1 && rRequest.m_nFormat == T3D_VT_PHYSICAL_FORMAT_R16_UNORM ) ) {
		memcpy( rRequest.m_pDestination, rRequest.m_pSourcePixels, rRequest.m_nSourceBytes );
		rWrittenBytes = rRequest.m_nSourceBytes;
		return true;
	}
	const bool bAlbedo = bV1 ? rRequest.m_nChannel == T3D_VT_LOGICAL_CHANNEL_ALBEDO : rRequest.m_nFormat == T3D_VT_FORMAT_ALBEDO_BC1_SRGB;
	if ( bAlbedo && rRequest.m_nFormat == nBc1Format && t3d_vt_godot_tile_has_cutout_alpha( rRequest ) ) {
		return t3d_vt_godot_reference_fallback( rRequest, rWrittenBytes );
	}

	Terrain3DMemoryReservation source_memory;
	if ( !source_memory.resize( uint64_t( rRequest.m_nSourceBytes ) * 5u ) ) {
		return false;
	}
	PackedByteArray SourceBytes;
	SourceBytes.resize( rRequest.m_nSourceBytes );
	memcpy( SourceBytes.ptrw(), rRequest.m_pSourcePixels, rRequest.m_nSourceBytes );
	Image::Format eSourceFormat = Image::FORMAT_RGBA8;
	if ( rRequest.m_nFormat == nBc5Format ) {
		eSourceFormat = Image::FORMAT_RG8;
	}
	Ref<Image> pImage =
	    Image::create_from_data( ( int32_t )rRequest.m_nWidth, ( int32_t )rRequest.m_nHeight, false, eSourceFormat, SourceBytes );
	if ( pImage.is_null() || pImage->is_empty() ) {
		return t3d_vt_godot_reference_fallback( rRequest, rWrittenBytes );
	}

	Error eError = ERR_UNAVAILABLE;
	Image::Format eExpectedFormat = Image::FORMAT_RGBA8;
	if ( rRequest.m_nFormat == nBc1Format ) {
		pImage->convert( Image::FORMAT_RGB8 );
		eError = pImage->compress_from_channels( Image::COMPRESS_S3TC, Image::USED_CHANNELS_RGB );
		eExpectedFormat = Image::FORMAT_DXT1;
	} else if ( rRequest.m_nFormat == nBc5Format ) {
		pImage->convert( Image::FORMAT_RG8 );
		eError = pImage->compress_from_channels( Image::COMPRESS_S3TC, Image::USED_CHANNELS_RG );
		eExpectedFormat = Image::FORMAT_RGTC_RG;
	} else if ( rRequest.m_nFormat == nBc7Format ) {
		pImage->convert( Image::FORMAT_RGBA8 );
		eError = pImage->compress( Image::COMPRESS_BPTC, Image::COMPRESS_SOURCE_GENERIC );
		eExpectedFormat = Image::FORMAT_BPTC_RGBA;
	}
	if ( eError != OK || pImage->get_format() != eExpectedFormat ) {
		return t3d_vt_godot_reference_fallback( rRequest, rWrittenBytes );
	}
	const PackedByteArray Compressed = pImage->get_data();
	const uint32_t nExpectedBytes =
	    bV1 ? t3d_vt_pack_expected_physical_tile_bytes( rRequest.m_nFormat ) : t3d_vt_pack_expected_tile_bytes( rRequest.m_nFormat );
	if ( Compressed.size() != ( int64_t )nExpectedBytes || Compressed.size() > ( int64_t )rRequest.m_nDestinationCapacity ) {
		return t3d_vt_godot_reference_fallback( rRequest, rWrittenBytes );
	}
	memcpy( rRequest.m_pDestination, Compressed.ptr(), ( size_t )Compressed.size() );
	rWrittenBytes = ( uint32_t )Compressed.size();
	if ( bAlbedo && rRequest.m_nFormat == nBc7Format && rRequest.m_nAlphaThreshold != 0u && rRequest.m_nQuality >= 2u ) {
		// Retain BPTC's color quality; repair only blocks whose decoded cutoff
		// coverage changed. The reference RGBA fit constrains each alpha class.
		if ( pImage->decompress() != OK ) {
			return t3d_vt_godot_reference_fallback( rRequest, rWrittenBytes );
		}
		pImage->convert( Image::FORMAT_RGBA8 );
		const PackedByteArray decoded = pImage->get_data();
		for ( uint32_t by = 0u; by < rRequest.m_nHeight / 4u; ++by ) {
			for ( uint32_t bx = 0u; bx < rRequest.m_nWidth / 4u; ++bx ) {
				uint8_t block[64];
				bool coverage_changed = false;
				for ( uint32_t y = 0u; y < 4u; ++y ) {
					for ( uint32_t x = 0u; x < 4u; ++x ) {
						const uint32_t offset = ( ( by * 4u + y ) * rRequest.m_nWidth + bx * 4u + x ) * 4u;
						memcpy( block + ( y * 4u + x ) * 4u, rRequest.m_pSourcePixels + offset, 4u );
						coverage_changed |= ( rRequest.m_pSourcePixels[offset + 3u] >= rRequest.m_nAlphaThreshold ) !=
						                    ( decoded[offset + 3u] >= rRequest.m_nAlphaThreshold );
					}
				}
				if ( coverage_changed ) {
					t3dVTEncodeTileRequest_t repair = rRequest;
					repair.m_pSourcePixels = block;
					repair.m_nSourceBytes = sizeof( block );
					repair.m_nWidth = 4u;
					repair.m_nHeight = 4u;
					repair.m_pDestination = rRequest.m_pDestination + ( by * ( rRequest.m_nWidth / 4u ) + bx ) * 16u;
					repair.m_nDestinationCapacity = 16u;
					uint32_t written = 0u;
					if ( !t3d_vt_godot_reference_fallback( repair, written ) || written != 16u ) {
						return false;
					}
				}
			}
		}
	}
	return true;
}

void t3d_vt_godot_make_encoder( t3dVTEncoderInterface_t& rEncoder )
{
	rEncoder.m_pUser = NULL;
	rEncoder.m_pEncodeTile = t3d_vt_godot_encode_tile;
}

static uint64_t t3d_vt_godot_hash_u64( uint64_t nHash, const uint64_t nValue )
{
	return t3dHashFnv1a64( &nValue, sizeof( nValue ), nHash );
}

static void t3d_vt_godot_clear_source( t3dVTGodotSource_t& rSource )
{
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		rSource.m_ChannelBytes[iChannel].clear();
		memset( &rSource.m_Channels[iChannel], 0, sizeof( rSource.m_Channels[iChannel] ) );
		memset( &rSource.m_ChannelDesc[iChannel], 0, sizeof( rSource.m_ChannelDesc[iChannel] ) );
		rSource.m_ChannelDesc[iChannel].m_nLogicalChannel = ( uint8_t )iChannel;
	}
	rSource.m_nStableKey = 0u;
	rSource.m_nAuthoredChannelMask = 0u;
	rSource.m_nSourceFlags = 0u;
	rSource.m_nOrmSourceLayout = 0u;
	rSource.m_nConsumer = 0u;
	rSource.m_eHighestAvailableProfile = T3D_VT_FIXED_PROFILE_INVALID;
}

static uint32_t t3d_vt_godot_source_extent( const t3dVTCookImageView_t& rView )
{
	uint32_t nExtent = rView.m_nWidth < rView.m_nHeight ? rView.m_nWidth : rView.m_nHeight;
	if ( nExtent > T3D_VT_MAX_AUTHORED_EXTENT ) {
		nExtent = T3D_VT_MAX_AUTHORED_EXTENT;
	}
	return nExtent;
}

static uint64_t t3d_vt_godot_channel_fingerprint( const t3dVTCookImageView_t& rView, const PackedByteArray& rBytes,
                                                  const t3dVTLogicalChannel_t Channel )
{
	if ( rView.m_pPixels == NULL || rBytes.is_empty() ) {
		return 0u;
	}
	uint64_t nHash = 1469598103934665603ULL;
	nHash = t3d_vt_godot_hash_u64( nHash, ( uint32_t )Channel );
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO ) {
		nHash = t3d_vt_godot_hash_u64( nHash, T3D_VT_REFERENCE_ALBEDO_CODEC_REVISION );
	}
	nHash = t3d_vt_godot_hash_u64( nHash, rView.m_nWidth );
	nHash = t3d_vt_godot_hash_u64( nHash, rView.m_nHeight );
	nHash = t3d_vt_godot_hash_u64( nHash, rView.m_nComponents );
	nHash = t3dHashFnv1a64( rBytes.ptr(), ( uint64_t )rBytes.size(), nHash );
	return nHash == 0u ? 1u : nHash;
}

static void t3d_vt_godot_finalize_source_channel( t3dVTGodotSource_t& rSource, const t3dVTLogicalChannel_t Channel,
                                                  const t3dVTConsumer_t Consumer, const uint32_t nWrapMode, const bool bAlphaScissor )
{
	const uint32_t iChannel = ( uint32_t )Channel;
	t3dVTGodotSourceChannel_t& desc = rSource.m_ChannelDesc[iChannel];
	const t3dVTCookImageView_t& view = rSource.m_Channels[iChannel];
	desc.m_nLogicalChannel = ( uint8_t )Channel;
	desc.m_nConsumer = ( uint8_t )Consumer;
	desc.m_nWrapMode = ( uint8_t )nWrapMode;
	desc.m_bAlphaScissor = bAlphaScissor ? 1u : 0u;
	if ( view.m_pPixels == NULL || rSource.m_ChannelBytes[iChannel].is_empty() ) {
		return;
	}
	desc.m_bAuthored = 1u;
	desc.m_nSourceWidth = view.m_nWidth;
	desc.m_nSourceHeight = view.m_nHeight;
	desc.m_nSourceExtent = t3d_vt_godot_source_extent( view );
	desc.m_nSourceFingerprint = t3d_vt_godot_channel_fingerprint( view, rSource.m_ChannelBytes[iChannel], Channel );
	rSource.m_nAuthoredChannelMask |= T3D_VT_LOGICAL_CHANNEL_BIT( Channel );
}

static void t3d_vt_godot_resolve_source_profile( t3dVTGodotSource_t& rSource )
{
	uint32_t nSourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT];
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		nSourceExtent[iChannel] = rSource.m_ChannelDesc[iChannel].m_nSourceExtent;
	}
	rSource.m_eHighestAvailableProfile =
	    t3d_vt_fixed_profile_highest_supported( ( t3dVTConsumer_t )rSource.m_nConsumer, rSource.m_nAuthoredChannelMask, nSourceExtent );
}

bool t3d_vt_godot_extract_instance_source( const Ref<Terrain3DRenderingMaterial>& rMaterial, uint64_t nStableKey,
                                           t3dVTGodotSource_t& rSource )
{
	t3d_vt_godot_clear_source( rSource );
	if ( rMaterial.is_null() ) {
		return false;
	}
	if ( nStableKey == 0u ) {
		nStableKey = rMaterial->getVtStableKey();
	}
	if ( nStableKey == 0u ) {
		return false;
	}
	if ( !t3d_vt_godot_extract_texture( rMaterial->getAlbedoTexture(), Image::FORMAT_RGBA8,
	                                    rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_ALBEDO],
	                                    rSource.m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO] ) ||
	     !t3d_vt_godot_extract_texture( rMaterial->getNormalTexture(), Image::FORMAT_RG8,
	                                    rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_NORMAL],
	                                    rSource.m_Channels[T3D_VT_LOGICAL_CHANNEL_NORMAL] ) ||
	     !t3d_vt_godot_extract_texture( rMaterial->getSurfaceTexture(), Image::FORMAT_RGBA8,
	                                    rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_ORM],
	                                    rSource.m_Channels[T3D_VT_LOGICAL_CHANNEL_ORM] ) ) {
		return false;
	}

	rSource.m_nStableKey = nStableKey;
	rSource.m_nConsumer = ( uint32_t )T3D_VT_CONSUMER_INSTANCE;
	rSource.m_nOrmSourceLayout = ( uint32_t )rMaterial->getSurfaceTextureLayout();
	if ( rSource.m_nOrmSourceLayout > ( uint32_t )SURFACE_TEXTURE_ROUGHNESS_A ) {
		return false;
	}
	if ( rMaterial->usesDirectOrmChannels() && rSource.m_nOrmSourceLayout == ( uint32_t )SURFACE_TEXTURE_ORM ) {
		rSource.m_nSourceFlags |= T3D_VT_GODOT_SOURCE_DIRECT_ORM;
	}
	bool bTextureAlpha = false;
	const PackedByteArray& rAlbedoBytes = rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_ALBEDO];
	if ( !rAlbedoBytes.is_empty() ) {
		// Extraction normalizes albedo to RGBA8; preserve translucent texels when present.
		for ( int64_t i = 3; i < rAlbedoBytes.size(); i += 4 ) {
			if ( rAlbedoBytes[i] != 255u ) {
				bTextureAlpha = true;
				break;
			}
		}
	}
	if ( rMaterial->isAlphaScissor() || rMaterial->getBaseColor().a < 1.0f || bTextureAlpha ) {
		rSource.m_nSourceFlags |= T3D_VT_GODOT_SOURCE_HAS_ALPHA | T3D_VT_GODOT_SOURCE_ALBEDO_RGBA;
	}
	const bool bClamp = rMaterial->getSamplerClass() == ( int32_t )T3D_VT_SAMPLER_LINEAR_CLAMP ||
	                    rMaterial->getSamplerClass() == ( int32_t )T3D_VT_SAMPLER_NEAREST_CLAMP;
	const uint32_t nWrapMode = bClamp ? T3D_VT_COOK_WRAP_CLAMP : T3D_VT_COOK_WRAP_REPEAT;
	t3d_vt_godot_finalize_source_channel( rSource, T3D_VT_LOGICAL_CHANNEL_ALBEDO, T3D_VT_CONSUMER_INSTANCE, nWrapMode,
	                                      rMaterial->isAlphaScissor() );
	t3d_vt_godot_finalize_source_channel( rSource, T3D_VT_LOGICAL_CHANNEL_NORMAL, T3D_VT_CONSUMER_INSTANCE, nWrapMode, false );
	t3d_vt_godot_finalize_source_channel( rSource, T3D_VT_LOGICAL_CHANNEL_ORM, T3D_VT_CONSUMER_INSTANCE, nWrapMode, false );
	// Match the shader's packed cutoff and uniform opacity. The first covered
	// source byte is ceil(cutoff / opacity), rather than a rounded raw cutoff.
	rSource.m_ChannelDesc[T3D_VT_LOGICAL_CHANNEL_ALBEDO].m_nAlphaThreshold =
	    ( uint8_t )CLAMP( ( int32_t )ceilf( ( float )t3dPackUnorm8Nearest( rMaterial->getAlphaScissorThreshold() ) /
		                                    MAX( ( float )rMaterial->getBaseColor().a, 0.0001f ) ),
		                  0, 255 );
	t3d_vt_godot_resolve_source_profile( rSource );
	return true;
}

bool t3d_vt_godot_extract_terrain_source( const Ref<Terrain3DAssets>& rAssets, const uint32_t nLogicalMaterialId, const uint64_t nStableKey,
                                          t3dVTGodotSource_t& rSource )
{
	t3d_vt_godot_clear_source( rSource );
	if ( rAssets.is_null() || !rAssets->usesPackedPbr() || nLogicalMaterialId >= ( uint32_t )rAssets->getTextureCount() ||
	     nLogicalMaterialId >= T3D_MAX_TEXTURE_ASSETS || nStableKey == 0u ) {
		return false;
	}
	const Ref<Terrain3DTextureAsset> asset = rAssets->getTextureAsset( ( int )nLogicalMaterialId );
	if ( asset.is_null() ) {
		return false;
	}
	if ( !t3d_vt_godot_extract_texture( asset->getAlbedoTexture(), Image::FORMAT_RGBA8,
	                                    rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_ALBEDO],
	                                    rSource.m_Channels[T3D_VT_LOGICAL_CHANNEL_ALBEDO] ) ||
	     !t3d_vt_godot_extract_texture( asset->getEffectiveSurfaceTexture(), Image::FORMAT_RGBA8,
	                                    rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_PBR],
	                                    rSource.m_Channels[T3D_VT_LOGICAL_CHANNEL_PBR] ) ||
	     !t3d_vt_godot_extract_texture( asset->getEffectiveDisplacementTexture(), Image::FORMAT_R8,
	                                    rSource.m_ChannelBytes[T3D_VT_LOGICAL_CHANNEL_HEIGHT],
	                                    rSource.m_Channels[T3D_VT_LOGICAL_CHANNEL_HEIGHT] ) ) {
		return false;
	}

	rSource.m_nStableKey = nStableKey;
	rSource.m_nConsumer = ( uint32_t )T3D_VT_CONSUMER_CLIPMAP;
	t3d_vt_godot_finalize_source_channel( rSource, T3D_VT_LOGICAL_CHANNEL_ALBEDO, T3D_VT_CONSUMER_CLIPMAP, T3D_VT_COOK_WRAP_REPEAT, false );
	t3d_vt_godot_finalize_source_channel( rSource, T3D_VT_LOGICAL_CHANNEL_PBR, T3D_VT_CONSUMER_CLIPMAP, T3D_VT_COOK_WRAP_REPEAT, false );
	t3d_vt_godot_finalize_source_channel( rSource, T3D_VT_LOGICAL_CHANNEL_HEIGHT, T3D_VT_CONSUMER_CLIPMAP, T3D_VT_COOK_WRAP_REPEAT, false );
	t3d_vt_godot_resolve_source_profile( rSource );
	return true;
}

bool t3d_vt_godot_make_sparse_cook_source( const t3dVTGodotSource_t& rSource, t3dVTCookSparseSource_t& rCookSource )
{
	memset( &rCookSource, 0, sizeof( rCookSource ) );
	if ( rSource.m_nStableKey == 0u || rSource.m_eHighestAvailableProfile == T3D_VT_FIXED_PROFILE_INVALID ||
	     rSource.m_nConsumer > T3D_VT_CONSUMER_COMPOSED || rSource.m_nOrmSourceLayout > T3D_VT_SURFACE_SOURCE_ROUGHNESS_A ) {
		return false;
	}
	rCookSource.m_nStableKey = rSource.m_nStableKey;
	rCookSource.m_nAuthoredChannelMask = rSource.m_nAuthoredChannelMask;
	rCookSource.m_nOrmSourceLayout = rSource.m_nOrmSourceLayout;
	rCookSource.m_nConsumer = rSource.m_nConsumer;
	rCookSource.m_nAlphaThreshold = rSource.m_ChannelDesc[T3D_VT_LOGICAL_CHANNEL_ALBEDO].m_nAlphaThreshold;
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		rCookSource.m_Channels[iChannel] = rSource.m_Channels[iChannel];
		rCookSource.m_nSourceFingerprint[iChannel] = rSource.m_ChannelDesc[iChannel].m_nSourceFingerprint;
		if ( rSource.m_ChannelDesc[iChannel].m_bAuthored != 0u ) {
			rCookSource.m_nWrapMode = rSource.m_ChannelDesc[iChannel].m_nWrapMode;
			if ( rSource.m_ChannelDesc[iChannel].m_bAlphaScissor != 0u ) {
				rCookSource.m_bAlphaScissor = 1u;
			}
		}
	}
	return true;
}

static Ref<Terrain3DRenderingMaterial> t3d_vt_godot_find_material_by_stable_key( const Ref<Terrain3DAssets>& rAssets,
                                                                                 const uint64_t nStableKey )
{
	if ( rAssets.is_null() || nStableKey == 0u ) {
		return Ref<Terrain3DRenderingMaterial>();
	}
	const int nMeshCount = rAssets->getMeshCount();
	for ( int iMesh = 0; iMesh < nMeshCount; ++iMesh ) {
		const Ref<Terrain3DMeshAsset> asset = rAssets->getMeshAsset( iMesh );
		if ( asset.is_null() ) {
			continue;
		}
		const Ref<Terrain3DRenderingMaterial> overrideMaterial = asset->getRenderingMaterialOverride();
		if ( overrideMaterial.is_valid() && overrideMaterial->getVtStableKey() == nStableKey ) {
			return overrideMaterial;
		}
		const Ref<Terrain3DRenderingMaterial> overlayMaterial = asset->getRenderingMaterialOverlay();
		if ( overlayMaterial.is_valid() && overlayMaterial->getVtStableKey() == nStableKey ) {
			return overlayMaterial;
		}
		const TypedArray<Terrain3DRenderingMaterial> surfaceMaterials = asset->getSurfaceRenderingMaterials();
		for ( int iMaterial = 0; iMaterial < surfaceMaterials.size(); ++iMaterial ) {
			const Ref<Terrain3DRenderingMaterial> material = surfaceMaterials[iMaterial];
			if ( material.is_valid() && material->getVtStableKey() == nStableKey ) {
				return material;
			}
		}
	}
	return Ref<Terrain3DRenderingMaterial>();
}

bool t3d_vt_godot_extract_source_by_stable_key( const Ref<Terrain3DAssets>& rAssets, const uint64_t nStableKey, t3dVTGodotSource_t& rSource,
                                                bool& rAuthored )
{
	rAuthored = false;
	if ( rAssets.is_null() || nStableKey == 0u ) {
		t3d_vt_godot_clear_source( rSource );
		return false;
	}
	uint32_t nLogicalMaterialId = 0u;
	if ( t3d_vt_decode_terrain_material_stable_key( nStableKey, nLogicalMaterialId ) ) {
		rAuthored = true;
		return t3d_vt_godot_extract_terrain_source( rAssets, nLogicalMaterialId, nStableKey, rSource );
	}
	const Ref<Terrain3DRenderingMaterial> material = t3d_vt_godot_find_material_by_stable_key( rAssets, nStableKey );
	if ( material.is_valid() ) {
		rAuthored = true;
		return t3d_vt_godot_extract_instance_source( material, nStableKey, rSource );
	}
	t3d_vt_godot_clear_source( rSource );
	rSource.m_nStableKey = nStableKey;
	return true;
}
struct t3dVTEditorAllocationBudget_t
{
	uint64_t m_nLimit = T3D_VT_COOK_WORKING_BYTE_LIMIT;
	uint64_t m_nLiveBytes = 0u;
};
struct t3dVTEditorAllocationHeader_t
{
	void *m_pBase;
	uint64_t m_nChargedBytes;
};
static void *t3d_vt_godot_editor_allocate( void *pUser, const uint64_t nBytes, const uint32_t nAlignment )
{
	if ( nBytes == 0u || nBytes > SIZE_MAX || nAlignment == 0u || ( nAlignment & ( nAlignment - 1u ) ) != 0u ) {
		return NULL;
	}
	const uint32_t nEffectiveAlignment = MAX( nAlignment, ( uint32_t )alignof( t3dVTEditorAllocationHeader_t ) );
	const uint64_t nExtra = ( uint64_t )nEffectiveAlignment - 1u + sizeof( t3dVTEditorAllocationHeader_t );
	if ( nBytes > SIZE_MAX - nExtra ) {
		return NULL;
	}
	t3dVTEditorAllocationBudget_t *const pBudget = ( t3dVTEditorAllocationBudget_t * )pUser;
	if ( pBudget != NULL && ( pBudget->m_nLiveBytes > pBudget->m_nLimit || nBytes + nExtra > pBudget->m_nLimit - pBudget->m_nLiveBytes ) ) {
		return NULL;
	}
	void *const pBase = t3dMemoryAllocate( ( size_t )( nBytes + nExtra ) );
	if ( pBase == NULL ) {
		return NULL;
	}
	const uintptr_t nStart = ( uintptr_t )pBase + sizeof( t3dVTEditorAllocationHeader_t );
	const uintptr_t nAligned = ( nStart + nEffectiveAlignment - 1u ) & ~( ( uintptr_t )nEffectiveAlignment - 1u );
	t3dVTEditorAllocationHeader_t& header = ( ( t3dVTEditorAllocationHeader_t * )nAligned )[-1];
	header.m_pBase = pBase;
	header.m_nChargedBytes = nBytes + nExtra;
	if ( pBudget != NULL ) {
		pBudget->m_nLiveBytes += header.m_nChargedBytes;
	}
	return ( void * )nAligned;
}

static void t3d_vt_godot_editor_free( void *pUser, void *pMemory )
{
	if ( pMemory != NULL ) {
		const t3dVTEditorAllocationHeader_t& header = ( ( t3dVTEditorAllocationHeader_t * )pMemory )[-1];
		t3dVTEditorAllocationBudget_t *const pBudget = ( t3dVTEditorAllocationBudget_t * )pUser;
		if ( pBudget != NULL ) {
			pBudget->m_nLiveBytes -= header.m_nChargedBytes;
		}
		t3dMemoryRelease( header.m_pBase );
	}
}

#define T3D_VT_EDITOR_CHANNEL_CACHE_MAGIC 0x38484356u
#define T3D_VT_EDITOR_CHANNEL_CACHE_VERSION 1u

// t3dVTEditorChannelCacheDiskHeader_s centralized in Terrain3DTypes.h.

typedef struct t3dVTEditorFragment_s
{
	uint64_t m_nStableKey;
	uint64_t m_nKey;
	PackedByteArray m_Bytes;
} t3dVTEditorFragment_t;

typedef struct t3dVTEditorUniquePayload_s
{
	t3dVTPackPayloadRecord_t m_Record;
	// The immutable input fragments outlive the merge. Borrow their payloads
	// until concatenation instead of allocating another complete byte copy.
	const uint8_t *m_pBytes = NULL;
	uint32_t m_nBytes = 0u;
} t3dVTEditorUniquePayload_t;

// t3dVTEditorTimedEncoder_s centralized in Terrain3DTypes.h.

static uint64_t t3d_vt_godot_now_usec( void )
{
	Time *const pTime = Time::get_singleton();
	return pTime ? pTime->get_ticks_usec() : 0u;
}

static String t3d_vt_godot_cache_key_text( const uint64_t nKey )
{
	return String::num_uint64( nKey, 16 );
}

static String t3d_vt_godot_channel_cache_path( const t3dVTEditorCookCache_t& rCache, const uint64_t nKey )
{
	return rCache.m_CacheDirectory.is_empty()
	           ? String()
			   : rCache.m_CacheDirectory.path_join( String( "channel_" ) + t3d_vt_godot_cache_key_text( nKey ) + ".bin" );
}

static String t3d_vt_godot_allocation_cache_path( const t3dVTEditorCookCache_t& rCache, const uint64_t nKey )
{
	return rCache.m_CacheDirectory.is_empty()
	           ? String()
			   : rCache.m_CacheDirectory.path_join( String( "allocation_" ) + t3d_vt_godot_cache_key_text( nKey ) + ".t3dvt" );
}

static bool t3d_vt_godot_write_cache_atomic( const String& rPath, const PackedByteArray& rBytes )
{
	if ( rPath.is_empty() || rBytes.is_empty() ) {
		return false;
	}
	Terrain3DFilePublication publication( rPath, rPath + String( ".tmp" ), rPath + String( ".bak" ) );
	return publication.stagePacked( rBytes ) == OK && publication.publish() == OK && publication.commit() == OK;
}

static bool t3d_vt_godot_timed_encode_tile( void *pUser, const t3dVTEncodeTileRequest_t& rRequest, uint32_t& rWrittenBytes )
{
	t3dVTEditorTimedEncoder_t *const pTimed = ( t3dVTEditorTimedEncoder_t * )pUser;
	if ( pTimed == NULL || pTimed->m_Base.m_pEncodeTile == NULL ) {
		return false;
	}
	const uint64_t nStart = t3d_vt_godot_now_usec();
	const bool bOk = pTimed->m_Base.m_pEncodeTile( pTimed->m_Base.m_pUser, rRequest, rWrittenBytes );
	const uint64_t nEnd = t3d_vt_godot_now_usec();
	if ( nEnd >= nStart ) {
		pTimed->m_nEncodeUsec += nEnd - nStart;
	}
	return bOk;
}

void t3d_vt_godot_editor_cache_reset( t3dVTEditorCookCache_t& rCache )
{
	rCache.m_ChannelEntries.clear();
	rCache.m_AllocationEntries.clear();
	rCache.m_nLifetimeHits = 0u;
	rCache.m_nLifetimeMisses = 0u;
}

void t3d_vt_godot_editor_cache_set_directory( t3dVTEditorCookCache_t& rCache, const String& rDirectory )
{
	rCache.m_CacheDirectory = rDirectory;
	if ( !rDirectory.is_empty() ) {
		DirAccess::make_dir_recursive_absolute( rDirectory );
	}
}

static t3dVTEditorChannelCacheEntry_t *t3d_vt_godot_find_channel_cache( t3dVTEditorCookCache_t& rCache, const uint64_t nKey,
                                                                        const uint32_t nChannel, const uint32_t nTargetExtent )
{
	for ( size_t iEntry = 0u; iEntry < rCache.m_ChannelEntries.size(); ++iEntry ) {
		t3dVTEditorChannelCacheEntry_t& rEntry = rCache.m_ChannelEntries[iEntry];
		if ( rEntry.m_nKey == nKey && rEntry.m_nLogicalChannel == nChannel && rEntry.m_nTargetExtent == nTargetExtent &&
		     !rEntry.m_Bytes.is_empty() ) {
			return &rEntry;
		}
	}
	return NULL;
}

static bool t3d_vt_godot_load_channel_cache_disk( t3dVTEditorCookCache_t& rCache, const uint64_t nKey, const uint32_t nChannel,
                                                  const uint32_t nTargetExtent, t3dVTEditorChannelCacheEntry_t *& rpEntry )
{
	rpEntry = NULL;
	const String path = t3d_vt_godot_channel_cache_path( rCache, nKey );
	if ( path.is_empty() || !FileAccess::file_exists( path ) ) {
		return false;
	}
	const Ref<FileAccess> cache_file = FileAccess::open( path, FileAccess::READ );
	Terrain3DMemoryReservation file_memory;
	if ( cache_file.is_null() || !file_memory.resize( cache_file->get_length() ) ) {
		return false;
	}
	const PackedByteArray fileBytes = cache_file->get_buffer( cache_file->get_length() );
	if ( fileBytes.size() < ( int64_t )sizeof( t3dVTEditorChannelCacheDiskHeader_t ) ) {
		return false;
	}
	t3dVTEditorChannelCacheDiskHeader_t header;
	memcpy( &header, fileBytes.ptr(), sizeof( header ) );
	if ( header.m_nMagic != T3D_VT_EDITOR_CHANNEL_CACHE_MAGIC || header.m_nVersion != T3D_VT_EDITOR_CHANNEL_CACHE_VERSION ||
	     header.m_nKey != nKey || header.m_nLogicalChannel != nChannel || header.m_nTargetExtent != nTargetExtent ||
	     header.m_nPayloadBytes == 0u || header.m_nPayloadBytes > INT64_MAX ||
	     ( uint64_t )fileBytes.size() != sizeof( header ) + header.m_nPayloadBytes ||
	     header.m_Layout.m_nTotalBytes != header.m_nPayloadBytes || header.m_Layout.m_nMipCount == 0u ||
	     header.m_Layout.m_nMipCount > T3D_VT_MAX_MIPS ) {
		return false;
	}
	const uint8_t *const pPayload = fileBytes.ptr() + sizeof( header );
	if ( t3d_vt_pack_checksum32( pPayload, header.m_nPayloadBytes ) != header.m_nPayloadChecksum ) {
		return false;
	}
	t3dVTEditorChannelCacheEntry_t entry;
	entry.m_nKey = nKey;
	entry.m_nLogicalChannel = nChannel;
	entry.m_nTargetExtent = nTargetExtent;
	entry.m_Layout = header.m_Layout;
	if ( !entry.m_memory.reserve( header.m_nPayloadBytes ) ) {
		return false;
	}
	entry.m_Bytes.resize( ( int64_t )header.m_nPayloadBytes );
	if ( entry.m_Bytes.size() != ( int64_t )header.m_nPayloadBytes ) {
		return false;
	}
	memcpy( entry.m_Bytes.ptrw(), pPayload, ( size_t )header.m_nPayloadBytes );
	if ( !t3dContainerReserveCold( rCache.m_ChannelEntries, rCache.m_ChannelEntries.size() + 1u ) ) {
		return false;
	}
	rCache.m_ChannelEntries.push_back( entry );
	rpEntry = &rCache.m_ChannelEntries.back();
	return true;
}

static void t3d_vt_godot_store_channel_cache_disk( const t3dVTEditorCookCache_t& rCache, const t3dVTEditorChannelCacheEntry_t& rEntry )
{
	const String path = t3d_vt_godot_channel_cache_path( rCache, rEntry.m_nKey );
	if ( path.is_empty() || rEntry.m_Bytes.is_empty() ) {
		return;
	}
	t3dVTEditorChannelCacheDiskHeader_t header;
	memset( &header, 0, sizeof( header ) );
	header.m_nMagic = T3D_VT_EDITOR_CHANNEL_CACHE_MAGIC;
	header.m_nVersion = T3D_VT_EDITOR_CHANNEL_CACHE_VERSION;
	header.m_nLogicalChannel = rEntry.m_nLogicalChannel;
	header.m_nTargetExtent = rEntry.m_nTargetExtent;
	header.m_nKey = rEntry.m_nKey;
	header.m_Layout = rEntry.m_Layout;
	header.m_nPayloadBytes = ( uint64_t )rEntry.m_Bytes.size();
	header.m_nPayloadChecksum = t3d_vt_pack_checksum32( rEntry.m_Bytes.ptr(), header.m_nPayloadBytes );
	PackedByteArray fileBytes;
	Terrain3DMemoryReservation file_memory;
	if ( !file_memory.resize( sizeof( header ) + header.m_nPayloadBytes ) ) {
		return;
	}
	fileBytes.resize( ( int64_t )( sizeof( header ) + header.m_nPayloadBytes ) );
	if ( fileBytes.size() != ( int64_t )( sizeof( header ) + header.m_nPayloadBytes ) ) {
		return;
	}
	memcpy( fileBytes.ptrw(), &header, sizeof( header ) );
	memcpy( fileBytes.ptrw() + sizeof( header ), rEntry.m_Bytes.ptr(), ( size_t )header.m_nPayloadBytes );
	( void )t3d_vt_godot_write_cache_atomic( path, fileBytes );
}

static t3dVTEditorAllocationCacheEntry_t *t3d_vt_godot_find_allocation_cache( t3dVTEditorCookCache_t& rCache, const uint64_t nStableKey,
                                                                              const uint64_t nKey )
{
	for ( size_t iEntry = 0u; iEntry < rCache.m_AllocationEntries.size(); ++iEntry ) {
		t3dVTEditorAllocationCacheEntry_t& rEntry = rCache.m_AllocationEntries[iEntry];
		if ( rEntry.m_nStableKey == nStableKey && rEntry.m_nKey == nKey && !rEntry.m_PackageBytes.is_empty() ) {
			return &rEntry;
		}
	}
	return NULL;
}

static bool t3d_vt_godot_validate_allocation_fragment( const PackedByteArray& rBytes, const uint64_t nStableKey )
{
	if ( rBytes.is_empty() ) {
		return false;
	}
	t3dVTPackBaselineView_t view;
	return t3d_vt_pack_open_baseline_view( rBytes.ptr(), ( uint64_t )rBytes.size(), view ) && view.m_pHeader->m_nAllocationCount == 1u &&
	       view.m_pAllocationKeys[0].m_nStableKey == nStableKey;
}

static bool t3d_vt_godot_load_allocation_cache_disk( t3dVTEditorCookCache_t& rCache, const uint64_t nStableKey, const uint64_t nKey,
                                                     t3dVTEditorAllocationCacheEntry_t *& rpEntry )
{
	rpEntry = NULL;
	const String path = t3d_vt_godot_allocation_cache_path( rCache, nKey );
	if ( path.is_empty() || !FileAccess::file_exists( path ) ) {
		return false;
	}
	const Ref<FileAccess> cache_file = FileAccess::open( path, FileAccess::READ );
	Terrain3DSharedMemoryReservation file_memory;
	if ( cache_file.is_null() || !file_memory.reserve( cache_file->get_length() ) ) {
		return false;
	}
	const PackedByteArray bytes = cache_file->get_buffer( cache_file->get_length() );
	if ( !t3d_vt_godot_validate_allocation_fragment( bytes, nStableKey ) ) {
		return false;
	}
	t3dVTEditorAllocationCacheEntry_t entry;
	entry.m_nStableKey = nStableKey;
	entry.m_nKey = nKey;
	entry.m_PackageBytes = bytes;
	entry.m_memory = file_memory;
	if ( !t3dContainerReserveCold( rCache.m_AllocationEntries, rCache.m_AllocationEntries.size() + 1u ) ) {
		return false;
	}
	rCache.m_AllocationEntries.push_back( entry );
	rpEntry = &rCache.m_AllocationEntries.back();
	return true;
}

static void t3d_vt_godot_store_allocation_cache_disk( const t3dVTEditorCookCache_t& rCache,
                                                      const t3dVTEditorAllocationCacheEntry_t& rEntry )
{
	const String path = t3d_vt_godot_allocation_cache_path( rCache, rEntry.m_nKey );
	if ( !path.is_empty() && !rEntry.m_PackageBytes.is_empty() ) {
		( void )t3d_vt_godot_write_cache_atomic( path, rEntry.m_PackageBytes );
	}
}

static uint64_t t3d_vt_godot_channel_cache_key( const t3dVTEditorCookCache_t& rCache, const t3dVTGodotSource_t& rSource,
                                                const t3dVTLogicalChannel_t Channel, const uint32_t nTargetExtent,
                                                const uint32_t nCompressionQuality, const uint32_t nMipQuality )
{
	const t3dVTGodotSourceChannel_t& rDesc = rSource.m_ChannelDesc[( uint32_t )Channel];
	uint64_t nHash = 1469598103934665603ULL;
	nHash = t3d_vt_godot_hash_u64( nHash, rDesc.m_nSourceFingerprint );
	// ORM channel interpretation changes the filtered pixels even when source bytes do not.
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nOrmSourceLayout );
	nHash = t3d_vt_godot_hash_u64( nHash, ( uint32_t )Channel );
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nConsumer );
	nHash = t3d_vt_godot_hash_u64( nHash, nTargetExtent );
	nHash = t3d_vt_godot_hash_u64( nHash, rDesc.m_nWrapMode );
	nHash = t3d_vt_godot_hash_u64( nHash, rDesc.m_bAlphaScissor );
	nHash = t3d_vt_godot_hash_u64( nHash, rDesc.m_nAlphaThreshold );
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nSourceFlags );
	nHash = t3d_vt_godot_hash_u64( nHash, T3D_VT_PAGE_INTERIOR );
	nHash = t3d_vt_godot_hash_u64( nHash, T3D_VT_PAGE_GUTTER );
	nHash = t3d_vt_godot_hash_u64( nHash, rCache.m_nFilterAbi );
	nHash = t3d_vt_godot_hash_u64( nHash, rCache.m_nCompressorAbi );
	nHash = t3d_vt_godot_hash_u64( nHash, nCompressionQuality );
	nHash = t3d_vt_godot_hash_u64( nHash, nMipQuality );
	return nHash == 0u ? 1u : nHash;
}

static uint64_t t3d_vt_godot_allocation_cache_key( const t3dVTGodotSource_t& rSource,
                                                   const uint64_t nChannelKeys[T3D_VT_LOGICAL_CHANNEL_COUNT],
                                                   const t3dVTCookerSettings_t& rSettings, const uint32_t nAllocationFlags )
{
	uint64_t nHash = 1469598103934665603ULL;
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nStableKey );
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nAuthoredChannelMask );
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nConsumer );
	nHash = t3d_vt_godot_hash_u64( nHash, ( uint32_t )rSource.m_eHighestAvailableProfile );
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nSourceFlags );
	nHash = t3d_vt_godot_hash_u64( nHash, rSource.m_nOrmSourceLayout );
	nHash = t3d_vt_godot_hash_u64( nHash, rSettings.m_nMipQuality );
	nHash = t3d_vt_godot_hash_u64( nHash, rSettings.m_nCompressionQuality );
	nHash = t3d_vt_godot_hash_u64( nHash, rSettings.m_nOuterCompression );
	nHash = t3d_vt_godot_hash_u64( nHash, nAllocationFlags );
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		nHash = t3d_vt_godot_hash_u64( nHash, nChannelKeys[iChannel] );
	}
	return nHash == 0u ? 1u : nHash;
}

static bool t3d_vt_godot_prepare_channel_cache( t3dVTEditorCookCache_t& rCache, const t3dVTGodotSource_t& rSource,
                                                t3dVTCookSparseSource_t& rCookSource, const t3dVTLogicalChannel_t Channel,
                                                const t3dVTCookerSettings_t& rSettings, const t3dVTCookAllocator_t& rAllocator,
                                                t3dVTEditorCookTelemetry_t& rTelemetry, uint64_t& rChannelKey, const bool bForceRebuild )
{
	const uint32_t nTargetExtent = t3d_vt_fixed_profile_channel_extent( rSource.m_eHighestAvailableProfile, Channel );
	if ( nTargetExtent == 0u || ( rSource.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( Channel ) ) == 0u ) {
		rChannelKey = 0u;
		return true;
	}
	rChannelKey =
	    t3d_vt_godot_channel_cache_key( rCache, rSource, Channel, nTargetExtent, rSettings.m_nCompressionQuality, rSettings.m_nMipQuality );
	t3dVTEditorChannelCacheEntry_t *pEntry =
	    bForceRebuild ? NULL : t3d_vt_godot_find_channel_cache( rCache, rChannelKey, ( uint32_t )Channel, nTargetExtent );
	if ( !bForceRebuild && pEntry == NULL ) {
		( void )t3d_vt_godot_load_channel_cache_disk( rCache, rChannelKey, ( uint32_t )Channel, nTargetExtent, pEntry );
	}
	if ( pEntry != NULL ) {
		++rTelemetry.m_nChannelCacheHits;
		++rCache.m_nLifetimeHits;
	} else {
		++rTelemetry.m_nChannelCacheMisses;
		++rCache.m_nLifetimeMisses;
		rTelemetry.m_nStage = T3D_VT_EDITOR_COOK_STAGE_FILTER;
		const uint64_t nStart = t3d_vt_godot_now_usec();
		uint8_t *pMipBytes = NULL;
		t3dVTMipChainLayout_t layout;
		if ( !t3d_vt_cooker_build_channel_pyramid( rAllocator, rCookSource, rSource.m_eHighestAvailableProfile, Channel, pMipBytes, layout,
		                                           rSettings.m_nMipQuality ) ||
		     pMipBytes == NULL || layout.m_nTotalBytes == 0u || layout.m_nTotalBytes > INT64_MAX ) {
			if ( pMipBytes != NULL ) {
				rAllocator.m_pFree( rAllocator.m_pUser, pMipBytes );
			}
			return false;
		}
		const uint64_t nEnd = t3d_vt_godot_now_usec();
		if ( nEnd >= nStart ) {
			rTelemetry.m_nFilterUsec += nEnd - nStart;
		}
		t3dVTEditorChannelCacheEntry_t entry;
		entry.m_nKey = rChannelKey;
		entry.m_nTargetExtent = nTargetExtent;
		entry.m_nLogicalChannel = ( uint32_t )Channel;
		entry.m_Layout = layout;
		if ( !entry.m_memory.reserve( layout.m_nTotalBytes ) ) {
			rAllocator.m_pFree( rAllocator.m_pUser, pMipBytes );
			return false;
		}
		entry.m_Bytes.resize( ( int64_t )layout.m_nTotalBytes );
		if ( entry.m_Bytes.size() != ( int64_t )layout.m_nTotalBytes ) {
			rAllocator.m_pFree( rAllocator.m_pUser, pMipBytes );
			return false;
		}
		memcpy( entry.m_Bytes.ptrw(), pMipBytes, ( size_t )layout.m_nTotalBytes );
		rAllocator.m_pFree( rAllocator.m_pUser, pMipBytes );
		if ( !t3dContainerReserveCold( rCache.m_ChannelEntries, rCache.m_ChannelEntries.size() + 1u ) ) {
			return false;
		}
		rCache.m_ChannelEntries.push_back( entry );
		pEntry = &rCache.m_ChannelEntries.back();
		t3d_vt_godot_store_channel_cache_disk( rCache, *pEntry );
	}
	rCookSource.m_PreparedChannels[( uint32_t )Channel].m_pMipBytes = pEntry->m_Bytes.ptr();
	rCookSource.m_PreparedChannels[( uint32_t )Channel].m_Layout = pEntry->m_Layout;
	rCookSource.m_PreparedChannels[( uint32_t )Channel].m_nTargetExtent = pEntry->m_nTargetExtent;
	return true;
}

static bool t3d_vt_godot_cook_allocation_fragment( const Ref<Terrain3DAssets>& rAssets, const uint64_t nStableKey,
                                                   t3dVTEditorCookCache_t& rCache, const t3dVTCookerSettings_t& rSettings,
                                                   const t3dVTCookAllocator_t& rAllocator, t3dVTEditorCookTelemetry_t& rTelemetry,
                                                   const bool bShippingImmutable, PackedByteArray& rFragmentBytes, uint64_t& rAllocationKey,
                                                   String *const pError, const bool bForceRebuild = false )
{
	rFragmentBytes.clear();
	rAllocationKey = 0u;
	rTelemetry.m_nStage = T3D_VT_EDITOR_COOK_STAGE_EXTRACT;
	const uint64_t nExtractStart = t3d_vt_godot_now_usec();
	t3dVTGodotSource_t source;
	bool bAuthored = false;
	if ( !t3d_vt_godot_extract_source_by_stable_key( rAssets, nStableKey, source, bAuthored ) || !bAuthored ||
	     source.m_eHighestAvailableProfile == T3D_VT_FIXED_PROFILE_INVALID ) {
		if ( pError ) {
			*pError = "VT V1 cooker could not extract an authored source";
		}
		return false;
	}
	const uint64_t nExtractEnd = t3d_vt_godot_now_usec();
	if ( nExtractEnd >= nExtractStart ) {
		rTelemetry.m_nExtractUsec += nExtractEnd - nExtractStart;
	}
	t3dVTCookSparseSource_t cookSource;
	if ( !t3d_vt_godot_make_sparse_cook_source( source, cookSource ) ) {
		return false;
	}
	uint32_t nAllocationFlags = T3D_VT_ALLOCATION_MATERIAL_UV;
	if ( bShippingImmutable ) {
		nAllocationFlags |= T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
		nAllocationFlags |= cookSource.m_nWrapMode == T3D_VT_COOK_WRAP_CLAMP ? T3D_VT_ALLOCATION_CLAMP : T3D_VT_ALLOCATION_REPEAT;
		if ( ( source.m_nSourceFlags & T3D_VT_GODOT_SOURCE_HAS_ALPHA ) != 0u ) {
			nAllocationFlags |= T3D_VT_ALLOCATION_HAS_ALPHA;
		}
		if ( ( source.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_HEIGHT ) ) != 0u ) {
			nAllocationFlags |= T3D_VT_ALLOCATION_HAS_HEIGHT;
		}
	} else {
		nAllocationFlags |= ( cookSource.m_nWrapMode == T3D_VT_COOK_WRAP_CLAMP ? T3D_VT_ALLOCATION_CLAMP : T3D_VT_ALLOCATION_REPEAT ) |
		                    T3D_VT_ALLOCATION_EDITOR_DYNAMIC;
	}
	if ( ( source.m_nSourceFlags & T3D_VT_GODOT_SOURCE_ALBEDO_RGBA ) != 0u ) {
		nAllocationFlags |= T3D_VT_ALLOCATION_ALBEDO_RGBA;
	}
	cookSource.m_nAllocationFlags = ( uint16_t )nAllocationFlags;
	uint64_t channelKeys[T3D_VT_LOGICAL_CHANNEL_COUNT];
	memset( channelKeys, 0, sizeof( channelKeys ) );
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		const t3dVTLogicalChannel_t Channel = ( t3dVTLogicalChannel_t )iChannel;
		const uint32_t nTargetExtent = t3d_vt_fixed_profile_channel_extent( source.m_eHighestAvailableProfile, Channel );
		if ( nTargetExtent != 0u && ( source.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( Channel ) ) != 0u ) {
			channelKeys[iChannel] = t3d_vt_godot_channel_cache_key( rCache, source, Channel, nTargetExtent, rSettings.m_nCompressionQuality,
			                                                        rSettings.m_nMipQuality );
		}
	}
	rAllocationKey = t3d_vt_godot_allocation_cache_key( source, channelKeys, rSettings, nAllocationFlags );
	cookSource.m_nGenerationSeed = ( uint32_t )( rAllocationKey ^ ( rAllocationKey >> 32u ) );
	if ( cookSource.m_nGenerationSeed == 0u ) {
		cookSource.m_nGenerationSeed = 1u;
	}
	t3dVTEditorAllocationCacheEntry_t *pAllocationCache =
	    bForceRebuild ? NULL : t3d_vt_godot_find_allocation_cache( rCache, nStableKey, rAllocationKey );
	if ( !bForceRebuild && pAllocationCache == NULL ) {
		( void )t3d_vt_godot_load_allocation_cache_disk( rCache, nStableKey, rAllocationKey, pAllocationCache );
	}
	if ( pAllocationCache != NULL ) {
		++rTelemetry.m_nAllocationCacheHits;
		++rCache.m_nLifetimeHits;
		rFragmentBytes = pAllocationCache->m_PackageBytes;
		return true;
	}
	++rTelemetry.m_nAllocationCacheMisses;
	++rTelemetry.m_nDirtyAllocations;
	++rCache.m_nLifetimeMisses;
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		uint64_t nPreparedKey = 0u;
		if ( !t3d_vt_godot_prepare_channel_cache( rCache, source, cookSource, ( t3dVTLogicalChannel_t )iChannel, rSettings, rAllocator,
		                                          rTelemetry, nPreparedKey, bForceRebuild ) ||
		     nPreparedKey != channelKeys[iChannel] ) {
			rCache.m_ChannelEntries.clear();
			if ( pError ) {
				*pError = "VT V1 cooker could not prepare a channel pyramid";
			}
			return false;
		}
	}
	t3dVTEditorTimedEncoder_t timedEncoder;
	memset( &timedEncoder, 0, sizeof( timedEncoder ) );
	t3d_vt_godot_make_encoder( timedEncoder.m_Base );
	t3dVTEncoderInterface_t encoder;
	encoder.m_pUser = &timedEncoder;
	encoder.m_pEncodeTile = &t3d_vt_godot_timed_encode_tile;
	t3dVTCookedSparsePackage_t package;
	rTelemetry.m_nStage = T3D_VT_EDITOR_COOK_STAGE_PAGE;
	const uint64_t nCookStart = t3d_vt_godot_now_usec();
	const bool bCooked = t3d_vt_cooker_cook_sparse_pages( rSettings, &cookSource, 1u, encoder, rAllocator, NULL, package );
	// Prepared pyramids are borrowed only by the sparse cook. Encoded fragments
	// and the disk cache retain reusable results; keeping raw pyramids for every
	// material consumes the headroom needed to serialize and merge the package.
	rCache.m_ChannelEntries.clear();
	const uint64_t nCookEnd = t3d_vt_godot_now_usec();
	rTelemetry.m_nEncodeUsec += timedEncoder.m_nEncodeUsec;
	if ( nCookEnd >= nCookStart ) {
		const uint64_t nTotal = nCookEnd - nCookStart;
		rTelemetry.m_nPageUsec += nTotal > timedEncoder.m_nEncodeUsec ? nTotal - timedEncoder.m_nEncodeUsec : 0u;
	}
	if ( !bCooked ) {
		if ( pError ) {
			*pError = "VT V1 cooker failed to cook an allocation";
		}
		return false;
	}
	t3dVTSerializedPackSource_t packSource;
	if ( !t3d_vt_cooker_build_pack_source( package, packSource ) ) {
		t3d_vt_cooker_release_sparse_package( package );
		return false;
	}
	const uint64_t nBytes = t3d_vt_pack_compute_bytes( packSource );
	if ( nBytes == 0u || nBytes > INT64_MAX ) {
		t3d_vt_cooker_release_sparse_package( package );
		return false;
	}
	Terrain3DSharedMemoryReservation fragment_memory;
	if ( !fragment_memory.reserve( nBytes ) ) {
		t3d_vt_cooker_release_sparse_package( package );
		return false;
	}
	rFragmentBytes.resize( ( int64_t )nBytes );
	uint64_t nWritten = 0u;
	const bool bWritten = rFragmentBytes.size() == ( int64_t )nBytes &&
	                      t3d_vt_pack_write( packSource, rFragmentBytes.ptrw(), nBytes, nWritten ) && nWritten == nBytes;
	t3d_vt_cooker_release_sparse_package( package );
	if ( !bWritten || !t3d_vt_godot_validate_allocation_fragment( rFragmentBytes, nStableKey ) ) {
		rFragmentBytes.clear();
		return false;
	}
	t3dVTEditorAllocationCacheEntry_t entry;
	entry.m_nStableKey = nStableKey;
	entry.m_nKey = rAllocationKey;
	entry.m_PackageBytes = rFragmentBytes;
	entry.m_memory = fragment_memory;
	entry.m_bPendingDiskWrite = true;
	for ( size_t iEntry = 0u; iEntry < rCache.m_AllocationEntries.size(); ++iEntry ) {
		if ( rCache.m_AllocationEntries[iEntry].m_nStableKey == nStableKey ) {
			rCache.m_AllocationEntries[iEntry] = entry;
			return true;
		}
	}
	if ( !t3dContainerReserveCold( rCache.m_AllocationEntries, rCache.m_AllocationEntries.size() + 1u ) ) {
		return false;
	}
	rCache.m_AllocationEntries.push_back( entry );
	return true;
}

static void t3d_vt_godot_sort_fragments( eastl::vector<t3dVTEditorFragment_t, Terrain3DEastlAllocator>& rFragments )
{
	// Stable equal-key order is part of the existing cooked package layout.
	eastl::stable_sort( rFragments.begin(), rFragments.end(),
	                    []( const t3dVTEditorFragment_t& a, const t3dVTEditorFragment_t& b ) { return a.m_nStableKey < b.m_nStableKey; } );
}

static bool t3d_vt_godot_payload_bytes( const t3dVTPackBaselineView_t& rView, const uint32_t nPayloadId, const uint8_t *& rpBytes,
                                        uint32_t& rBytes )
{
	rpBytes = NULL;
	rBytes = 0u;
	if ( nPayloadId >= rView.m_pHeader->m_nPayloadCount ) {
		return false;
	}
	const t3dVTPackPayloadRecord_t& rPayload = rView.m_pPayloads[nPayloadId];
	if ( rPayload.m_nClusterId >= rView.m_pHeader->m_nClusterCount || rPayload.m_nCodec != T3D_VT_PACK_CODEC_NONE ||
	     rPayload.m_nStoredBytes != rPayload.m_nDecodedBytes ) {
		return false;
	}
	const t3dVTPackClusterRecord_t& rCluster = rView.m_pClusters[rPayload.m_nClusterId];
	if ( rCluster.m_nCodec != T3D_VT_PACK_CODEC_NONE || rPayload.m_nClusterOffset > rCluster.m_nDecodedBytes ||
	     rPayload.m_nDecodedBytes > rCluster.m_nDecodedBytes - rPayload.m_nClusterOffset ) {
		return false;
	}
	rpBytes = rView.m_pPayloadData + rCluster.m_nPayloadDataOffset + rPayload.m_nClusterOffset;
	rBytes = rPayload.m_nDecodedBytes;
	return true;
}

struct t3dVTEditorPayloadIndex_t
{
	eastl::vector<uint32_t, Terrain3DEastlAllocator> m_heads;
	eastl::vector<uint32_t, Terrain3DEastlAllocator> m_next;
	uint64_t m_nComparisons = 0u;
	bool prepare( const uint32_t nCount )
	{
		uint64_t nBuckets = 1u;
		while ( nBuckets < ( uint64_t )nCount * 2u ) {
			nBuckets *= 2u;
		}
		return t3dVectorResizeCold( m_heads, ( size_t )nBuckets, UINT32_MAX ) && t3dContainerReserveCold( m_next, nCount );
	}
};

static uint32_t t3d_vt_godot_find_or_add_payload( eastl::vector<t3dVTEditorUniquePayload_t, Terrain3DEastlAllocator>& rPayloads,
                                                  t3dVTEditorPayloadIndex_t& rIndex, const t3dVTPackPayloadRecord_t& rPayload,
                                                  const uint8_t *const pBytes, const uint32_t nBytes )
{
	// Hash actual bytes, not just the supplied signature. Equality still checks
	// metadata and bytes so a hash collision never merges distinct payloads.
	uint64_t nHash = t3d_vt_pack_hash64( pBytes, nBytes ) * 1099511628211ull + rPayload.m_nContentSignature;
	nHash ^= ( uint64_t )rPayload.m_nFormat << 32u;
	nHash ^= rPayload.m_nFlags;
	nHash ^= nHash >> 30u;
	nHash *= 0xbf58476d1ce4e5b9ull;
	nHash ^= nHash >> 27u;
	nHash *= 0x94d049bb133111ebull;
	nHash ^= nHash >> 31u;
	const size_t nBucket = ( size_t )( nHash & ( rIndex.m_heads.size() - 1u ) );
	for ( uint32_t iPayload = rIndex.m_heads[nBucket]; iPayload != UINT32_MAX; iPayload = rIndex.m_next[iPayload] ) {
		++rIndex.m_nComparisons;
		const t3dVTEditorUniquePayload_t& rExisting = rPayloads[iPayload];
		if ( rExisting.m_Record.m_nContentSignature == rPayload.m_nContentSignature && rExisting.m_Record.m_nFormat == rPayload.m_nFormat &&
		     rExisting.m_Record.m_nFlags == rPayload.m_nFlags && rExisting.m_nBytes == nBytes &&
		     memcmp( rExisting.m_pBytes, pBytes, nBytes ) == 0 ) {
			return iPayload;
		}
	}
	if ( rPayloads.size() == rPayloads.capacity() || rIndex.m_next.size() == rIndex.m_next.capacity() ) {
		return UINT32_MAX;
	}
	t3dVTEditorUniquePayload_t entry;
	entry.m_Record = rPayload;
	entry.m_Record.m_nClusterId = 0u;
	entry.m_Record.m_nClusterOffset = 0u;
	entry.m_Record.m_nCodec = T3D_VT_PACK_CODEC_NONE;
	entry.m_Record.m_nStoredBytes = nBytes;
	entry.m_Record.m_nDecodedBytes = nBytes;
	entry.m_pBytes = pBytes;
	entry.m_nBytes = nBytes;
	rPayloads.push_back( entry );
	const uint32_t nId = ( uint32_t )rPayloads.size() - 1u;
	rIndex.m_next.push_back( rIndex.m_heads[nBucket] );
	rIndex.m_heads[nBucket] = nId;
	return nId;
}

struct t3dVTEditorAllocationInput_t
{
	uint32_t nFragment;
	uint32_t nAllocation;
	uint64_t nStableKey;
};

static bool t3dVTAllocationInputLess( const t3dVTEditorAllocationInput_t& a, const t3dVTEditorAllocationInput_t& b )
{
	return a.nStableKey < b.nStableKey;
}

struct t3dVTEditorMergePlan_t
{
	uint64_t m_nAllocations = 0u, m_nPages = 0u, m_nTails = 0u, m_nViews = 0u, m_nPayloads = 0u;
	uint64_t m_nInputBytes = 0u, m_nWorkingBytes = 0u;
};

static bool t3dVTMergeAddBytes( uint64_t& rBytes, const uint64_t nCount, const uint64_t nStride )
{
	uint64_t nBytes = 0u;
	return t3dCheckedMultiply( nCount, nStride, nBytes ) && t3dCheckedAdd( rBytes, nBytes, rBytes );
}

static bool t3dVTPlanMerge( const eastl::vector<t3dVTEditorFragment_t, Terrain3DEastlAllocator>& rFragments,
                            eastl::vector<t3dVTPackBaselineView_t, Terrain3DEastlAllocator>& rViews, t3dVTEditorMergePlan_t& rPlan,
                            const uint64_t nWorkingByteLimit )
{
	if ( rFragments.empty() || rFragments.size() > UINT32_MAX ||
	     !t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rFragments.size(), sizeof( t3dVTPackBaselineView_t ) ) ||
	     rPlan.m_nWorkingBytes > nWorkingByteLimit || !t3dVectorResizeCold( rViews, rFragments.size() ) ) {
		return false;
	}
	for ( size_t i = 0u; i < rFragments.size(); ++i ) {
		const PackedByteArray& bytes = rFragments[i].m_Bytes;
		t3dVTPackBaselineView_t& view = rViews[i];
		if ( !t3d_vt_pack_open_baseline_view( bytes.ptr(), ( uint64_t )bytes.size(), view ) ||
		     ( rFragments[i].m_nStableKey != 0u &&
		       ( view.m_pHeader->m_nAllocationCount != 1u || view.m_pAllocationKeys[0].m_nStableKey != rFragments[i].m_nStableKey ) ) ) {
			return false;
		}
		const t3dVTPackHeader_t& header = *view.m_pHeader;
		if ( !t3dCheckedAdd( rPlan.m_nInputBytes, ( uint64_t )bytes.size(), rPlan.m_nInputBytes ) ||
		     !t3dCheckedAdd( rPlan.m_nAllocations, ( uint64_t )header.m_nAllocationCount, rPlan.m_nAllocations ) ||
		     !t3dCheckedAdd( rPlan.m_nPages, ( uint64_t )header.m_nPageCount, rPlan.m_nPages ) ||
		     !t3dCheckedAdd( rPlan.m_nTails, ( uint64_t )header.m_nTailCount, rPlan.m_nTails ) ||
		     !t3dCheckedAdd( rPlan.m_nViews, ( uint64_t )header.m_nQualityViewCount, rPlan.m_nViews ) ||
		     !t3dCheckedAdd( rPlan.m_nPayloads, ( uint64_t )header.m_nPayloadCount, rPlan.m_nPayloads ) ) {
			return false;
		}
	}
	if ( rPlan.m_nAllocations > UINT32_MAX / T3D_VT_LOGICAL_CHANNEL_COUNT || rPlan.m_nPages > UINT32_MAX || rPlan.m_nTails > UINT32_MAX ||
	     rPlan.m_nViews > UINT32_MAX || rPlan.m_nPayloads > UINT32_MAX / 4u ) {
		return false;
	}
	// Admit the bounded metadata first. Borrowed input bytes are already owned
	// by the caller. Deduplicated concatenation and output are admitted later,
	// when their actual sizes are known; input size is not their working cost.
	// Include allocator/header slack for all table allocations and fragment maps.
	const uint64_t nAllocationSlack = sizeof( t3dEastlAllocationHeader_t ) + T3D_EASTL_DEFAULT_ALIGNMENT + sizeof( t3dNativeMemoryHeader );
	return t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rFragments.size() + 14u, nAllocationSlack ) &&
	       t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rFragments.size(),
	                           sizeof( t3dVTEditorFragment_t ) + sizeof( eastl::vector<uint32_t, Terrain3DEastlAllocator> ) ) &&
	       t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rPlan.m_nAllocations,
	                           sizeof( t3dVTEditorAllocationInput_t ) + sizeof( t3dVTPackAllocationRecord_t ) +
	                               sizeof( t3dVTPackAllocationKeyRecord_t ) +
	                               T3D_VT_LOGICAL_CHANNEL_COUNT * sizeof( t3dVTChannelAllocation_t ) ) &&
	       t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rPlan.m_nPages, sizeof( t3dVTPackPageRecord_t ) ) &&
	       t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rPlan.m_nTails, sizeof( t3dVTPackTailRecord_t ) ) &&
	       t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rPlan.m_nViews, sizeof( t3dVTPackQualityView_t ) ) &&
	       t3dVTMergeAddBytes( rPlan.m_nWorkingBytes, rPlan.m_nPayloads,
	                           sizeof( t3dVTEditorUniquePayload_t ) + sizeof( t3dVTPackPayloadRecord_t ) + 6u * sizeof( uint32_t ) );
}

static bool t3dVTMergeAdmitWorkingBytes( const uint64_t nRequired, const uint64_t nLimit, t3dVTMergeStatistics_t *const pStatistics )
{
	if ( pStatistics != NULL ) {
		pStatistics->m_nWorkingBytes = MAX( pStatistics->m_nWorkingBytes, nRequired );
		pStatistics->m_bWorkingLimitExceeded = nRequired > nLimit;
	}
	return nRequired <= nLimit;
}

static bool t3d_vt_godot_merge_allocation_fragments( const eastl::vector<t3dVTEditorFragment_t, Terrain3DEastlAllocator>& rFragments,
                                                     const uint32_t nPackageGeneration, PackedByteArray& rPackageBytes,
                                                     const uint64_t nWorkingByteLimit = T3D_VT_COOK_WORKING_BYTE_LIMIT,
                                                     t3dVTMergeStatistics_t *const pStatistics = NULL,
                                                     Terrain3DSharedMemoryReservation *const pPackageMemory = NULL,
                                                     String *const pMergeError = NULL )
{
	const char *stage = "input validation";
	const auto fail = [&]() {
		if ( pMergeError ) {
			*pMergeError = String( stage );
		}
		return false;
	};
	if ( rFragments.empty() ) {
		return fail();
	}
	eastl::vector<t3dVTPackAllocationRecord_t, Terrain3DEastlAllocator> allocations;
	eastl::vector<t3dVTPackAllocationKeyRecord_t, Terrain3DEastlAllocator> keys;
	eastl::vector<t3dVTChannelAllocation_t, Terrain3DEastlAllocator> channels;
	eastl::vector<t3dVTPackPageRecord_t, Terrain3DEastlAllocator> pages;
	eastl::vector<t3dVTPackTailRecord_t, Terrain3DEastlAllocator> tails;
	eastl::vector<t3dVTPackQualityView_t, Terrain3DEastlAllocator> views;
	eastl::vector<t3dVTEditorUniquePayload_t, Terrain3DEastlAllocator> uniquePayloads;
	t3dVTEditorMergePlan_t plan;
	t3dVTEditorPayloadIndex_t payloadIndex;
	eastl::vector<t3dVTPackBaselineView_t, Terrain3DEastlAllocator> inputViews;
	eastl::vector<eastl::vector<uint32_t, Terrain3DEastlAllocator>, Terrain3DEastlAllocator> payloadMaps;
	eastl::vector<t3dVTEditorAllocationInput_t, Terrain3DEastlAllocator> inputs;
	if ( !t3dVTPlanMerge( rFragments, inputViews, plan, nWorkingByteLimit ) ) {
		if ( plan.m_nWorkingBytes > nWorkingByteLimit ) {
			( void )t3dVTMergeAdmitWorkingBytes( plan.m_nWorkingBytes, nWorkingByteLimit, pStatistics );
		}
		return fail();
	}
	// This is a local working ceiling, not another shared-account reservation.
	// Native tables and Godot buffers charge that account individually.
	if ( !t3dVTMergeAdmitWorkingBytes( plan.m_nWorkingBytes, nWorkingByteLimit, pStatistics ) ) {
		return fail();
	}
	stage = "metadata allocation";
	if ( !t3dVectorResizeCold( payloadMaps, rFragments.size() ) || !t3dContainerReserveCold( inputs, ( size_t )plan.m_nAllocations ) ||
	     !t3dContainerReserveCold( allocations, ( size_t )plan.m_nAllocations ) ||
	     !t3dContainerReserveCold( keys, ( size_t )plan.m_nAllocations ) ||
	     !t3dContainerReserveCold( channels, ( size_t )plan.m_nAllocations * T3D_VT_LOGICAL_CHANNEL_COUNT ) ||
	     !t3dContainerReserveCold( pages, ( size_t )plan.m_nPages ) || !t3dContainerReserveCold( tails, ( size_t )plan.m_nTails ) ||
	     !t3dContainerReserveCold( views, ( size_t )plan.m_nViews ) ||
	     !t3dContainerReserveCold( uniquePayloads, ( size_t )plan.m_nPayloads ) || !payloadIndex.prepare( ( uint32_t )plan.m_nPayloads ) ) {
		return fail();
	}
	stage = "payload extraction";
	for ( uint32_t iFragment = 0u; iFragment < ( uint32_t )rFragments.size(); ++iFragment ) {
		t3dVTPackBaselineView_t& view = inputViews[iFragment];
		for ( uint32_t iAllocation = 0u; iAllocation < view.m_pHeader->m_nAllocationCount; ++iAllocation ) {
			inputs.push_back( { iFragment, iAllocation, view.m_pAllocationKeys[iAllocation].m_nStableKey } );
		}
		eastl::vector<uint32_t, Terrain3DEastlAllocator>& map = payloadMaps[iFragment];
		if ( !t3dVectorResizeCold( map, view.m_pHeader->m_nPayloadCount ) ) {
			return fail();
		}
		for ( uint32_t iPayload = 0u; iPayload < view.m_pHeader->m_nPayloadCount; ++iPayload ) {
			const uint8_t *pBytes = NULL;
			uint32_t nBytes = 0u;
			if ( !t3d_vt_godot_payload_bytes( view, iPayload, pBytes, nBytes ) ) {
				return fail();
			}
			map[iPayload] = t3d_vt_godot_find_or_add_payload( uniquePayloads, payloadIndex, view.m_pPayloads[iPayload], pBytes, nBytes );
			if ( map[iPayload] == UINT32_MAX ) {
				return fail();
			}
		}
	}
	if ( pStatistics != NULL ) {
		pStatistics->m_nPayloadComparisons = payloadIndex.m_nComparisons;
	}
	stage = "allocation remapping";
	eastl::sort( inputs.begin(), inputs.end(), &t3dVTAllocationInputLess );
	for ( size_t iInput = 0u; iInput < inputs.size(); ++iInput ) {
		const t3dVTEditorAllocationInput_t& input = inputs[iInput];
		if ( iInput != 0u && inputs[iInput - 1u].nStableKey == input.nStableKey ) {
			return fail();
		}
		const t3dVTPackBaselineView_t& view = inputViews[input.nFragment];
		const eastl::vector<uint32_t, Terrain3DEastlAllocator>& payloadMap = payloadMaps[input.nFragment];
		const uint32_t nNewAllocation = ( uint32_t )allocations.size();
		allocations.push_back( view.m_pAllocations[input.nAllocation] );
		t3dVTPackAllocationKeyRecord_t key = view.m_pAllocationKeys[input.nAllocation];
		key.m_nAllocationId = nNewAllocation;
		keys.push_back( key );
		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
			t3dVTChannelAllocation_t channel = view.m_pChannelAllocations[input.nAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
			if ( channel.m_nMipCount != 0u ) {
				const uint32_t nOldBase = channel.m_nBaseVirtualPage;
				const uint32_t nNewBase = ( uint32_t )pages.size();
				for ( uint32_t iPage = 0u; iPage < channel.m_nPageCount; ++iPage ) {
					t3dVTPackPageRecord_t page = view.m_pPages[nOldBase + iPage];
					page.m_nVirtualPage = nNewBase + iPage;
					if ( page.m_nParentPage != T3D_VT_INVALID_PAGE ) {
						page.m_nParentPage = page.m_nParentPage - nOldBase + nNewBase;
					}
					page.m_nAllocationId = nNewAllocation;
					page.m_nPayloadId = payloadMap[page.m_nPayloadId];
					pages.push_back( page );
				}
				t3dVTPackTailRecord_t tail = view.m_pTails[channel.m_nTailIndex];
				tail.m_nCanonicalAllocationId = nNewAllocation;
				tail.m_nPayloadId = payloadMap[tail.m_nPayloadId];
				channel.m_nBaseVirtualPage = nNewBase;
				channel.m_nTailIndex = ( uint32_t )tails.size();
				tails.push_back( tail );
			}
			channels.push_back( channel );
		}
		for ( uint32_t iView = 0u; iView < view.m_pHeader->m_nQualityViewCount; ++iView ) {
			if ( view.m_pQualityViews[iView].m_nAllocationId != input.nAllocation ) {
				continue;
			}
			t3dVTPackQualityView_t quality = view.m_pQualityViews[iView];
			quality.m_nAllocationId = nNewAllocation;
			views.push_back( quality );
		}
	}
	stage = "assembled table validation";
	if ( allocations.empty() || uniquePayloads.empty() || tails.empty() || views.empty() ) {
		return fail();
	}
	eastl::vector<t3dVTPackPayloadRecord_t, Terrain3DEastlAllocator> payloads;
	if ( !t3dVectorResizeCold( payloads, uniquePayloads.size() ) ) {
		return fail();
	}
	uint64_t nPayloadBytes = 0u;
	for ( size_t iPayload = 0u; iPayload < uniquePayloads.size(); ++iPayload ) {
		nPayloadBytes += uniquePayloads[iPayload].m_nBytes;
		if ( nPayloadBytes > INT64_MAX || nPayloadBytes > UINT32_MAX ) {
			return fail();
		}
	}
	uint32_t nPayloadOffset = 0u;
	for ( uint32_t iPayload = 0u; iPayload < ( uint32_t )uniquePayloads.size(); ++iPayload ) {
		payloads[iPayload] = uniquePayloads[iPayload].m_Record;
		payloads[iPayload].m_nClusterId = 0u;
		payloads[iPayload].m_nClusterOffset = nPayloadOffset;
		nPayloadOffset += uniquePayloads[iPayload].m_nBytes;
	}
	t3dVTPackClusterRecord_t cluster;
	memset( &cluster, 0, sizeof( cluster ) );
	cluster.m_nPayloadDataOffset = 0u;
	cluster.m_nStoredBytes = ( uint32_t )nPayloadBytes;
	cluster.m_nDecodedBytes = ( uint32_t )nPayloadBytes;
	cluster.m_nFirstPayload = 0u;
	cluster.m_nPayloadCount = ( uint32_t )payloads.size();
	cluster.m_nCodec = T3D_VT_PACK_CODEC_NONE;
	t3dVTPackFormatFamilyRecord_t formatFamilies[T3D_VT_LOGICAL_CHANNEL_COUNT];
	if ( !t3d_vt_pack_init_format_family_records( formatFamilies, T3D_VT_LOGICAL_CHANNEL_COUNT ) ) {
		return fail();
	}
	t3dVTSerializedPackSource_t packSource;
	memset( &packSource, 0, sizeof( packSource ) );
	packSource.m_pAllocations = allocations.data();
	packSource.m_nAllocationCount = ( uint32_t )allocations.size();
	packSource.m_pAllocationKeys = keys.data();
	packSource.m_pChannelAllocations = channels.data();
	packSource.m_nChannelAllocationCount = ( uint32_t )channels.size();
	packSource.m_pPages = pages.empty() ? NULL : pages.data();
	packSource.m_nPageCount = ( uint32_t )pages.size();
	packSource.m_pPayloads = payloads.data();
	packSource.m_nPayloadCount = ( uint32_t )payloads.size();
	packSource.m_pClusters = &cluster;
	packSource.m_nClusterCount = 1u;
	packSource.m_pTails = tails.data();
	packSource.m_nTailCount = ( uint32_t )tails.size();
	packSource.m_pQualityViews = views.data();
	packSource.m_nQualityViewCount = ( uint32_t )views.size();
	packSource.m_pFormatFamilies = formatFamilies;
	packSource.m_nFormatFamilyCount = T3D_VT_LOGICAL_CHANNEL_COUNT;
	packSource.m_nPayloadDataBytes = nPayloadBytes;
	packSource.m_nPackageGeneration = nPackageGeneration == 0u ? 1u : nPackageGeneration;
	// Assemble borrowed payloads directly into the final package. Keeping a
	// second concatenated copy exhausts live editor headroom during shipping.
	const uint64_t nMetadataBytes = t3d_vt_pack_compute_metadata_bytes( packSource );
	uint64_t nBytes = 0u;
	if ( nMetadataBytes == 0u || !t3dCheckedAdd( nMetadataBytes, nPayloadBytes, nBytes ) || nBytes > INT64_MAX ) {
		return fail();
	}
	stage = "output allocation";
	PackedByteArray output;
	Terrain3DSharedMemoryReservation outputMemory;
	uint64_t nWorkingBytes = 0u;
	if ( !t3dCheckedAdd( plan.m_nWorkingBytes, nBytes + 64u, nWorkingBytes ) ||
	     !t3dVTMergeAdmitWorkingBytes( nWorkingBytes, nWorkingByteLimit, pStatistics ) ) {
		return fail();
	}
	if ( !outputMemory.reserve( nBytes + 64u ) || output.resize( ( int64_t )nBytes ) != OK ) {
		return fail();
	}
	uint8_t *const pPayloadBytes = output.ptrw() + nMetadataBytes;
	for ( uint32_t iPayload = 0u; iPayload < ( uint32_t )uniquePayloads.size(); ++iPayload ) {
		memcpy( pPayloadBytes + payloads[iPayload].m_nClusterOffset, uniquePayloads[iPayload].m_pBytes, uniquePayloads[iPayload].m_nBytes );
	}
	cluster.m_nContentSignature = t3d_vt_pack_hash64( pPayloadBytes, nPayloadBytes );
	cluster.m_nChecksum = t3d_vt_pack_checksum32( pPayloadBytes, nPayloadBytes );
	packSource.m_pPayloadData = pPayloadBytes;
	stage = "package validation";
	if ( t3d_vt_pack_compute_bytes( packSource ) != nBytes ) {
		return fail();
	}
	stage = "package serialization";
	uint64_t nWritten = 0u;
	if ( !t3d_vt_pack_write( packSource, output.ptrw(), nBytes, nWritten ) || nWritten != nBytes ) {
		return fail();
	}
	if ( pMergeError ) {
		*pMergeError = String();
	}
	rPackageBytes = output;
	if ( pPackageMemory != NULL ) {
		*pPackageMemory = outputMemory;
	}
	return true;
}

bool t3d_vt_godot_cook_source_fragment( const t3dVTCookSparseSource_t& rSource, PackedByteArray& rBytes, const uint64_t nWorkingByteLimit )
{
	rBytes.clear();
	t3dVTCookerSettings_t settings{};
	t3d_vt_cooker_default_settings( settings );
	t3dVTCookAllocator_t allocator{};
	t3dVTEditorAllocationBudget_t budget;
	budget.m_nLimit = nWorkingByteLimit;
	allocator.m_pUser = &budget;
	allocator.m_pAllocate = &t3d_vt_godot_editor_allocate;
	allocator.m_pFree = &t3d_vt_godot_editor_free;
	t3dVTEncoderInterface_t encoder{};
	t3d_vt_godot_make_encoder( encoder );
	t3dVTCookedSparsePackage_t package{};
	const bool bCooked = t3d_vt_cooker_cook_sparse_pages( settings, &rSource, 1u, encoder, allocator, NULL, package );
	bool bWritten = false;
	if ( bCooked ) {
		t3dVTSerializedPackSource_t packSource{};
		if ( t3d_vt_cooker_build_pack_source( package, packSource ) ) {
			const uint64_t nBytes = t3d_vt_pack_compute_bytes( packSource );
			if ( budget.m_nLiveBytes <= budget.m_nLimit && nBytes != 0u && nBytes <= INT64_MAX &&
			     nBytes <= budget.m_nLimit - budget.m_nLiveBytes && rBytes.resize( ( int64_t )nBytes ) == OK ) {
				uint64_t nWritten = 0u;
				bWritten = t3d_vt_pack_write( packSource, rBytes.ptrw(), nBytes, nWritten ) && nWritten == nBytes;
			}
		}
	}
	t3d_vt_cooker_release_sparse_package( package );
	if ( !bWritten ) {
		rBytes.clear();
	}
	return bWritten;
}

bool t3d_vt_godot_merge_packages( const PackedByteArray *pPackages, const uint32_t nCount, const uint32_t nGeneration,
                                  PackedByteArray& rPackageBytes, const uint64_t nWorkingByteLimit,
                                  t3dVTMergeStatistics_t *const pStatistics, Terrain3DSharedMemoryReservation *const pPackageMemory )
{
	if ( pStatistics != NULL ) {
		*pStatistics = t3dVTMergeStatistics_t{};
	}
	if ( pPackages == NULL || nCount == 0u ) {
		return false;
	}
	eastl::vector<t3dVTEditorFragment_t, Terrain3DEastlAllocator> fragments;
	if ( !t3dVTMergeAdmitWorkingBytes( ( uint64_t )nCount * sizeof( t3dVTEditorFragment_t ), nWorkingByteLimit, pStatistics ) ||
	     !t3dContainerReserveCold( fragments, nCount ) ) {
		return false;
	}
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		if ( pPackages[i].is_empty() ) {
			continue;
		}
		t3dVTEditorFragment_t fragment{};
		fragment.m_Bytes = pPackages[i];
		fragments.push_back( fragment );
	}
	return t3d_vt_godot_merge_allocation_fragments( fragments, nGeneration, rPackageBytes, nWorkingByteLimit, pStatistics, pPackageMemory );
}

bool t3d_vt_godot_cook_editor_package_incremental( const Ref<Terrain3DAssets>& rAssets, const uint64_t *const pStableKeys,
                                                   const t3dRenderingMaterialGpu_t *const pMaterialRecords, const uint32_t nMaterialCount,
                                                   t3dVTEditorCookCache_t& rCache, PackedByteArray& rPackageBytes, uint64_t& rFingerprint,
                                                   uint32_t& rPackageGeneration, t3dVTEditorCookTelemetry_t& rTelemetry,
                                                   String *const pError, Terrain3DSharedMemoryReservation *const pPackageMemory,
                                                   const bool bForceRebuild )
{
	( void )pMaterialRecords; // Runtime-only scalar/material state must not invalidate page payloads.
	rPackageBytes.clear();
	rFingerprint = 0u;
	rPackageGeneration = 0u;
	memset( &rTelemetry, 0, sizeof( rTelemetry ) );
	if ( rAssets.is_null() || pStableKeys == NULL || nMaterialCount == 0u ) {
		if ( pError ) {
			*pError = "VT editor V1 incremental cooker has no material source";
		}
		return false;
	}
	// Reject ambiguous allocation identities before cooking, and discard cache
	// entries for removed materials before borrowing this invocation's fragments.
	for ( uint32_t i = 0; i < nMaterialCount; ++i ) {
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pStableKeys[i] == pStableKeys[j] ) {
				if ( pError ) {
					*pError = "VT editor V1 incremental cooker encountered duplicate stable keys";
				}
				return false;
			}
		}
	}
	rCache.m_ChannelEntries.clear();
	for ( size_t i = rCache.m_AllocationEntries.size(); i > 0; --i ) {
		bool active = false;
		for ( uint32_t material = 0; material < nMaterialCount; ++material ) {
			if ( rCache.m_AllocationEntries[i - 1].m_nStableKey == pStableKeys[material] ) {
				active = true;
				break;
			}
		}
		if ( !active ) {
			rCache.m_AllocationEntries.erase( rCache.m_AllocationEntries.begin() + ( i - 1 ) );
		}
	}
	t3dVTCookerSettings_t settings;
	t3d_vt_cooker_default_settings( settings );
	t3dVTCookAllocator_t allocator;
	t3dVTEditorAllocationBudget_t allocation_budget;
	allocation_budget.m_nLimit = t3dMemoryCompositionLimit( g_t3dMemoryAccount );
	allocator.m_pUser = &allocation_budget;
	allocator.m_pAllocate = &t3d_vt_godot_editor_allocate;
	allocator.m_pFree = &t3d_vt_godot_editor_free;
	eastl::vector<t3dVTEditorFragment_t, Terrain3DEastlAllocator> fragments;
	if ( !t3dContainerReserveCold( fragments, nMaterialCount ) ) {
		if ( pError ) {
			*pError = "VT editor V1 incremental cooker could not allocate fragment staging";
		}
		return false;
	}
	for ( uint32_t iMaterial = 0u; iMaterial < nMaterialCount; ++iMaterial ) {
		if ( pStableKeys[iMaterial] == 0u ) {
			if ( pError ) {
				*pError = "VT editor V1 incremental cooker encountered a zero stable key";
			}
			return false;
		}
		t3dVTEditorFragment_t fragment;
		fragment.m_nStableKey = pStableKeys[iMaterial];
		if ( !t3d_vt_godot_cook_allocation_fragment( rAssets, fragment.m_nStableKey, rCache, settings, allocator, rTelemetry, false,
		                                             fragment.m_Bytes, fragment.m_nKey, pError, bForceRebuild ) ) {
			return false;
		}
		fragments.push_back( fragment );
	}
	t3d_vt_godot_sort_fragments( fragments );
	uint64_t nFingerprint = 1469598103934665603ULL;
	nFingerprint = t3d_vt_godot_hash_u64( nFingerprint, nMaterialCount );
	for ( size_t iFragment = 0u; iFragment < fragments.size(); ++iFragment ) {
		nFingerprint = t3d_vt_godot_hash_u64( nFingerprint, fragments[iFragment].m_nStableKey );
		nFingerprint = t3d_vt_godot_hash_u64( nFingerprint, fragments[iFragment].m_nKey );
	}
	rFingerprint = nFingerprint == 0u ? 1u : nFingerprint;
	rPackageGeneration = ( uint32_t )( rFingerprint ^ ( rFingerprint >> 32u ) );
	if ( rPackageGeneration == 0u ) {
		rPackageGeneration = 1u;
	}
	rTelemetry.m_nStage = T3D_VT_EDITOR_COOK_STAGE_PACK;
	const uint64_t nPackStart = t3d_vt_godot_now_usec();
	t3dVTMergeStatistics_t statistics;
	const uint64_t denied_before = g_t3dMemoryAccount ? g_t3dMemoryAccount->stats().denied : 0u;
	String merge_error;
	const uint64_t composition_limit = t3dMemoryCompositionLimit( g_t3dMemoryAccount );
	if ( !t3d_vt_godot_merge_allocation_fragments( fragments, rPackageGeneration, rPackageBytes, composition_limit, &statistics,
	                                               pPackageMemory, &merge_error ) ) {
		if ( pError ) {
			*pError = "VT editor V1 incremental cooker could not stage the deterministic package: ";
			if ( g_t3dMemoryAccount && g_t3dMemoryAccount->stats().denied != denied_before ) {
				const auto memory = g_t3dMemoryAccount->stats();
				*pError += String( "shared memory budget exhausted (committed=" ) + String::num_uint64( memory.committed ) +
				           String( ", limit=" ) + String::num_uint64( memory.limit ) + String( ", planned_working_bytes=" ) +
				           String::num_uint64( statistics.m_nWorkingBytes ) + String( ")" );
			} else if ( statistics.m_bWorkingLimitExceeded ) {
				*pError += String( "composition working limit exceeded (required=" ) + String::num_uint64( statistics.m_nWorkingBytes ) +
				           String( ", limit=" ) + String::num_uint64( composition_limit ) + String( ")" );
			} else {
				*pError += "invalid allocation fragments or failed package serialization";
				*pError += String( " (stage=" ) + merge_error + String( ")" );
			}
		}
		return false;
	}
	const uint64_t nPackEnd = t3d_vt_godot_now_usec();
	if ( nPackEnd >= nPackStart ) {
		rTelemetry.m_nPackUsec += nPackEnd - nPackStart;
	}
	t3dVTPackBaselineView_t finalView;
	if ( !t3d_vt_pack_open_baseline_view( rPackageBytes.ptr(), ( uint64_t )rPackageBytes.size(), finalView ) ||
	     finalView.m_pHeader->m_nAllocationCount != nMaterialCount ) {
		rPackageBytes.clear();
		if ( pError ) {
			*pError = "VT editor V1 incremental cooker staged an invalid package";
		}
		return false;
	}
	// Newly encoded allocation fragments are disposable until the aggregate package validates.
	for ( size_t i = 0u; i < rCache.m_AllocationEntries.size(); ++i ) {
		t3dVTEditorAllocationCacheEntry_t& entry = rCache.m_AllocationEntries[i];
		if ( !entry.m_bPendingDiskWrite ) {
			continue;
		}
		t3d_vt_godot_store_allocation_cache_disk( rCache, entry );
		entry.m_bPendingDiskWrite = false;
	}
	rTelemetry.m_nStage = T3D_VT_EDITOR_COOK_STAGE_COMPLETE;
	if ( pError ) {
		*pError = String();
	}
	return true;
}

static bool t3d_vt_godot_validate_shipping_package( const Ref<Terrain3DAssets>& rAssets, const PackedByteArray& rPackageBytes,
                                                    const uint32_t nExpectedAllocationCount, String *const pError )
{
	t3dVTPackBaselineView_t view;
	if ( rPackageBytes.is_empty() || !t3d_vt_pack_open_baseline_view( rPackageBytes.ptr(), ( uint64_t )rPackageBytes.size(), view ) ||
	     view.m_pHeader->m_nVersion != T3D_VT_FILE_FORMAT_VERSION || view.m_pHeader->m_nAllocationCount != nExpectedAllocationCount ||
	     view.m_pHeader->m_nFullySupportedProfileMask == 0u ) {
		if ( pError ) {
			*pError = "VT shipping V1 package failed structural validation";
		}
		return false;
	}
	uint32_t nViewCursor = 0u;
	for ( uint32_t iAllocation = 0u; iAllocation < nExpectedAllocationCount; ++iAllocation ) {
		const t3dVTPackAllocationRecord_t& allocation = view.m_pAllocations[iAllocation];
		const t3dVTPackAllocationKeyRecord_t& key = view.m_pAllocationKeys[iAllocation];
		if ( ( allocation.m_nFlags & T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE ) == 0u ||
		     ( allocation.m_nFlags & T3D_VT_ALLOCATION_EDITOR_DYNAMIC ) != 0u ) {
			if ( pError ) {
				*pError = "VT shipping V1 package contains a non-shipping allocation";
			}
			return false;
		}
		t3dVTGodotSource_t source;
		bool bAuthored = false;
		if ( !t3d_vt_godot_extract_source_by_stable_key( rAssets, key.m_nStableKey, source, bAuthored ) || !bAuthored ||
		     source.m_eHighestAvailableProfile == T3D_VT_FIXED_PROFILE_INVALID || allocation.m_nConsumer != source.m_nConsumer ||
		     allocation.m_nHighestAvailableProfile != ( uint8_t )source.m_eHighestAvailableProfile ) {
			if ( pError ) {
				*pError = "VT shipping V1 allocation/source contract mismatch";
			}
			return false;
		}
		uint32_t sourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT];
		uint32_t nExpectedAllocationMask = 0u;
		for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
			sourceExtent[iChannel] = source.m_ChannelDesc[iChannel].m_nSourceExtent;
			if ( ( source.m_nAuthoredChannelMask & T3D_VT_LOGICAL_CHANNEL_BIT( iChannel ) ) != 0u &&
			     t3d_vt_fixed_profile_channel_extent( source.m_eHighestAvailableProfile, ( t3dVTLogicalChannel_t )iChannel ) != 0u ) {
				nExpectedAllocationMask |= T3D_VT_LOGICAL_CHANNEL_BIT( iChannel );
			}
		}
		if ( allocation.m_nChannelMask != nExpectedAllocationMask ) {
			if ( pError ) {
				*pError = "VT shipping V1 package is missing or misroutes an authored channel";
			}
			return false;
		}
		for ( uint32_t iProfile = 0u; iProfile <= allocation.m_nHighestAvailableProfile; ++iProfile ) {
			const t3dVTFixedProfile_t Profile = ( t3dVTFixedProfile_t )iProfile;
			if ( !t3d_vt_fixed_profile_source_supports( Profile, ( t3dVTConsumer_t )source.m_nConsumer, source.m_nAuthoredChannelMask,
			                                            sourceExtent ) ||
			     nViewCursor >= view.m_pHeader->m_nQualityViewCount ) {
				if ( pError ) {
					*pError = "VT shipping V1 quality view would require an upscale";
				}
				return false;
			}
			const t3dVTPackQualityView_t& qualityView = view.m_pQualityViews[nViewCursor++];
			if ( qualityView.m_nAllocationId != iAllocation || qualityView.m_nProfile != iProfile ) {
				if ( pError ) {
					*pError = "VT shipping V1 quality-view ordering is invalid";
				}
				return false;
			}
			uint32_t nExpectedViewMask = 0u;
			for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
				const uint32_t nBit = T3D_VT_LOGICAL_CHANNEL_BIT( iChannel );
				const bool bEnabled = ( nExpectedAllocationMask & nBit ) != 0u &&
				                      t3d_vt_fixed_profile_channel_extent( Profile, ( t3dVTLogicalChannel_t )iChannel ) != 0u;
				if ( !bEnabled ) {
					if ( qualityView.m_nBaseMip[iChannel] != T3D_VT_PACK_INVALID_MIP ||
					     qualityView.m_nMaxMip[iChannel] != T3D_VT_PACK_INVALID_MIP ) {
						return false;
					}
					continue;
				}
				nExpectedViewMask |= nBit;
				const t3dVTChannelAllocation_t& channel = view.m_pChannelAllocations[iAllocation * T3D_VT_LOGICAL_CHANNEL_COUNT + iChannel];
				uint8_t nExpectedBaseMip = T3D_VT_PACK_INVALID_MIP;
				if ( !t3d_vt_fixed_profile_quality_base_mip( source.m_eHighestAvailableProfile, Profile, ( t3dVTLogicalChannel_t )iChannel,
				                                             channel.m_nMipCount, nExpectedBaseMip ) ||
				     qualityView.m_nBaseMip[iChannel] != nExpectedBaseMip || qualityView.m_nMaxMip[iChannel] != channel.m_nMipCount - 1u ) {
					if ( pError ) {
						*pError = "VT shipping V1 quality-view mip mapping is invalid";
					}
					return false;
				}
			}
			if ( qualityView.m_nChannelMask != nExpectedViewMask ) {
				if ( pError ) {
					*pError = "VT shipping V1 quality-view channel mask is invalid";
				}
				return false;
			}
		}
	}
	if ( nViewCursor != view.m_pHeader->m_nQualityViewCount ) {
		if ( pError ) {
			*pError = "VT shipping V1 package advertises unmatched quality views";
		}
		return false;
	}
	return true;
}

bool t3d_vt_godot_cook_runtime_package( const Ref<Terrain3DAssets>& rAssets, const uint64_t *const pStableKeys,
                                        const t3dRenderingMaterialGpu_t *const pMaterialRecords, const uint32_t nMaterialCount,
                                        PackedByteArray& rPackageBytes, uint32_t& rPackageGeneration, String *const pError )
{
	( void )pMaterialRecords;
	rPackageBytes.clear();
	rPackageGeneration = 0u;
	if ( rAssets.is_null() || pStableKeys == NULL || nMaterialCount == 0u ) {
		if ( pError ) {
			*pError = "VT shipping V1 cooker has no material source";
		}
		return false;
	}
	t3dVTCookerSettings_t settings;
	t3d_vt_cooker_default_settings( settings );
	t3dVTCookAllocator_t allocator;
	allocator.m_pUser = NULL;
	allocator.m_pAllocate = &t3d_vt_godot_editor_allocate;
	allocator.m_pFree = &t3d_vt_godot_editor_free;
	t3dVTEditorCookCache_t cache;
	t3d_vt_godot_editor_cache_reset( cache );
	t3dVTEditorCookTelemetry_t telemetry;
	memset( &telemetry, 0, sizeof( telemetry ) );
	eastl::vector<t3dVTEditorFragment_t, Terrain3DEastlAllocator> fragments;
	if ( !t3dContainerReserveCold( fragments, nMaterialCount ) ) {
		if ( pError ) {
			*pError = "VT shipping V1 cooker could not allocate fragment staging";
		}
		return false;
	}
	for ( uint32_t iMaterial = 0u; iMaterial < nMaterialCount; ++iMaterial ) {
		if ( pStableKeys[iMaterial] == 0u ) {
			if ( pError ) {
				*pError = "VT shipping V1 cooker encountered a zero stable key";
			}
			return false;
		}
		t3dVTEditorFragment_t fragment;
		fragment.m_nStableKey = pStableKeys[iMaterial];
		if ( !t3d_vt_godot_cook_allocation_fragment( rAssets, fragment.m_nStableKey, cache, settings, allocator, telemetry, true,
		                                             fragment.m_Bytes, fragment.m_nKey, pError ) ) {
			return false;
		}
		fragments.push_back( fragment );
	}
	t3d_vt_godot_sort_fragments( fragments );
	uint64_t nFingerprint = 1469598103934665603ULL;
	nFingerprint = t3d_vt_godot_hash_u64( nFingerprint, nMaterialCount );
	for ( size_t iFragment = 0u; iFragment < fragments.size(); ++iFragment ) {
		nFingerprint = t3d_vt_godot_hash_u64( nFingerprint, fragments[iFragment].m_nStableKey );
		nFingerprint = t3d_vt_godot_hash_u64( nFingerprint, fragments[iFragment].m_nKey );
	}
	rPackageGeneration = ( uint32_t )( nFingerprint ^ ( nFingerprint >> 32u ) );
	if ( rPackageGeneration == 0u ) {
		rPackageGeneration = 1u;
	}
	t3dVTMergeStatistics_t statistics{};
	String merge_error;
	const uint64_t composition_limit = t3dMemoryCompositionLimit( g_t3dMemoryAccount );
	const uint64_t denied_before = g_t3dMemoryAccount ? g_t3dMemoryAccount->stats().denied : 0u;
	if ( !t3d_vt_godot_merge_allocation_fragments( fragments, rPackageGeneration, rPackageBytes, composition_limit, &statistics, NULL,
	                                               &merge_error ) ) {
		rPackageBytes.clear();
		rPackageGeneration = 0u;
		if ( pError ) {
			*pError = String( "VT shipping V1 package merge failed (stage=" ) + merge_error + String( ")" );
			if ( g_t3dMemoryAccount && g_t3dMemoryAccount->stats().denied != denied_before ) {
				const auto memory = g_t3dMemoryAccount->stats();
				*pError += String( ": shared memory budget exhausted (committed=" ) + String::num_uint64( memory.committed ) +
				           String( ", limit=" ) + String::num_uint64( memory.limit ) + String( ")" );
			} else if ( statistics.m_bWorkingLimitExceeded ) {
				*pError += String( ": composition working limit exceeded (required=" ) + String::num_uint64( statistics.m_nWorkingBytes ) +
				           String( ", limit=" ) + String::num_uint64( composition_limit ) + String( ")" );
			}
		}
		return false;
	}
	if ( !t3d_vt_godot_validate_shipping_package( rAssets, rPackageBytes, nMaterialCount, pError ) ) {
		rPackageBytes.clear();
		rPackageGeneration = 0u;
		if ( pError && pError->is_empty() ) {
			*pError = "VT shipping V1 package validation failed";
		}
		return false;
	}
	if ( pError ) {
		*pError = String();
	}
	return true;
}

bool t3d_vt_godot_collect_asset_materials( const Ref<Terrain3DAssets>& rAssets, t3dVTGodotAssetMaterials_t& rMaterials,
                                           String *const pError )
{
	rMaterials.m_stableKeys.clear();
	rMaterials.m_records.clear();
	uint32_t rMaterialCount = 0u;
	if ( rAssets.is_null() ) {
		if ( pError ) {
			*pError = "VT asset material collector has no Terrain3DAssets source";
		}
		return false;
	}
	const uint32_t nTerrainMaterialCount =
	    rAssets->usesPackedPbr() ? ( uint32_t )MIN( rAssets->getTextureCount(), T3D_MAX_TEXTURE_ASSETS ) : 0u;
	uint32_t nMaxMaterialRefs = nTerrainMaterialCount;
	const int nMeshCount = rAssets->getMeshCount();
	for ( int iMesh = 0; iMesh < nMeshCount; ++iMesh ) {
		const Ref<Terrain3DMeshAsset> asset = rAssets->getMeshAsset( iMesh );
		if ( asset.is_null() || !asset->isEnabled() ) {
			continue;
		}
		const TypedArray<Terrain3DRenderingMaterial> surfaceMaterials = asset->getSurfaceRenderingMaterials();
		if ( surfaceMaterials.size() > INT32_MAX - 2 || nMaxMaterialRefs > UINT32_MAX - ( uint32_t )surfaceMaterials.size() - 2u ) {
			if ( pError ) {
				*pError = "VT asset material collector material reference count overflowed";
			}
			return false;
		}
		nMaxMaterialRefs += ( uint32_t )surfaceMaterials.size() + 2u;
	}
	if ( nMaxMaterialRefs == 0u ) {
		if ( pError ) {
			*pError = String();
		}
		return true;
	}
	if ( !t3dContainerReserveCold( rMaterials.m_stableKeys, nMaxMaterialRefs ) ||
	     !t3dContainerReserveCold( rMaterials.m_records, nMaxMaterialRefs ) ) {
		if ( pError ) {
			*pError = "VT asset material collector could not allocate material identity tables";
		}
		return false;
	}
	t3dVectorResizeWithinCapacity( rMaterials.m_stableKeys, nMaxMaterialRefs );
	t3dVectorResizeWithinCapacity( rMaterials.m_records, nMaxMaterialRefs );
	uint64_t *const pStableKeys = rMaterials.m_stableKeys.data();
	t3dRenderingMaterialGpu_t *const pMaterialRecords = rMaterials.m_records.data();
	for ( uint32_t iMaterial = 0u; iMaterial < nTerrainMaterialCount; ++iMaterial ) {
		const Ref<Terrain3DTextureAsset> asset = rAssets->getTextureAsset( ( int )iMaterial );
		if ( asset.is_null() ) {
			continue;
		}
		pStableKeys[rMaterialCount] = t3d_vt_terrain_material_stable_key( iMaterial );
		memset( &pMaterialRecords[rMaterialCount], 0, sizeof( pMaterialRecords[rMaterialCount] ) );
		pMaterialRecords[rMaterialCount].m_nVtAllocation = T3D_VT_INVALID_ALLOCATION;
		pMaterialRecords[rMaterialCount].m_nOverlayVtAllocation = T3D_VT_INVALID_ALLOCATION;
		++rMaterialCount;
	}
	auto appendMaterial = [&]( const Ref<Terrain3DRenderingMaterial>& rMaterial ) -> bool {
		if ( rMaterial.is_null() ) {
			return true;
		}
		// Scalar-only instance materials do not own VT payloads. Their GPU record
		// remains valid with T3D_VT_INVALID_ALLOCATION and the renderer supplies
		// neutral texture backing. Do not manufacture a required ALBEDO channel.
		// If any texture is authored, keep the material in the VT set so a missing
		// required ALBEDO still fails validation instead of being silently hidden.
		if ( rMaterial->getAlbedoTexture().is_null() && rMaterial->getNormalTexture().is_null() &&
		     rMaterial->getSurfaceTexture().is_null() ) {
			return true;
		}
		const uint64_t nStableKey = rMaterial->getVtStableKey();
		uint32_t nReservedTerrainId = 0u;
		if ( nStableKey == 0u || t3d_vt_decode_terrain_material_stable_key( nStableKey, nReservedTerrainId ) ||
		     t3d_vt_is_terrain_region_stable_key( nStableKey ) ) {
			return false;
		}
		for ( uint32_t iKey = 0u; iKey < rMaterialCount; ++iKey ) {
			if ( pStableKeys[iKey] == nStableKey ) {
				return true;
			}
		}
		if ( rMaterialCount >= nMaxMaterialRefs ||
		     !t3d_vt_godot_compile_material_record( rMaterial, 0u, pMaterialRecords[rMaterialCount] ) ) {
			return false;
		}
		pStableKeys[rMaterialCount++] = nStableKey;
		return true;
	};
	bool bCollected = true;
	for ( int iMesh = 0; iMesh < nMeshCount && bCollected; ++iMesh ) {
		const Ref<Terrain3DMeshAsset> asset = rAssets->getMeshAsset( iMesh );
		if ( asset.is_null() || !asset->isEnabled() ) {
			continue;
		}
		bCollected = asset->ensureRenderingMaterialStableKeysCold() && appendMaterial( asset->getRenderingMaterialOverride() ) &&
		             appendMaterial( asset->getRenderingMaterialOverlay() );
		const TypedArray<Terrain3DRenderingMaterial> surfaceMaterials = asset->getSurfaceRenderingMaterials();
		for ( int iMaterial = 0; bCollected && iMaterial < surfaceMaterials.size(); ++iMaterial ) {
			bCollected = appendMaterial( surfaceMaterials[iMaterial] );
		}
	}
	if ( !bCollected ) {
		rMaterials.m_stableKeys.clear();
		rMaterials.m_records.clear();
		if ( pError ) {
			*pError = "VT asset material collector could not establish stable material identity";
		}
		return false;
	}
	t3dVectorResizeWithinCapacity( rMaterials.m_stableKeys, rMaterialCount );
	t3dVectorResizeWithinCapacity( rMaterials.m_records, rMaterialCount );
	if ( pError ) {
		*pError = String();
	}
	return true;
}

bool t3d_vt_godot_cook_runtime_assets_package( const Ref<Terrain3DAssets>& rAssets, PackedByteArray& rPackageBytes,
                                               uint32_t& rPackageGeneration, uint32_t& rMaterialCount, String *const pError )
{
	rPackageBytes.clear();
	rPackageGeneration = 0u;
	rMaterialCount = 0u;
	t3dVTGodotAssetMaterials_t materials;
	if ( !t3d_vt_godot_collect_asset_materials( rAssets, materials, pError ) ) {
		return false;
	}
	rMaterialCount = ( uint32_t )materials.m_stableKeys.size();
	if ( rMaterialCount == 0u ) {
		return true;
	}
	return t3d_vt_godot_cook_runtime_package( rAssets, materials.m_stableKeys.data(), materials.m_records.data(), rMaterialCount,
	                                          rPackageBytes, rPackageGeneration, pError );
}

bool t3d_vt_godot_compile_material_record( const Ref<Terrain3DRenderingMaterial>& rMaterial, const uint32_t nAllocationId,
                                           t3dRenderingMaterialGpu_t& rRecord )
{
	if ( rMaterial.is_null() || nAllocationId == T3D_VT_INVALID_ALLOCATION ) {
		return false;
	}
	t3dRenderingMaterialVTBindings_t Bindings;
	Bindings.m_nVtAllocation = nAllocationId;
	Bindings.m_nOverlayVtAllocation = T3D_VT_INVALID_ALLOCATION;
	return rMaterial->compileVtRecordCold( Bindings, rRecord );
}

// Existing CS13 sidecar output is a cold page source, never a second physical
// residency owner. Feed it through the normal V1 cooker/merged package so page
// allocations, root tails, uploads and eviction all belong to VirtualTexture.
// This function is called only after the editor has verified material parity
// and supplied the independently computed current source fingerprint.
// UCMP-03: decode one complete authored composed mip from the validated .t3sc
// sidecar into its V1 logical format. The BC decoder is the existing CS15
// decoder. No independent GPU residency or second page table is created.
static bool t3d_comp_surf_ucmp_decode_mip_cold( const t3dComposedSurfaceSidecarView_t& rView, const uint8_t *const pSidecar,
                                                const uint64_t nSidecarBytes, const uint64_t nRegionKey, const uint32_t nMip,
                                                const uint32_t nTiles, const uint32_t nFormat, const uint32_t nChannel,
                                                const uint32_t nComponents, uint8_t *const pOutput, const uint32_t nOutputExtent )
{
	if ( !pOutput || !nTiles || nTiles > 4u || !nOutputExtent )
		return false;
	const uint32_t nativeExtent = nTiles * T3D_COMP_SURF_PAGE_INTERIOR;
	PackedByteArray native, decoded;
	if ( native.resize( int64_t( nativeExtent ) * nativeExtent * nComponents ) != OK ||
	     decoded.resize( T3D_COMP_SURF_RGBA_SCRATCH_BYTES ) != OK )
		return false;
	uint8_t *const pNative = native.ptrw();
	for ( uint32_t pageY = 0u; pageY < nTiles; ++pageY ) {
		for ( uint32_t pageX = 0u; pageX < nTiles; ++pageX ) {
			uint32_t index = UINT32_MAX;
			t3dComposedSurfacePageRecord_t page{};
			if ( !t3d_comp_surf_find_page_location( rView, nRegionKey, nMip, pageX, pageY, index ) ||
			     !t3d_comp_surf_sidecar_record( rView, index, page ) || page.m_nChannelMask != T3D_COMP_SURF_CHANNEL_MASK_ALL )
				return false;
			const t3dComposedSurfaceChannelRecord_t& blob = page.m_channels[nChannel];
			if ( !t3d_comp_surf_range_within( blob.m_nFileOffset, blob.m_nByteCount, nSidecarBytes ) ||
			     !t3d_comp_surf_validate_channel_payload( blob, pSidecar + blob.m_nFileOffset, blob.m_nByteCount ) ||
			     !t3d_composed_surface_pack_decode_godot_image( NULL, nFormat, pSidecar + blob.m_nFileOffset, blob.m_nByteCount,
			                                                    decoded.ptrw(), T3D_COMP_SURF_RGBA_SCRATCH_BYTES ) )
				return false;
			const uint8_t *const rgba = decoded.ptr();
			for ( uint32_t y = 0u; y < T3D_COMP_SURF_PAGE_INTERIOR; ++y ) {
				for ( uint32_t x = 0u; x < T3D_COMP_SURF_PAGE_INTERIOR; ++x ) {
					const uint32_t nx = pageX * T3D_COMP_SURF_PAGE_INTERIOR + x;
					const uint32_t ny = pageY * T3D_COMP_SURF_PAGE_INTERIOR + y;
					const uint32_t rx = x + T3D_COMP_SURF_PAGE_GUTTER;
					const uint32_t ry = y + T3D_COMP_SURF_PAGE_GUTTER;
					memcpy( pNative + ( ny * nativeExtent + nx ) * nComponents, rgba + ( ry * T3D_COMP_SURF_PHYSICAL_TILE + rx ) * 4u,
					        nComponents );
				}
			}
		}
	}
	for ( uint32_t y = 0u; y < nOutputExtent; ++y ) {
		const uint32_t sy = uint32_t( uint64_t( y ) * nativeExtent / nOutputExtent );
		for ( uint32_t x = 0u; x < nOutputExtent; ++x ) {
			const uint32_t sx = uint32_t( uint64_t( x ) * nativeExtent / nOutputExtent );
			memcpy( pOutput + ( y * nOutputExtent + x ) * nComponents, pNative + ( sy * nativeExtent + sx ) * nComponents, nComponents );
		}
	}
	return true;
}

static bool t3d_comp_surf_ucmp_prepare_channel_cold( const t3dComposedSurfaceSidecarView_t& rView, const uint8_t *const pSidecar,
                                                     const uint64_t nSidecarBytes, const uint64_t nRegionKey, const uint32_t nTiles,
                                                     const uint32_t nChannel, const t3dVTLogicalChannel_t eLogical,
                                                     const t3dVTPhysicalFormatFamily_t eFormat, const uint32_t nComponents,
                                                     const uint32_t nOutputExtent, PackedByteArray& rPrepared,
                                                     t3dVTMipChainLayout_t& rLayout )
{
	rPrepared.clear();
	if ( !t3d_vt_page_builder_layout_mips( nOutputExtent, nOutputExtent, nComponents, rLayout ) || rLayout.m_nTotalBytes > INT32_MAX ||
	     rPrepared.resize( ( int64_t )rLayout.m_nTotalBytes ) != OK )
		return false;
	const uint32_t sourceExtent = nTiles * T3D_COMP_SURF_PAGE_INTERIOR;
	uint32_t sourceMipMax = 0u;
	for ( uint32_t n = nTiles; n > 1u; n >>= 1u )
		++sourceMipMax;
	// Map the authored mip hierarchy to the V1 channel-specific quality view.
	// NORMAL/PBR have half the maximum ALBEDO resolution. Beyond the last
	// authored mip, use the existing V1 channel-specific filtering for tails.
	uint32_t lastWritten = UINT32_MAX;
	for ( uint32_t level = 0u; level < rLayout.m_nMipCount; ++level ) {
		const uint32_t extent = rLayout.m_Levels[level].m_nWidth;
		uint32_t sampledMip = 0u;
		uint32_t checkExtent = sourceExtent;
		while ( checkExtent > extent && sampledMip < sourceMipMax ) {
			checkExtent >>= 1u;
			++sampledMip;
		}
		if ( checkExtent > extent )
			break;
		const uint32_t tiles = MAX( 1u, nTiles >> sampledMip );
		if ( !t3d_comp_surf_ucmp_decode_mip_cold( rView, pSidecar, nSidecarBytes, nRegionKey, sampledMip, tiles, ( uint32_t )eFormat,
		                                          nChannel, nComponents, rPrepared.ptrw() + rLayout.m_Levels[level].m_nOffset, extent ) )
			return false;
		lastWritten = level;
	}
	if ( lastWritten == UINT32_MAX )
		return false;
	if ( lastWritten + 1u == rLayout.m_nMipCount )
		return true;
	// The V1 mip builder already handles gamma-aware color and normalized
	// normals. Build the remaining tail from the last authored composed mip.
	const t3dVTMipLevelLayout_t& last = rLayout.m_Levels[lastWritten];
	t3dVTMipChainLayout_t tailLayout{};
	if ( !t3d_vt_page_builder_layout_mips( last.m_nWidth, last.m_nHeight, nComponents, tailLayout ) ||
	     tailLayout.m_nTotalBytes > INT32_MAX )
		return false;
	PackedByteArray tail;
	if ( tail.resize( ( int64_t )tailLayout.m_nTotalBytes ) != OK )
		return false;
	memcpy( tail.ptrw(), rPrepared.ptr() + last.m_nOffset, last.m_nBytes );
	if ( !t3d_vt_page_builder_build_logical_channel_mips_u8( eLogical, false, 0u, tail.ptrw(), tailLayout.m_nTotalBytes, tailLayout, 1u ) )
		return false;
	for ( uint32_t i = 1u; lastWritten + i < rLayout.m_nMipCount; ++i ) {
		const t3dVTMipLevelLayout_t& target = rLayout.m_Levels[lastWritten + i];
		const t3dVTMipLevelLayout_t& source = tailLayout.m_Levels[i];
		if ( target.m_nBytes != source.m_nBytes )
			return false;
		memcpy( rPrepared.ptrw() + target.m_nOffset, tail.ptr() + source.m_nOffset, target.m_nBytes );
	}
	return true;
}

bool t3d_vt_godot_import_composed_sidecar( const uint8_t *const pSidecar, const uint64_t nSidecarBytes,
                                           const uint64_t nExpectedSourceFingerprint, const uint32_t nPackageGeneration,
                                           PackedByteArray& rFragment, String *const pError )
{
	rFragment.clear();
	const auto fail = [&]( const char *const pszReason ) -> bool {
		if ( pError )
			*pError = String( pszReason );
		return false;
	};
	if ( !pSidecar || nSidecarBytes > SIZE_MAX || nSidecarBytes < sizeof( t3dComposedSurfaceFileHeader_t ) || !nExpectedSourceFingerprint ||
	     !nPackageGeneration )
		return fail( "Invalid composed V1 sidecar import inputs" );
	t3dComposedSurfaceSidecarView_t view{};
	if ( !t3d_comp_surf_open_sidecar_cold( pSidecar, ( size_t )nSidecarBytes, nExpectedSourceFingerprint, view ) ||
	     !view.m_header.m_nPageCount || !t3d_comp_surf_validate_sidecar_bytes( pSidecar, ( size_t )nSidecarBytes, true ) )
		return fail( "Stale, corrupt or invalid composed sidecar" );

	const t3dVTLogicalChannel_t logical[3] = { T3D_VT_LOGICAL_CHANNEL_ALBEDO, T3D_VT_LOGICAL_CHANNEL_NORMAL, T3D_VT_LOGICAL_CHANNEL_PBR };
	const t3dVTPhysicalFormatFamily_t format[3] = { T3D_VT_PHYSICAL_FORMAT_BC1_UNORM, T3D_VT_PHYSICAL_FORMAT_BC5_UNORM,
	                                                T3D_VT_PHYSICAL_FORMAT_BC7_UNORM };
	const uint32_t components[3] = { 4u, 2u, 4u };
	eastl::vector<PackedByteArray, Terrain3DEastlAllocator> fragments;
	if ( !t3dContainerReserveCold( fragments, MIN( view.m_header.m_nPageCount, T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY ) ) )
		return fail( "Composed V1 fragment reservation failed" );

	uint64_t previousRegion = 0u;
	for ( uint32_t index = 0u; index < view.m_header.m_nPageCount; ) {
		t3dComposedSurfacePageRecord_t first{};
		if ( !t3d_comp_surf_sidecar_record( view, index, first ) || first.m_nRegionKey <= previousRegion ||
		     first.m_nRegionKey > T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY )
			return fail( "Composed region order or stable-key range invalid" );
		t3dComposedSurfacePageRecord_t records[T3D_COMP_SURF_MAX_REGION_MIP_PAGES]{};
		uint32_t count = 0u;
		while ( index < view.m_header.m_nPageCount ) {
			t3dComposedSurfacePageRecord_t page{};
			if ( !t3d_comp_surf_sidecar_record( view, index, page ) )
				return fail( "Unreadable composed page record" );
			if ( page.m_nRegionKey != first.m_nRegionKey )
				break;
			if ( count == T3D_COMP_SURF_MAX_REGION_MIP_PAGES )
				return fail( "Composed region exceeds bounded mip hierarchy" );
			records[count++] = page;
			++index;
		}
		uint32_t tiles = 0u;
		uint64_t signature = 0u;
		if ( !t3d_comp_surf_import_region_mips_complete( records, count, tiles, signature ) )
			return fail( "Incomplete or unordered composed mip hierarchy" );
		previousRegion = first.m_nRegionKey;
		const uint32_t nativeExtent = tiles * T3D_COMP_SURF_PAGE_INTERIOR;
		const uint32_t albedoExtent = MAX( nativeExtent, 256u );
		const uint32_t outputExtent[3] = { albedoExtent, MAX( albedoExtent / 2u, 128u ), MAX( albedoExtent / 2u, 128u ) };
		t3dVTCookSparseSource_t source{};
		source.m_nStableKey = t3d_vt_composed_region_stable_key( ( uint32_t )( first.m_nRegionKey - 1u ) );
		source.m_nConsumer = T3D_VT_CONSUMER_COMPOSED;
		source.m_nAuthoredChannelMask = T3D_VT_COMPOSED_REQUIRED_CHANNEL_MASK;
		source.m_nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
		source.m_nAllocationFlags = T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP | T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
		source.m_nGenerationSeed = ( uint32_t )( signature ^ ( signature >> 32u ) );
		if ( !source.m_nGenerationSeed )
			source.m_nGenerationSeed = 1u;
		PackedByteArray prepared[3];
		for ( uint32_t channel = 0u; channel < 3u; ++channel ) {
			t3dVTMipChainLayout_t layout{};
			if ( !t3d_comp_surf_ucmp_prepare_channel_cold( view, pSidecar, nSidecarBytes, first.m_nRegionKey, tiles, channel,
			                                               logical[channel], format[channel], components[channel], outputExtent[channel],
			                                               prepared[channel], layout ) )
				return fail( "Composed mip decode or V1 channel preparation failed" );
			t3dVTCookPreparedChannel_t& p = source.m_PreparedChannels[( uint32_t )logical[channel]];
			p.m_pMipBytes = prepared[channel].ptr();
			p.m_Layout = layout;
			p.m_nTargetExtent = outputExtent[channel];
			t3dVTCookImageView_t& image = source.m_Channels[( uint32_t )logical[channel]];
			image.m_pPixels = prepared[channel].ptr();
			image.m_nWidth = outputExtent[channel];
			image.m_nHeight = outputExtent[channel];
			image.m_nComponents = components[channel];
			image.m_nRowBytes = outputExtent[channel] * components[channel];
			source.m_nSourceFingerprint[( uint32_t )logical[channel]] = signature ^ ( uint64_t( channel + 1u ) << 56u );
		}
		PackedByteArray fragment;
		if ( !t3d_vt_godot_cook_source_fragment( source, fragment ) || fragment.is_empty() )
			return fail( "Composed V1 prepared channel cook failed" );
		fragments.push_back( fragment );
	}
	if ( fragments.empty() ||
	     !t3d_vt_godot_merge_packages( fragments.data(), ( uint32_t )fragments.size(), nPackageGeneration, rFragment ) ||
	     rFragment.is_empty() ) {
		rFragment.clear();
		return fail( "Composed V1 package merge failed" );
	}
	if ( pError )
		*pError = String();
	return true;
}
