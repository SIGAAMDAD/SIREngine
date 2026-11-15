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

#include <Core/Platform/SIRPlatformFile.h>
#include "Terrain3D/src/authoring/vt/build/terrain_3d_vt_cooker.h"
#include "Terrain3D/src/authoring/vt/build/terrain_3d_vt_reference_encoder.h"
#include "Terrain3D/src/authoring/vt/io/terrain_3d_vt_pack_writer.h"

#if defined( SIR_VT_WITH_ZSTD )
#include <zstd.h>
#endif

#include <limits.h>
#include <string.h>

namespace
{
struct VTTransientSources
{
	const sirVirtualTextureCookAllocator_t& allocator;
	t3dVTCookSparseSource_t *pSources;

	~VTTransientSources( void )
	{
		if ( pSources != nullptr ) {
			allocator.pfnFree( allocator.pUser, pSources );
		}
	}
};

bool initializeSources( const sirVirtualTextureCookSource_t *pSources, uint32_t nSources, VTTransientSources& temporary )
{
	if ( pSources == nullptr || nSources == 0u || nSources > UINT32_MAX / sizeof( t3dVTCookSparseSource_t ) ) {
		return false;
	}
	temporary.pSources = static_cast<t3dVTCookSparseSource_t *>(
	    temporary.allocator.pfnAllocate( temporary.allocator.pUser, ( uint64_t )nSources * sizeof( t3dVTCookSparseSource_t ), 16u ) );
	if ( temporary.pSources == nullptr ) {
		return false;
	}
	memset( temporary.pSources, 0, ( size_t )nSources * sizeof( t3dVTCookSparseSource_t ) );
	for ( uint32_t i = 0u; i < nSources; ++i ) {
		const sirVirtualTextureCookSource_t& input = pSources[i];
		if ( input.nAllocationFlags > UINT16_MAX ) {
			return false;
		}
		t3dVTCookSparseSource_t& source = temporary.pSources[i];
		source.m_nStableKey = input.nStableKey;
		source.m_nAuthoredChannelMask = input.nAuthoredChannelMask;
		source.m_nOrmSourceLayout = input.nOrmSourceLayout;
		source.m_nWrapMode = input.nWrapMode;
		source.m_nConsumer = input.nConsumer;
		source.m_nAllocationFlags = ( uint16_t )input.nAllocationFlags;
		source.m_nGenerationSeed = input.nGenerationSeed;
		source.m_bAlphaScissor = input.bAlphaScissor;
		source.m_nAlphaThreshold = input.nAlphaThreshold;
		for ( uint32_t channel = 0u; channel < 6u; ++channel ) {
			source.m_nSourceFingerprint[channel] = input.nSourceFingerprint[channel];
			const sirVirtualTextureCookImage_t& image = input.images[channel];
			if ( ( input.nAuthoredChannelMask & ( 1u << channel ) ) != 0u ) {
				const uint64_t rowBytes = ( uint64_t )image.nWidth * image.nComponents;
				if ( image.pPixels == nullptr || image.nWidth == 0u || image.nHeight == 0u || image.nComponents == 0u ||
				     image.nComponents > 4u || rowBytes > UINT32_MAX || image.nRowBytes < rowBytes ||
				     ( uint64_t )( image.nHeight - 1u ) * image.nRowBytes + rowBytes > image.nBackingBytes ) {
					return false;
				}
			}
			source.m_Channels[channel] = { image.pPixels, image.nWidth, image.nHeight, image.nComponents, image.nRowBytes };
		}
	}
	return true;
}

#if defined( SIR_VT_WITH_ZSTD )
uint64_t zstdBound( void *, uint32_t codec, uint64_t inputBytes )
{
	if ( codec != T3D_VT_PACK_CODEC_ZSTD || inputBytes > SIZE_MAX ) {
		return 0u;
	}
	return ( uint64_t )ZSTD_compressBound( ( size_t )inputBytes );
}

bool zstdEncode( void *, uint32_t codec, const uint8_t *pSource, uint32_t sourceBytes, uint8_t *pDestination, uint32_t capacity,
                 uint32_t& produced )
{
	produced = 0u;
	if ( codec != T3D_VT_PACK_CODEC_ZSTD || pSource == nullptr || pDestination == nullptr ) {
		return false;
	}
	const size_t written = ZSTD_compress( pDestination, capacity, pSource, sourceBytes, 3 );
	if ( ZSTD_isError( written ) != 0u || written > UINT32_MAX ) {
		return false;
	}
	produced = ( uint32_t )written;
	return true;
}
#endif

bool serializeCooked( const t3dVTCookedSparsePackage_t& package, sirVirtualTextureCookOutput_t& output )
{
	t3dVTSerializedPackSource_t source{};
	if ( !t3d_vt_cooker_build_pack_source( package, source ) || !t3d_vt_pack_validate_source( source ) ) {
		return false;
	}
	const uint64_t required = t3d_vt_pack_compute_bytes( source );
	if ( required == 0u || required > output.nCapacity ) {
		return false;
	}
	return t3d_vt_pack_write( source, output.pBytes, output.nCapacity, output.nWritten );
}
} // namespace

