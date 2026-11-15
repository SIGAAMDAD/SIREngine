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

#include <Renderer/Drivers/D3D11/SIRD3D11Driver.h>
#include <Renderer/Drivers/D3D11/SIRD3D11Formats.h>
#include <stdio.h>

static uint32_t s_nChecks = 0;
static uint32_t s_nFailures = 0;

static void sirD3D11Check( bool bValid, const char *pszLabel ) noexcept
{
	++s_nChecks;
	if ( !bValid ) {
		fprintf( stderr, "D3D11 format probe failure: %s\n", pszLabel );
		++s_nFailures;
	}
}

int main( void )
{
	SIRD3D11Driver driver;
	uint32_t nAdapters = 0;
	sirD3D11Check( driver.getAdapterCount( &nAdapters ) == SIR_GRAPHICS_SUCCESS && nAdapters > 0, "adapter count" );
	uint32_t nWarpIndex = UINT32_MAX;
	for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
		sirGraphicsAdapterInfo_t adapter = {};
		if ( driver.queryAdapter( iAdapter, &adapter ) == SIR_GRAPHICS_SUCCESS && adapter.bSoftwareAdapter ) {
			nWarpIndex = iAdapter;
			break;
		}
	}
	sirD3D11Check( nWarpIndex != UINT32_MAX, "WARP present" );
	if ( nWarpIndex == UINT32_MAX ) {
		return 1;
	}
	sirGraphicsDeviceCreateDesc_t desc = {};
	desc.nAdapterIndex = nWarpIndex;
	desc.nFramesInFlight = 2;
	desc.bAllowSoftware = true;
	SIRRenderingDevice *pDevice = nullptr;
	sirD3D11Check( driver.createDevice( desc, &pDevice ) == SIR_GRAPHICS_SUCCESS && pDevice != nullptr, "WARP device" );
	if ( pDevice == nullptr ) {
		return 1;
	}
	SIRD3D11Device *pNative = static_cast<SIRD3D11Device *>( pDevice );
	sirD3D11FormatSupport_t support = {};
	support.nSize = sizeof( support );
	sirD3D11Check( pNative->queryNativeFormatSupport( SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &support ) == SIR_GRAPHICS_SUCCESS,
	               "RGBA8 support probe" );
	sirD3D11Check( support.eFormat == SIR_GRAPHICS_FORMAT_RGBA8_UNORM, "RGBA8 identity" );
	sirD3D11Check( ( support.nDimensionMask & ( 1U << ( SIR_GRAPHICS_TEXTURE_2D - 1U ) ) ) != 0, "RGBA8 2D" );
	sirD3D11Check( ( support.nSupportedUsages & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0, "RGBA8 RTV" );
	sirD3D11Check( ( support.nSupportedUsages & SIR_GRAPHICS_USAGE_SAMPLED ) != 0, "RGBA8 SRV" );
	sirD3D11Check( ( support.nSampleCountMask & 1U ) != 0, "RGBA8 1x" );
	sirGraphicsCapabilities_t capabilities = {};
	sirD3D11Check( pDevice->queryCapabilities( &capabilities ) == SIR_GRAPHICS_SUCCESS, "native caps" );
	sirD3D11Check( capabilities.nMaximumSampleCount == sirD3D11GetMaxSampleCount( &support ), "actual MSAA limit" );
	sirGraphicsABIPipelineModelCaps_t model = {};
	model.nSize = sizeof( model );
	sirD3D11Check( pDevice->queryPipelineModelCaps( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE, &model ) == SIR_GRAPHICS_SUCCESS,
	               "native model caps" );
	sirD3D11Check( model.nMaximumSamples == capabilities.nMaximumSampleCount, "model MSAA matches native" );
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 512;
	texture.nHeight = 512;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &texture, &support ) == SIR_GRAPHICS_SUCCESS, "RGBA8 color admit" );
	texture.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	sirD3D11Check( sirD3D11ValidateTextureSupport( &texture, &support ) == SIR_GRAPHICS_UNSUPPORTED, "RGBA8 depth reject" );
	sirD3D11Check( pNative->queryNativeFormatSupport( SIR_GRAPHICS_FORMAT_UNKNOWN, &support ) == SIR_GRAPHICS_UNSUPPORTED,
	               "unknown DXGI map" );
	support.nSize = sizeof( support ) - 1;
	sirD3D11Check( pNative->queryNativeFormatSupport( SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &support ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	               "size validation" );
	support.nSize = sizeof( support );
	const sirGraphicsFormat_t szFormats[] = {
	    SIR_GRAPHICS_FORMAT_RGBA8_UNORM,       SIR_GRAPHICS_FORMAT_BGRA8_UNORM,  SIR_GRAPHICS_FORMAT_RGBA8_SRGB,
	    SIR_GRAPHICS_FORMAT_RGBA16_FLOAT,      SIR_GRAPHICS_FORMAT_R8_UNORM,     SIR_GRAPHICS_FORMAT_RG8_UNORM,
	    SIR_GRAPHICS_FORMAT_R16_FLOAT,         SIR_GRAPHICS_FORMAT_R32_FLOAT,    SIR_GRAPHICS_FORMAT_R32_UINT,
	    SIR_GRAPHICS_FORMAT_RG32_FLOAT,        SIR_GRAPHICS_FORMAT_RGBA32_FLOAT, SIR_GRAPHICS_FORMAT_D16_UNORM,
	    SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT, SIR_GRAPHICS_FORMAT_D32_FLOAT,    SIR_GRAPHICS_FORMAT_BC1_UNORM,
	    SIR_GRAPHICS_FORMAT_BC3_UNORM,         SIR_GRAPHICS_FORMAT_BC5_UNORM,    SIR_GRAPHICS_FORMAT_BC7_UNORM };
	for ( uint32_t i = 0; i < 18; ++i ) {
		support = {};
		support.nSize = sizeof( support );
		const sirGraphicsResult_t eResult = pNative->queryNativeFormatSupport( szFormats[i], &support );
		sirD3D11Check( eResult == SIR_GRAPHICS_SUCCESS || eResult == SIR_GRAPHICS_UNSUPPORTED, "mapped format probe" );
		if ( eResult == SIR_GRAPHICS_SUCCESS ) {
			sirD3D11Check( support.eFormat == szFormats[i], "native format identity" );
		}
	}
	support = {};
	support.nSize = sizeof( support );
	sirD3D11Check( pNative->queryNativeFormatSupport( SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT, &support ) == SIR_GRAPHICS_SUCCESS,
	               "D24 depth format probe" );
	sirD3D11Check( ( support.nSupportedUsages & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0, "D24 DSV admission" );

	// Native WARP ownership and resource creation. These assertions run only
	// under a real Windows D3D11 runtime; portable tests cannot replace them.
	sirGraphicsMemoryBudget_t budgetBefore = {};
	sirD3D11Check( pDevice->queryMemoryBudget( &budgetBefore ) == SIR_GRAPHICS_SUCCESS, "pre-texture native budget" );
	sirGraphicsTextureDesc_t nativeTexture = {};
	nativeTexture.nWidth = 32;
	nativeTexture.nHeight = 32;
	nativeTexture.nDepth = 1;
	nativeTexture.nArrayLayers = 1;
	nativeTexture.nMipLevels = 1;
	nativeTexture.nSampleCount = 1;
	nativeTexture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	nativeTexture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	nativeTexture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	nativeTexture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hTexture = {};
	sirD3D11Check( pDevice->createTexture( nativeTexture, &hTexture ) == SIR_GRAPHICS_SUCCESS, "native RGBA8 texture" );
	if ( hTexture.nGeneration != 0 ) {
		sirGraphicsMemoryBudget_t budgetDuring = {};
		sirD3D11Check( pDevice->queryMemoryBudget( &budgetDuring ) == SIR_GRAPHICS_SUCCESS, "native texture budget" );
		sirD3D11Check( budgetDuring.nCommittedBytes == budgetBefore.nCommittedBytes + 4096, "texture logical accounting" );
		sirGraphicsViewDesc_t view = {};
		view.hResource = hTexture;
		view.eFormat = nativeTexture.eFormat;
		view.nMipLevels = 1;
		view.nLayers = 1;
		view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
		sirGraphicsHandle_t hSampled = {};
		sirD3D11Check( pDevice->createView( view, &hSampled ) == SIR_GRAPHICS_SUCCESS, "native sampled view" );
		view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
		sirGraphicsHandle_t hRenderTarget = {};
		sirD3D11Check( pDevice->createView( view, &hRenderTarget ) == SIR_GRAPHICS_SUCCESS, "native RTV" );
		view.nBaseMip = 1;
		sirGraphicsHandle_t hInvalid = {};
		sirD3D11Check( pDevice->createView( view, &hInvalid ) == SIR_GRAPHICS_INVALID_ARGUMENT && hInvalid.nGeneration == 0,
		               "invalid RTV subresource cannot allocate" );
		sirGraphicsCompletionSet_t empty = {};
		empty.nDeviceGeneration = pDevice->getDeviceGeneration();
		if ( hRenderTarget.nGeneration != 0 ) {
			sirD3D11Check( pDevice->retireResource( hRenderTarget, empty ) == SIR_GRAPHICS_SUCCESS, "release native RTV" );
		}
		if ( hSampled.nGeneration != 0 ) {
			sirD3D11Check( pDevice->retireResource( hSampled, empty ) == SIR_GRAPHICS_SUCCESS, "release native SRV" );
		}
		sirD3D11Check( pDevice->retireResource( hTexture, empty ) == SIR_GRAPHICS_SUCCESS, "release native texture" );
		sirD3D11Check( pDevice->retireResource( hTexture, empty ) == SIR_GRAPHICS_INVALID_HANDLE, "double retire denied" );
		sirGraphicsMemoryBudget_t budgetAfter = {};
		sirD3D11Check( pDevice->queryMemoryBudget( &budgetAfter ) == SIR_GRAPHICS_SUCCESS, "post-texture native budget" );
		sirD3D11Check( budgetAfter.nCommittedBytes == budgetBefore.nCommittedBytes, "native texture accounting restored" );
	}
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
	sampler.eAddressW = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.fMaximumAnisotropy = 1.0f;
	sampler.fMinimumLod = 0.0f;
	sampler.fMaximumLod = 16.0f;
	sirGraphicsHandle_t hSampler = {};
	sirD3D11Check( pDevice->createSampler( sampler, &hSampler ) == SIR_GRAPHICS_SUCCESS, "native sampler state" );
	if ( hSampler.nGeneration != 0 ) {
		sirGraphicsCompletionSet_t empty = {};
		empty.nDeviceGeneration = pDevice->getDeviceGeneration();
		sirD3D11Check( pDevice->retireResource( hSampler, empty ) == SIR_GRAPHICS_SUCCESS, "release native sampler" );
	}
	sampler.fMaximumAnisotropy = 0.0f;
	hSampler = {};
	sirD3D11Check( pDevice->createSampler( sampler, &hSampler ) == SIR_GRAPHICS_INVALID_ARGUMENT && hSampler.nGeneration == 0,
	               "invalid sampler rejected before COM create" );
	sirD3D11Check( driver.destroyDevice( pDevice ) == SIR_GRAPHICS_SUCCESS, "destroy WARP" );
	printf( "D3D11 WARP format probe: %u assertions, %u failures, max RGBA8 samples=%u\n", s_nChecks, s_nFailures,
	        capabilities.nMaximumSampleCount );
	return s_nFailures == 0 ? 0 : 1;
}
