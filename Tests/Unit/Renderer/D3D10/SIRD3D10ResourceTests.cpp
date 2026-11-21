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

#include <Renderer/Drivers/D3D10/SIRD3D10Resources.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <Renderer/Drivers/D3D10/SIRD3D10Formats.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static void sirD3D10Require( bool bCondition, const char *pszMsg ) noexcept
{
	if ( !bCondition ) {
		std::fprintf( stderr, "FAIL: %s\n", pszMsg );
		abort();
	}
	else {
		std::printf( "PASS: %s\n", pszMsg );
	}
}

int main( void )
{
	// These tests require a native D3D10 device - in CI/headless they will be skipped
	// For now, we test the validation logic with mock devices

	std::printf( "D3D10 Resource validation tests (CPU mock)\n" );

	// Test buffer handle validation with invalid generation
	{
		sirGraphicsHandle_t hBuffer = {};
		hBuffer.nSlot = 0;
		hBuffer.nGeneration = 1;
		hBuffer.nDeviceGeneration = 2; // Mismatch
		hBuffer.eType = SIR_GRAPHICS_HANDLE_BUFFER;

		sirD3D10Buffer_t *pBuffer = nullptr;
		sirGraphicsResult_t eResult = sirD3D10ValidateBufferHandle( 1, hBuffer, &pBuffer );
		sirD3D10Require( eResult == SIR_GRAPHICS_INVALID_HANDLE, "stale device generation rejected" );
	}

	// Test buffer handle validation with invalid type
	{
		sirGraphicsHandle_t hBuffer = {};
		hBuffer.nSlot = 0;
		hBuffer.nGeneration = 1;
		hBuffer.nDeviceGeneration = 1;
		hBuffer.eType = SIR_GRAPHICS_HANDLE_TEXTURE; // Wrong type

		sirD3D10Buffer_t *pBuffer = nullptr;
		sirGraphicsResult_t eResult = sirD3D10ValidateBufferHandle( 1, hBuffer, &pBuffer );
		sirD3D10Require( eResult == SIR_GRAPHICS_INVALID_HANDLE, "wrong handle type rejected" );
	}

	// Test texture handle validation
	{
		sirGraphicsHandle_t hTexture = {};
		hTexture.nSlot = 0;
		hTexture.nGeneration = 1;
		hTexture.nDeviceGeneration = 1;
		hTexture.eType = SIR_GRAPHICS_HANDLE_TEXTURE;

		sirD3D10Texture_t *pTexture = nullptr;
		sirGraphicsResult_t eResult = sirD3D10ValidateTextureHandle( 1, hTexture, &pTexture );
		sirD3D10Require( eResult == SIR_GRAPHICS_INVALID_HANDLE, "unallocated texture rejected" );
	}

	// Test view handle validation
	{
		sirGraphicsHandle_t hView = {};
		hView.nSlot = 0;
		hView.nGeneration = 1;
		hView.nDeviceGeneration = 1;
		hView.eType = SIR_GRAPHICS_HANDLE_VIEW;

		sirD3D10View_t *pView = nullptr;
		sirGraphicsResult_t eResult = sirD3D10ValidateViewHandle( 1, hView, &pView );
		sirD3D10Require( eResult == SIR_GRAPHICS_INVALID_HANDLE, "unallocated view rejected" );
	}

	// Test sampler handle validation
	{
		sirGraphicsHandle_t hSampler = {};
		hSampler.nSlot = 0;
		hSampler.nGeneration = 1;
		hSampler.nDeviceGeneration = 1;
		hSampler.eType = SIR_GRAPHICS_HANDLE_SAMPLER;

		sirD3D10Sampler_t *pSampler = nullptr;
		sirGraphicsResult_t eResult = sirD3D10ValidateSamplerHandle( 1, hSampler, &pSampler );
		sirD3D10Require( eResult == SIR_GRAPHICS_INVALID_HANDLE, "unallocated sampler rejected" );
	}

	// Test format validation
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.nSampleCount = 1;
		desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		support.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		support.nDimensionMask = ( 1U << ( SIR_GRAPHICS_TEXTURE_1D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_3D - 1U ) ) |
		                         ( 1U << ( SIR_GRAPHICS_TEXTURE_CUBE - 1U ) );
		support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT |
		                           SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
		support.nSampleCountMask = 0x1F;

		sirGraphicsResult_t eResult = sirD3D10ValidateTextureSupport( &desc, &support );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "valid texture desc accepted" );
	}

	// Test invalid texture usage
	{
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 256;
		desc.nHeight = 256;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 1;
		desc.nSampleCount = 1;
		desc.nUsage = SIR_GRAPHICS_USAGE_STORAGE; // Not supported in D3D10
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
		desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;

		sirD3D10FormatSupport_t support = {};
		support.nSize = sizeof( support );
		support.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
		support.nDimensionMask = ( 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U ) );
		support.nSupportedUsages = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
		support.nSampleCountMask = 0x01;

		sirGraphicsResult_t eResult = sirD3D10ValidateTextureSupport( &desc, &support );
		sirD3D10Require( eResult == SIR_GRAPHICS_UNSUPPORTED, "unsupported usage rejected" );
	}

	std::printf( "\nAll D3D10 Resource validation tests passed!\n" );
	return 0;
}