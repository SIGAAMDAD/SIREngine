/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/D3D10/SIRD3D10Formats.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

static void sirD3D10Require( bool bCondition ) noexcept
{
	if ( !bCondition ) {
		abort();
	}
}

// Mock format support for CPU testing
static sirGraphicsResult_t sirD3D10MockQueryFormatSupport( sirGraphicsFormat_t eFormat, sirD3D10FormatSupport_t *pSupport ) noexcept
{
	if ( pSupport == nullptr || pSupport->nSize < sizeof( *pSupport ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirD3D10FormatSupport_t support = {};
	support.nSize = sizeof( support );
	support.eFormat = eFormat;

	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		support.nDimensionMask = ( 1U << ( SIR_GRAPHICS_TEXTURE_1D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_3D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_CUBE - 1U ) );
		support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT |
		                           SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
		support.nSampleCountMask = 0x1F; // 1x, 2x, 4x, 8x, 16x
		break;
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		support.nDimensionMask = ( 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U ) );
		support.nSupportedUsages = SIR_GRAPHICS_USAGE_DEPTH_STENCIL | SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
		support.nSampleCountMask = 0x1F;
		break;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
	case SIR_GRAPHICS_FORMAT_BC5_UNORM:
		support.nDimensionMask = ( 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_CUBE - 1U ) );
		support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
		support.nSampleCountMask = 0x01; // No multisample for BC
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	*pSupport = support;
	return SIR_GRAPHICS_SUCCESS;
}

int main( void )
{
	// Test valid texture descriptions
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.nSampleCount = 1;
		desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS );
	}

	// Test 3D texture
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 64;
		desc.nHeight = 64;
		desc.nDepth = 64;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 6;
		desc.nSampleCount = 1;
		desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_3D;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS );
	}

	// Test cube texture
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 6;
		desc.nMipLevels = 8;
		desc.nSampleCount = 1;
		desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS );
	}

	// Test multisample texture
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 512;
		desc.nHeight = 512;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1; // Must be 1 for multisample
		desc.nSampleCount = 4;
		desc.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_SUCCESS );
	}

	// Test negative: zero dimensions
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 0;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test negative: multisample with mips
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 4; // Invalid with multisample
		desc.nSampleCount = 4;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test negative: multisample on 3D texture
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 64;
		desc.nHeight = 64;
		desc.nDepth = 64;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.nSampleCount = 4; // Invalid for 3D
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_3D;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test negative: cube array not multiple of 6
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 5; // Invalid - must be multiple of 6
		desc.nMipLevels = 1;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( sirD3D10ValidateTextureSupport( &desc, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test negative: unsupported format
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.eFormat = ( sirGraphicsFormat_t )999; // Invalid format
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		sirD3D10Require( sirD3D10MockQueryFormatSupport( desc.eFormat, &support ) == SIR_GRAPHICS_UNSUPPORTED );
	}

	// Test max sample count helper
	{
		sirD3D10FormatSupport_t support = {};
		support.nSampleCountMask = 0x1F; // 1x through 16x
		sirD3D10Require( sirD3D10GetMaxSampleCount( &support ) == 16 );
	}

	return 0;
}