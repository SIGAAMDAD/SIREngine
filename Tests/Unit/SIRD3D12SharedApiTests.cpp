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

#include <Renderer/Drivers/D3D12/SIRD3D12Driver.h>
#include <Renderer/SIRGraphicsABIOperations.h>
#include <Renderer/SIRGraphicsRegistry.h>
#include <stdio.h>
#include <string.h>

static_assert( sizeof( sirGraphicsDeviceCreateDesc_t ) == 48, "D3D12 requires the shared 48-byte descriptor ABI" );
static_assert( sizeof( sirGraphicsHandle_t ) == 16, "Graphics handles must stay 16 bytes" );
static_assert( sizeof( sirGraphicsCompletion_t ) == 16, "Graphics completion tokens must stay 16 bytes" );

int main( void )
{
	SIRD3D12Driver driver;
	SIRGraphicsRegistry registry;
	if ( registry.registerDriver( &driver ) != SIR_GRAPHICS_SUCCESS || registry.findDriver( SIR_GRAPHICS_BACKEND_D3D12 ) != &driver ) {
		return 1;
	}

	uint32_t nAdapters = 0;
	if ( driver.getAdapterCount( &nAdapters ) != SIR_GRAPHICS_SUCCESS || nAdapters == 0 || nAdapters > 257 ) {
		return 2;
	}
	uint32_t nSoftwareIndex = UINT32_MAX;
	uint64_t szLuids[257] = {};
	for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS || info.nAdapterIndex != iAdapter ||
		     info.nApiVersion.nMajor != 12 ) {
			return 3;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iAdapter; ++iPrevious ) {
			if ( szLuids[iPrevious] == info.nAdapterLuid ) {
				return 4;
			}
		}
		szLuids[iAdapter] = info.nAdapterLuid;
		if ( info.bSoftwareAdapter ) {
			if ( iAdapter != nAdapters - 1 || nSoftwareIndex != UINT32_MAX ) {
				return 4;
			}
			nSoftwareIndex = iAdapter;
		}
	}
	sirGraphicsAdapterInfo_t invalid = {};
	if ( nSoftwareIndex != nAdapters - 1 || driver.queryAdapter( nAdapters, &invalid ) != SIR_GRAPHICS_UNSUPPORTED ||
	     driver.queryAdapter( 0, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 4;
	}
	sirGraphicsSelection_t selection = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	selection.nMinimumApiVersion = { 12, 0, 0 };
	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	selection.bAllowSoftware = true;
	SIRGraphicsDriver *pSelected = nullptr;
	sirGraphicsAdapterInfo_t selectedInfo = {};
	if ( registry.selectDriver( selection, &pSelected, &selectedInfo ) != SIR_GRAPHICS_SUCCESS || pSelected != &driver ) {
		return 5;
	}

	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nSoftwareIndex;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bHeadless = true;
	config.bAllowSoftware = true;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 6;
	}
	sirD3D12AdapterInfo_t active = {};
	sirGraphicsAdapterInfo_t expected = {};
	if ( driver.queryAdapter( nSoftwareIndex, &expected ) != SIR_GRAPHICS_SUCCESS ||
	     static_cast<SIRD3D12RenderingDevice *>( pDevice )->queryNativeAdapterInfo( &active ) != SIR_GRAPHICS_SUCCESS ||
	     ( uint64_t )active.nAdapterLuid != expected.nAdapterLuid || !active.bSoftware ) {
		return 28;
	}
	SIRRenderingDevice *pDuplicate = pDevice;
	if ( driver.createDevice( config, &pDuplicate ) != SIR_GRAPHICS_BUSY || pDuplicate != nullptr ) {
		return 7;
	}
	sirGraphicsCapabilities_t capabilities = {};
	if ( pDevice->getBackend() != SIR_GRAPHICS_BACKEND_D3D12 || pDevice->queryCapabilities( &capabilities ) != SIR_GRAPHICS_SUCCESS ||
	     capabilities.nDeviceGeneration == 0 || ( capabilities.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_COPY ) ) == 0 ||
	     ( capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_INDIRECT_DRAW ) != 0 ) {
		return 8;
	}
	if ( capabilities.nMaximumSampleCount == 0 || capabilities.nMaximumSampleCount > 32 ||
	     ( capabilities.nMaximumSampleCount & ( capabilities.nMaximumSampleCount - 1 ) ) != 0 ) {
		return 31;
	}
	sirGraphicsABIPipelineModelCaps_t modelCaps = {};
	if ( pDevice->queryPipelineModelCaps( SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &modelCaps ) != SIR_GRAPHICS_SUCCESS ||
	     modelCaps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN || modelCaps.nDeviceGeneration != capabilities.nDeviceGeneration ||
	     modelCaps.nShaderFormatMask != ( 1U << ( SIR_GRAPHICS_SHADER_DXIL - 1 ) ) ||
	     ( modelCaps.nShaderStageMask & ( SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE ) ) !=
	         ( SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE ) ) {
		return 29;
	}
	if ( pDevice->queryPipelineModelCaps( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION, &modelCaps ) != SIR_GRAPHICS_UNSUPPORTED ||
	     pDevice->queryPipelineModelCaps( SIR_GRAPHICS_PIPELINE_MODEL_MODERN, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 30;
	}
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = capabilities.nDeviceGeneration;
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 4096;
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t handle = {};
	if ( pDevice->createBuffer( buffer, &handle ) != SIR_GRAPHICS_SUCCESS || handle.nGeneration == 0 ||
	     handle.nDeviceGeneration != capabilities.nDeviceGeneration ) {
		return 9;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = handle;
	view.eFormat = SIR_GRAPHICS_FORMAT_R32_UINT;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirGraphicsHandle_t hView = {};
	if ( pDevice->createView( view, &hView ) != SIR_GRAPHICS_SUCCESS || pDevice->retireResource( handle, immediate ) != SIR_GRAPHICS_BUSY ||
	     pDevice->retireResource( hView, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( handle, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 14;
	}
	sirGraphicsHandle_t reused = {};
	if ( pDevice->createBuffer( buffer, &reused ) != SIR_GRAPHICS_SUCCESS || reused.nSlot != handle.nSlot ||
	     reused.nGeneration == handle.nGeneration || pDevice->retireResource( handle, immediate ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     pDevice->retireResource( reused, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 15;
	}

	buffer.nUsage = SIR_GRAPHICS_USAGE_CONSTANT | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t upload = {};
	sirGraphicsMappedBuffer_t mapped = {};
	if ( pDevice->createBuffer( buffer, &upload ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->mapBuffer( upload, 128, 64, &mapped ) != SIR_GRAPHICS_SUCCESS || mapped.pData == nullptr ) {
		return 16;
	}
	memset( mapped.pData, 0x5A, 64 );
	if ( pDevice->flushBuffer( upload, 128, 64 ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->mapBuffer( upload, 4096, 1, &mapped ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 17;
	}
	view = {};
	view.hResource = upload;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_CONSTANT;
	sirGraphicsHandle_t cbv = {};
	if ( pDevice->createView( view, &cbv ) != SIR_GRAPHICS_SUCCESS || pDevice->retireResource( cbv, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( upload, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 18;
	}

	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 32;
	texture.nHeight = 32;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hTexture = {};
	if ( pDevice->createTexture( texture, &hTexture ) != SIR_GRAPHICS_SUCCESS ) {
		return 19;
	}
	view = {};
	view.hResource = hTexture;
	view.eFormat = texture.eFormat;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirGraphicsHandle_t rtv = {};
	if ( pDevice->createView( view, &rtv ) != SIR_GRAPHICS_SUCCESS || pDevice->retireResource( rtv, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hTexture, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 20;
	}

	texture.nSampleCount = 64;
	sirGraphicsHandle_t hUnsupportedMsaa = {};
	if ( pDevice->createTexture( texture, &hUnsupportedMsaa ) != SIR_GRAPHICS_UNSUPPORTED || hUnsupportedMsaa.nGeneration != 0 ) {
		return 32;
	}
	if ( capabilities.nMaximumSampleCount > 1 ) {
		texture.nSampleCount = capabilities.nMaximumSampleCount;
		texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
		sirGraphicsHandle_t hMsaa = {};
		if ( pDevice->createTexture( texture, &hMsaa ) != SIR_GRAPHICS_SUCCESS ||
		     pDevice->retireResource( hMsaa, immediate ) != SIR_GRAPHICS_SUCCESS ) {
			return 33;
		}
		texture.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
		if ( pDevice->createTexture( texture, &hUnsupportedMsaa ) != SIR_GRAPHICS_UNSUPPORTED || hUnsupportedMsaa.nGeneration != 0 ) {
			return 34;
		}
	}
	texture.nSampleCount = 1;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_SRGB;
	texture.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	if ( pDevice->createTexture( texture, &hUnsupportedMsaa ) != SIR_GRAPHICS_UNSUPPORTED || hUnsupportedMsaa.nGeneration != 0 ) {
		return 35;
	}
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	sirGraphicsHandle_t hStorageTexture = {};
	if ( pDevice->createTexture( texture, &hStorageTexture ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hStorageTexture, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 36;
	}
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	sampler.fMaximumAnisotropy = 1.0f;
	sampler.fMaximumLod = 16.0f;
	sirGraphicsHandle_t hSampler = {};
	if ( pDevice->createSampler( sampler, &hSampler ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hSampler, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 21;
	}
	// Test fixed descriptor capacity and exact generation reclamation.
	sirGraphicsSamplerDesc_t samplerMany = {};
	samplerMany.eFilter = SIR_GRAPHICS_FILTER_NEAREST;
	samplerMany.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	samplerMany.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
	samplerMany.eAddressW = SIR_GRAPHICS_ADDRESS_CLAMP;
	samplerMany.fMaximumAnisotropy = 1.0f;
	samplerMany.fMinimumLod = 0.0f;
	samplerMany.fMaximumLod = 4.0f;
	sirGraphicsHandle_t szSamplers[128] = {};
	for ( uint32_t iSampler = 0; iSampler < 128; ++iSampler ) {
		if ( pDevice->createSampler( samplerMany, &szSamplers[iSampler] ) != SIR_GRAPHICS_SUCCESS ) {
			return 22;
		}
	}
	sirGraphicsHandle_t hExcess = {};
	if ( pDevice->createSampler( samplerMany, &hExcess ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || hExcess.nGeneration != 0 ) {
		return 23;
	}
	for ( uint32_t iSampler = 0; iSampler < 128; ++iSampler ) {
		if ( pDevice->retireResource( szSamplers[iSampler], immediate ) != SIR_GRAPHICS_SUCCESS ) {
			return 24;
		}
	}
	if ( pDevice->createSampler( samplerMany, &hExcess ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hExcess, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 25;
	}
	// Invalid completion must preserve the live handle and its allocation.
	buffer = {};
	buffer.nSizeBytes = 2048;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( buffer, &hReadback ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->mapBuffer( hReadback, 0, 2048, &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, 0, 2048 ) != SIR_GRAPHICS_SUCCESS ) {
		return 26;
	}
	sirGraphicsCompletionSet_t future = immediate;
	future.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	future.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = UINT64_MAX;
	if ( pDevice->retireResource( hReadback, future ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pDevice->retireResource( hReadback, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 27;
	}
	sirGraphicsCompletion_t forged = {};
	forged.nDeviceGeneration = capabilities.nDeviceGeneration;
	forged.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	forged.nValue = 1;
	bool bCompleted = true;
	if ( pDevice->poll( forged, &bCompleted ) != SIR_GRAPHICS_INVALID_ARGUMENT || bCompleted ) {
		return 10;
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommands ) != SIR_GRAPHICS_SUCCESS || pCommands == nullptr ||
	     pCommands->getQueue() != SIR_GRAPHICS_QUEUE_DIRECT || pCommands->close() != SIR_GRAPHICS_SUCCESS ) {
		return 11;
	}
	if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS || driver.destroyDevice( pDevice ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 12;
	}
	pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ||
	     pDevice->getDeviceGeneration() == capabilities.nDeviceGeneration || driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS ) {
		return 13;
	}
	// Physical GPU runners verify that each reported index opens the matching native LUID.
	config.bAllowSoftware = false;
	for ( uint32_t iAdapter = 0; iAdapter < nSoftwareIndex; ++iAdapter ) {
		sirGraphicsAdapterInfo_t reported = {};
		if ( driver.queryAdapter( iAdapter, &reported ) != SIR_GRAPHICS_SUCCESS || reported.bSoftwareAdapter ) {
			return 29;
		}
		config.nAdapterIndex = iAdapter;
		pDevice = nullptr;
		if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
			return 30;
		}
		sirD3D12AdapterInfo_t native = {};
		const bool bMatch = static_cast<SIRD3D12RenderingDevice *>( pDevice )->queryNativeAdapterInfo( &native ) == SIR_GRAPHICS_SUCCESS &&
		                    ( uint64_t )native.nAdapterLuid == reported.nAdapterLuid && !native.bSoftware;
		if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS || !bMatch ) {
			return 31;
		}
	}
	return 0;
}
