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

#include <Renderer/Drivers/D3D11/SIRD3D11Formats.h>
#include <stdio.h>

static uint32_t s_nChecks = 0;
static uint32_t s_nFailures = 0;

static void sirD3D11Check( bool bGood, const char *pszCase ) noexcept
{
	++s_nChecks;
	if ( !bGood ) {
		++s_nFailures;
		fprintf( stderr, "D3D11 format validation: %s\n", pszCase );
	}
}

int main( void )
{
	sirD3D11FormatSupport_t support = {};
	support.nSize = sizeof( support );
	support.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	support.nDimensionMask = 0xF;
	support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_COPY_SOURCE |
	                           SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	support.nSampleCountMask = 0x7;
	sirGraphicsTextureDesc_t desc = {};
	desc.nWidth = 128;
	desc.nHeight = 128;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 1;
	desc.nSampleCount = 1;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;

	sirD3D11Check( sirD3D11GetMaxSampleCount( &support ) == 4, "sample mask maximum" );
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "valid color texture" );
	sirD3D11Check( sirD3D11ValidateTextureSupport( nullptr, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing descriptor" );
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "missing support" );
	support.nSize = sizeof( support ) - 1;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "bad support size" );
	sirD3D11Check( sirD3D11GetMaxSampleCount( &support ) == 0, "invalid support mask" );
	support.nSize = sizeof( support );
	sirD3D11Check( sirD3D11GetMaxSampleCount( nullptr ) == 0, "null sample cap" );
	desc.nWidth = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero width" );
	desc.nWidth = 128;
	desc.nArrayLayers = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero layers" );
	desc.nArrayLayers = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "buffer usage is not texture" );
	desc.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_UNSUPPORTED, "UAV not advertised" );
	desc.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "RTV and DSV conflict" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.nSampleCount = 3;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "non power of two samples" );
	desc.nSampleCount = 8;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_UNSUPPORTED, "unavailable 8x" );
	desc.nSampleCount = 4;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "available 4x" );
	desc.nMipLevels = 2;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA with mipmaps" );
	desc.nSampleCount = 1;
	desc.nMipLevels = 9;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "excess mip count" );
	desc.nMipLevels = 8;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "full mip chain" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	desc.nArrayLayers = 6;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "cube array layers" );
	desc.nArrayLayers = 7;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "cube layers not multiple of six" );
	desc.nArrayLayers = 6;
	desc.nHeight = 64;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "cube not square" );
	desc.nHeight = 128;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "1D height not one" );
	desc.nHeight = 1;
	desc.nArrayLayers = 1;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "valid 1D" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	desc.nHeight = 128;
	desc.nDepth = 4;
	desc.nArrayLayers = 2;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "3D arrays forbidden" );
	desc.nArrayLayers = 1;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "valid 3D" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nDepth = 1;
	desc.eFormat = SIR_GRAPHICS_FORMAT_R8_UNORM;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "format mismatch" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	support.nDimensionMask = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_UNSUPPORTED, "unsupported 2D" );
	support.nDimensionMask = 0xF;
	support.nSampleCountMask = 0;
	sirD3D11Check( sirD3D11GetMaxSampleCount( &support ) == 0, "no sample support" );
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_UNSUPPORTED, "unsupported sample" );

	// Input-domain and short-circuit adversarial cases, including paths that
	// must reject before asking a native device about format support.
	support.nDimensionMask = 0xF;
	support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_COPY_SOURCE |
	                           SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	support.nSampleCountMask = 0x7;
	desc = {};
	desc.nWidth = 128;
	desc.nHeight = 128;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 1;
	desc.nSampleCount = 1;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.nHeight = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero height" );
	desc.nHeight = 128;
	desc.nDepth = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero depth" );
	desc.nDepth = 1;
	desc.nMipLevels = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero mip chain" );
	desc.nMipLevels = 1;
	desc.nSampleCount = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero samples" );
	desc.nSampleCount = 1;
	desc.nUsage = 0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero usage" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.eDimension = ( sirGraphicsTextureDimension_t )0;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid dimension low" );
	desc.eDimension = ( sirGraphicsTextureDimension_t )5;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid dimension high" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nSampleCount = 64;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "sample count cap" );
	desc.nSampleCount = 1;
	desc.eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unknown format" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_UNSUPPORTED, "depth usage unavailable" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.nSampleCount = 2;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA 1D forbidden" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "MSAA UAV forbidden" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.nSampleCount = 1;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	desc.nHeight = 1;
	desc.nDepth = 2;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "1D depth not one" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nHeight = 128;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT, "2D depth not one" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	desc.nDepth = 512;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "3D depth largest" );
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.nDepth = 1;
	desc.nWidth = 256;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS, "width largest" );
	support.nSampleCountMask = 1;
	sirD3D11Check( sirD3D11GetMaxSampleCount( &support ) == 1, "1x sample limit" );
	support.nSampleCountMask = 1U << 5;
	sirD3D11Check( sirD3D11GetMaxSampleCount( &support ) == 32, "32x sample limit" );

	printf( "D3D11 format validation: %u assertions, %u failures\n", s_nChecks, s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
