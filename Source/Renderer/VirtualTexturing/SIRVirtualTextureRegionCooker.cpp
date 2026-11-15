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

#include <Renderer/VirtualTexturing/SIRVirtualTextureRegionCooker.h>
#include "Terrain3D/src/rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "Terrain3D/src/core/utility/terrain_3d_hash_bytes.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

namespace
{
bool overlaps( const void *pA, uint64_t nA, const void *pB, uint64_t nB )
{
	if ( pA == nullptr || pB == nullptr || nA == 0u || nB == 0u ) {
		return false;
	}
	const uintptr_t a = ( uintptr_t )pA;
	const uintptr_t b = ( uintptr_t )pB;
	if ( nA > UINTPTR_MAX - a || nB > UINTPTR_MAX - b ) {
		return true;
	}
	return a < b + nB && b < a + nA;
}

bool validateImage( const sirVirtualTextureCookImage_t& image )
{
	const uint64_t rowBytes = ( uint64_t )image.nWidth * 4u;
	return image.pPixels != nullptr && image.nWidth != 0u && image.nHeight != 0u && image.nWidth <= INT32_MAX &&
	       image.nHeight <= INT32_MAX && image.nComponents == 4u && image.nRowBytes >= rowBytes &&
	       ( uint64_t )( image.nHeight - 1u ) * image.nRowBytes + rowBytes <= image.nBackingBytes;
}

bool validateRegion( const sirVirtualTextureRegionCookRequest_t& request, const sirVirtualTextureRegionSurfaces_t& surfaces )
{
	const uint32_t extent = request.nExtent;
	if ( ( extent != 256u && extent != 512u ) || request.nGeneration == 0u || request.nRegionIndex >= 1024u ||
	     ( request.bReuseSurfaces && ( request.nPreviousRegionIndex != request.nRegionIndex || request.nPreviousExtent != extent ) ) ) {
		return false;
	}
	if ( surfaces.pAlbedo == nullptr || surfaces.pPbr == nullptr || surfaces.nAlbedoCapacity < ( uint64_t )extent * extent * 4u ||
	     surfaces.nPbrCapacity < ( uint64_t )( extent / 2u ) * ( extent / 2u ) * 4u ) {
		return false;
	}
	const uint64_t albedoBytes = ( uint64_t )extent * extent * 4u;
	const uint64_t pbrBytes = ( uint64_t )( extent / 2u ) * ( extent / 2u ) * 4u;
	if ( overlaps( surfaces.pAlbedo, albedoBytes, surfaces.pPbr, pbrBytes ) ) {
		return false;
	}
	const sirVirtualTextureCookImage_t& center = request.neighbors[4];
	if ( !validateImage( center ) ) {
		return false;
	}
	for ( uint32_t i = 0u; i < 9u; ++i ) {
		const sirVirtualTextureCookImage_t& neighbor = request.neighbors[i];
		if ( neighbor.pPixels != nullptr && !validateImage( neighbor ) ) {
			return false;
		}
		if ( neighbor.pPixels != nullptr && ( overlaps( neighbor.pPixels, neighbor.nBackingBytes, surfaces.pAlbedo, albedoBytes ) ||
		                                      overlaps( neighbor.pPixels, neighbor.nBackingBytes, surfaces.pPbr, pbrBytes ) ) ) {
			return false;
		}
	}
	return true;
}

float colorTap( const sirVirtualTextureRegionCookRequest_t& request, int x, int z, uint32_t component )
{
	const sirVirtualTextureCookImage_t& center = request.neighbors[4];
	const int dx = x < 0 ? -1 : ( x >= ( int )center.nWidth ? 1 : 0 );
	const int dz = z < 0 ? -1 : ( z >= ( int )center.nHeight ? 1 : 0 );
	const sirVirtualTextureCookImage_t& neighbor = request.neighbors[( dz + 1 ) * 3 + dx + 1];
	const bool useNeighbor = neighbor.pPixels != nullptr && neighbor.nWidth == center.nWidth && neighbor.nHeight == center.nHeight;
	const sirVirtualTextureCookImage_t& source = useNeighbor ? neighbor : center;
	if ( useNeighbor ) {
		x -= dx * ( int )center.nWidth;
		z -= dz * ( int )center.nHeight;
	}
	x = x < 0 ? 0 : ( x >= ( int )source.nWidth ? ( int )source.nWidth - 1 : x );
	z = z < 0 ? 0 : ( z >= ( int )source.nHeight ? ( int )source.nHeight - 1 : z );
	return ( float )source.pPixels[( uint64_t )z * source.nRowBytes + ( uint64_t )x * 4u + component] / 255.0f;
}

float colorSample( const sirVirtualTextureRegionCookRequest_t& request, float u, float v, uint32_t component )
{
	const sirVirtualTextureCookImage_t& center = request.neighbors[4];
	const float fxPos = u * center.nWidth - 0.5f;
	const float fzPos = v * center.nHeight - 0.5f;
	const int ix = ( int )floorf( fxPos );
	const int iz = ( int )floorf( fzPos );
	const float fx = fxPos - ix;
	const float fz = fzPos - iz;
	const float a = colorTap( request, ix, iz, component );
	const float b = colorTap( request, ix + 1, iz, component );
	const float c = colorTap( request, ix, iz + 1, component );
	const float d = colorTap( request, ix + 1, iz + 1, component );
	return ( a + ( b - a ) * fx ) * ( 1.0f - fz ) + ( c + ( d - c ) * fx ) * fz;
}

uint8_t nearestUnorm( float value )
{
	const float clamped = value < 0.0f ? 0.0f : ( value > 1.0f ? 1.0f : value );
	return ( uint8_t )( clamped * 255.0f + 0.5f );
}

uint8_t linearToSrgb( float value )
{
	const float srgb = value <= 0.0031308f ? value * 12.92f : 1.055f * powf( value, 1.0f / 2.4f ) - 0.055f;
	return nearestUnorm( srgb );
}

typedef struct sirVirtualTextureRegionPixelPosition_s
{
	uint32_t x;
	uint32_t z;
	uint32_t channel;
} sirVirtualTextureRegionPixelPosition_t;

void cookPixel( const sirVirtualTextureRegionCookRequest_t& request, uint8_t *pPixel,
                const sirVirtualTextureRegionPixelPosition_t& position )
{
	const uint32_t side = position.channel == 0u ? request.nExtent : request.nExtent / 2u;
	const float u = ( position.x + 0.5f ) / side;
	const float v = ( position.z + 0.5f ) / side;
	if ( position.channel == 0u ) {
		for ( uint32_t component = 0u; component < 3u; ++component ) {
			pPixel[component] = linearToSrgb( colorSample( request, u, v, component ) );
		}
		pPixel[3] = 255u;
	} else {
		pPixel[0] = 128u;
		pPixel[1] = 128u;
		pPixel[2] = nearestUnorm( colorSample( request, u, v, 3u ) );
		pPixel[3] = 255u;
	}
}

bool cookSurface( const sirVirtualTextureRegionCookRequest_t& request, const sirVirtualTextureRegionSurfaces_t& surfaces, uint32_t channel )
{
	const uint32_t side = channel == 0u ? request.nExtent : request.nExtent / 2u;
	const uint32_t pages = request.nExtent / T3D_VT_PAGE_INTERIOR;
	const uint32_t dirty = request.bReuseSurfaces ? request.nDirtyMip0Mask : ( 1u << ( pages * pages ) ) - 1u;
	uint8_t *pPixels = channel == 0u ? surfaces.pAlbedo : surfaces.pPbr;
	for ( uint32_t z = 0u; z < side; ++z ) {
		if ( request.pfnShouldCancel != nullptr && request.pfnShouldCancel( request.pCancelUser ) ) {
			return false;
		}
		for ( uint32_t x = 0u; x < side; ++x ) {
			const uint32_t page =
			    ( z * request.nExtent / side / T3D_VT_PAGE_INTERIOR ) * pages + x * request.nExtent / side / T3D_VT_PAGE_INTERIOR;
			if ( ( dirty & ( 1u << page ) ) != 0u ) {
				cookPixel( request, pPixels + ( ( uint64_t )z * side + x ) * 4u, { x, z, channel } );
			}
		}
	}
	return request.pfnShouldCancel == nullptr || !request.pfnShouldCancel( request.pCancelUser );
}
} // namespace