bool SIRVirtualTextureCooker::supportsZstd( void ) noexcept
{
#if defined( SIR_VT_WITH_ZSTD )
	return true;
#else
	return false;
#endif
}

bool SIRVirtualTextureCooker::cook( const sirVirtualTextureCookBatch_t& batch, sirVirtualTextureCookOutput_t& output ) noexcept
{
	const sirVirtualTextureCookAllocator_t& allocator = batch.allocator;
	const bool bZstd = batch.bZstd;
	output.nWritten = 0u;
	if ( allocator.pfnAllocate == nullptr || allocator.pfnFree == nullptr || output.pBytes == nullptr || output.nCapacity == 0u ||
	     ( bZstd && !supportsZstd() ) ) {
		return false;
	}
	VTTransientSources sources{ allocator, nullptr };
	if ( !initializeSources( batch.pSources, batch.nSourceCount, sources ) ) {
		return false;
	}
	t3dVTCookAllocator_t donorAllocator{ allocator.pUser, allocator.pfnAllocate, allocator.pfnFree };
	t3dVTEncoderInterface_t encoder{};
	t3d_vt_reference_encoder_make_interface( encoder );
	t3dVTCookerSettings_t settings{};
	t3d_vt_cooker_default_settings( settings );
	settings.m_nOuterCompression = bZstd ? T3D_VT_OUTER_COMPRESSION_ZSTD : T3D_VT_OUTER_COMPRESSION_NONE;
	t3dVTCookClusterCodec_t codec{};
#if defined( SIR_VT_WITH_ZSTD )
	codec.m_pBound = &zstdBound;
	codec.m_pEncode = &zstdEncode;
#endif
	t3dVTCookedSparsePackage_t cooked{};
	const bool success = t3d_vt_cooker_cook_sparse_pages( settings, sources.pSources, batch.nSourceCount, encoder, donorAllocator,
	                                                      bZstd ? &codec : nullptr, cooked );
	if ( !success ) {
		t3d_vt_cooker_release_sparse_package( cooked );
		return false;
	}
	const bool serialized = serializeCooked( cooked, output );
	t3d_vt_cooker_release_sparse_package( cooked );
	return serialized;
}

bool SIRVirtualTextureCooker::publishAtomic( const char *pszDestination, const char *pszStaging,
                                             const sirVirtualTextureCookOutput_t& output ) noexcept
{
	if ( pszDestination == nullptr || pszStaging == nullptr || pszDestination[0] == '\0' || pszStaging[0] == '\0' ||
	     strcmp( pszDestination, pszStaging ) == 0 || output.pBytes == nullptr || output.nWritten < T3D_VT_PACK_HEADER_BYTES ||
	     output.nWritten > output.nCapacity ) {
		return false;
	}
	t3dVTPackBaselineView_t validated{};
	if ( !t3d_vt_pack_open_baseline_view( output.pBytes, output.nWritten, validated ) ) {
		return false;
	}
	SIRPlatformFile file;
	if ( file.open( pszStaging, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_EXCLUSIVE ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t transferred = 0u;
	const bool written = file.writeAt( 0u, output.pBytes, output.nWritten, &transferred ) == SIR_FILE_OK &&
	                     transferred == output.nWritten && file.flush() == SIR_FILE_OK;
	const bool closed = file.close() == SIR_FILE_OK;
	if ( !written || !closed ) {
		SIRPlatformFile::remove( pszStaging );
		return false;
	}
	if ( SIRPlatformFile::renameReplace( pszStaging, pszDestination ) != SIR_FILE_OK ) {
		SIRPlatformFile::remove( pszStaging );
		return false;
	}
	return true;
}