bool SIRVirtualTextureRegionCooker::compose( const sirVirtualTextureRegionCookRequest_t& request,
                                             const sirVirtualTextureRegionSurfaces_t& surfaces ) noexcept
{
	if ( !validateRegion( request, surfaces ) ) {
		return false;
	}
	return cookSurface( request, surfaces, 0u ) && cookSurface( request, surfaces, 1u );
}

// Transform exactly the donor's native t3dMegaVTCookPixels channel contract;
// the existing Terrain3D V1 cooker still encodes every page and compact tail.
static void setupRegionSource( const sirVirtualTextureRegionCookRequest_t& region, const sirVirtualTextureRegionSurfaces_t& surfaces,
                               sirVirtualTextureCookSource_t& source )
{
	source = {};
	const uint32_t extent = region.nExtent;
	source.nStableKey = t3d_vt_terrain_region_stable_key( region.nRegionIndex );
	source.nGenerationSeed = region.nGeneration;
	source.nConsumer = T3D_VT_CONSUMER_CLIPMAP;
	source.nWrapMode = T3D_VT_COOK_WRAP_CLAMP;
	source.nAllocationFlags = T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP | T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
	source.nAuthoredChannelMask =
	    T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR );
	source.images[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = { surfaces.pAlbedo, extent, extent, 4u, extent * 4u, surfaces.nAlbedoCapacity };
	source.images[T3D_VT_LOGICAL_CHANNEL_PBR] = { surfaces.pPbr, extent / 2u, extent / 2u, 4u, extent * 2u, surfaces.nPbrCapacity };
	source.nSourceFingerprint[T3D_VT_LOGICAL_CHANNEL_ALBEDO] = t3dHashPublicationBytes( surfaces.pAlbedo, ( size_t )extent * extent * 4u );
	source.nSourceFingerprint[T3D_VT_LOGICAL_CHANNEL_PBR] =
	    t3dHashPublicationBytes( surfaces.pPbr, ( size_t )( extent / 2u ) * ( extent / 2u ) * 4u );
}

bool SIRVirtualTextureRegionCooker::cook( const sirVirtualTextureRegionPackRequest_t& request,
                                          sirVirtualTextureCookOutput_t& package ) noexcept
{
	package.nWritten = 0u;
	if ( !compose( request.region, request.surfaces ) ) {
		return false;
	}
	sirVirtualTextureCookSource_t source{};
	setupRegionSource( request.region, request.surfaces, source );
	const sirVirtualTextureCookBatch_t batch{ &source, 1u, request.allocator, request.bZstd };
	return SIRVirtualTextureCooker::cook( batch, package );
}

bool SIRVirtualTextureRegionCooker::cookBatch( const sirVirtualTextureRegionBatchRequest_t& request,
                                               sirVirtualTextureCookOutput_t& package ) noexcept
{
	package.nWritten = 0u;
	if ( request.pRegions == nullptr || request.pSurfaces == nullptr || request.pCookScratch == nullptr || request.nRegionCount == 0u ||
	     request.nRegionCount > 32u || request.nSourceCapacity < request.nRegionCount || request.pfnIsCurrent == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0u; i < request.nRegionCount; ++i ) {
		const sirVirtualTextureRegionCookRequest_t& region = request.pRegions[i];
		if ( !request.pfnIsCurrent( request.pCurrentUser, region.nRegionIndex, region.nGeneration ) ) {
			return false;
		}
		for ( uint32_t j = 0u; j < i; ++j ) {
			if ( request.pRegions[j].nRegionIndex == region.nRegionIndex ) {
				return false;
			}
		}
		if ( !compose( region, request.pSurfaces[i] ) ) {
			return false;
		}
		setupRegionSource( region, request.pSurfaces[i], request.pCookScratch[i] );
	}
	const sirVirtualTextureCookBatch_t batch{ request.pCookScratch, request.nRegionCount, request.allocator, request.bZstd };
	if ( !SIRVirtualTextureCooker::cook( batch, package ) ) {
		return false;
	}
	for ( uint32_t i = 0u; i < request.nRegionCount; ++i ) {
		const sirVirtualTextureRegionCookRequest_t& region = request.pRegions[i];
		if ( !request.pfnIsCurrent( request.pCurrentUser, region.nRegionIndex, region.nGeneration ) ) {
			package.nWritten = 0u;
			return false;
		}
	}
	return true;
}
