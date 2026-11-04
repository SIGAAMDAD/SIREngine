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

#include <Renderer/Drivers/Vulkan/SIRVulkanGraphics.h>
#include <Renderer/SIRGraphicsRegistry.h>

#include <stdio.h>
#include <string.h>

int main( void )
{
	SIRVulkanDriver driver;
	SIRGraphicsRegistry registry;
	if ( driver.getBackend() != SIR_GRAPHICS_BACKEND_VULKAN || registry.registerDriver( &driver ) != SIR_GRAPHICS_SUCCESS ||
	     registry.findDriver( SIR_GRAPHICS_BACKEND_VULKAN ) != &driver ||
	     driver.getAdapterCount( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     driver.destroyDevice( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 1;
	}

	SIRVulkanRenderingDevice inert;
	sirGraphicsCapabilities_t capabilities = {};
	if ( inert.getBackend() != SIR_GRAPHICS_BACKEND_VULKAN || inert.getDeviceGeneration() != 0 ||
	     inert.queryCapabilities( &capabilities ) != SIR_GRAPHICS_BUSY ) {
		return 2;
	}

	sirGraphicsDeviceCreateDesc_t create = {};
	create.nAdapterIndex = 0;
	create.nFramesInFlight = 2;
	create.nRequiredFeatures = 0;
	create.bHeadless = true;
	SIRRenderingDevice *pDevice = nullptr;
	const sirGraphicsResult_t eCreate = driver.createDevice( create, &pDevice );
	if ( eCreate == SIR_GRAPHICS_UNSUPPORTED ) {
		printf( "SKIP: Vulkan 1.4 hardware unavailable\n" );
		return 77;
	}
	if ( eCreate != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 3;
	}
	if ( pDevice->getDeviceGeneration() == 0 || pDevice->queryCapabilities( &capabilities ) != SIR_GRAPHICS_SUCCESS ||
	     capabilities.eBackend != SIR_GRAPHICS_BACKEND_VULKAN || ( capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_GRAPHICS ) != 0 ||
	     ( capabilities.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return 4;
	}

	SIRGraphicsCommandList *pCancelled = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCancelled ) != SIR_GRAPHICS_SUCCESS || pCancelled == nullptr ||
	     driver.destroyDevice( pDevice ) != SIR_GRAPHICS_BUSY || pCancelled->close() != SIR_GRAPHICS_SUCCESS ) {
		return 15;
	}

	sirGraphicsBufferDesc_t sourceDesc = {};
	sourceDesc.nSizeBytes = 4096;
	sourceDesc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	sourceDesc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsBufferDesc_t gpuDesc = {};
	gpuDesc.nSizeBytes = 4096;
	gpuDesc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	gpuDesc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsBufferDesc_t readbackDesc = {};
	readbackDesc.nSizeBytes = 4096;
	readbackDesc.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	readbackDesc.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;

	sirGraphicsHandle_t hSource = {};
	sirGraphicsHandle_t hGpu = {};
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( sourceDesc, &hSource ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->createBuffer( gpuDesc, &hGpu ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->createBuffer( readbackDesc, &hReadback ) != SIR_GRAPHICS_SUCCESS ) {
		return 5;
	}
	uint8_t szExpected[128] = {};
	for ( uint32_t iByte = 0; iByte < sizeof( szExpected ); ++iByte ) {
		szExpected[iByte] = ( uint8_t )( iByte ^ 0x5a );
	}
	sirGraphicsMappedBuffer_t mapped = {};
	if ( pDevice->mapBuffer( hSource, 0, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS || mapped.pData == nullptr ||
	     mapped.nBytes != sizeof( szExpected ) ) {
		return 6;
	}
	memcpy( mapped.pData, szExpected, sizeof( szExpected ) );
	if ( pDevice->flushBuffer( hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->unmapBuffer( hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 7;
	}

	SIRGraphicsCommandList *pAbandoned = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pAbandoned ) != SIR_GRAPHICS_SUCCESS || pAbandoned == nullptr ||
	     pAbandoned->copyBuffer( hGpu, 0, hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pAbandoned->close() != SIR_GRAPHICS_SUCCESS || pAbandoned->reset() != SIR_GRAPHICS_SUCCESS ||
	     pAbandoned->close() != SIR_GRAPHICS_SUCCESS ) {
		return 16;
	}

	SIRGraphicsCommandList *pFirst = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pFirst ) != SIR_GRAPHICS_SUCCESS || pFirst == nullptr ||
	     pFirst->copyBuffer( hGpu, 0, hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pFirst->close() != SIR_GRAPHICS_SUCCESS ) {
		return 8;
	}
	sirGraphicsSubmission_t firstSubmission = {};
	firstSubmission.ppCommandLists = &pFirst;
	firstSubmission.nCommandListCount = 1;
	sirGraphicsCompletion_t first = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, firstSubmission, &first ) != SIR_GRAPHICS_SUCCESS || first.nValue == 0 ) {
		return 9;
	}

	SIRGraphicsCommandList *pSecond = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pSecond ) != SIR_GRAPHICS_SUCCESS || pSecond == nullptr ||
	     pSecond->copyBuffer( hReadback, 0, hGpu, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pSecond->close() != SIR_GRAPHICS_SUCCESS ) {
		return 10;
	}
	sirGraphicsSubmission_t secondSubmission = {};
	secondSubmission.ppCommandLists = &pSecond;
	secondSubmission.nCommandListCount = 1;
	secondSubmission.pWaits = &first;
	secondSubmission.nWaitCount = 1;
	sirGraphicsCompletion_t second = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, secondSubmission, &second ) != SIR_GRAPHICS_SUCCESS || second.nValue <= first.nValue ||
	     pDevice->wait( second, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
		return 11;
	}

	if ( pDevice->mapBuffer( hReadback, 0, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     memcmp( mapped.pData, szExpected, sizeof( szExpected ) ) != 0 ||
	     pDevice->unmapBuffer( hReadback, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 12;
	}
	sirGraphicsCompletionSet_t completed = {};
	completed.nDeviceGeneration = pDevice->getDeviceGeneration();
	completed.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = second.nValue;

	// The new image path creates actual Vulkan images/views/samplers through
	// VMA, then verifies the image-view lifetime dependency and stale handles.
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hTexture = {};
	if ( pDevice->createTexture( texture, &hTexture ) != SIR_GRAPHICS_SUCCESS || hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return 17;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = hTexture;
	view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	sirGraphicsHandle_t hView = {};
	if ( pDevice->createView( view, &hView ) != SIR_GRAPHICS_SUCCESS || hView.eType != SIR_GRAPHICS_HANDLE_VIEW ||
	     driver.destroyDevice( pDevice ) != SIR_GRAPHICS_BUSY || pDevice->retireResource( hTexture, completed ) != SIR_GRAPHICS_BUSY ) {
		return 18;
	}
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.eFilter = SIR_GRAPHICS_FILTER_TRILINEAR;
	sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
	sampler.eAddressW = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.fMaximumAnisotropy = 1.0f;
	sampler.fMinimumLod = 0.0f;
	sampler.fMaximumLod = 1.0f;
	sirGraphicsHandle_t hSampler = {};
	if ( pDevice->createSampler( sampler, &hSampler ) != SIR_GRAPHICS_SUCCESS || hSampler.eType != SIR_GRAPHICS_HANDLE_SAMPLER ) {
		return 19;
	}
	// Transfer a tightly packed 4x8 RGBA8 image, then read it back using
	// synchronization2 transitions and the existing timeline completion.
	sirGraphicsBufferTextureCopy_t copy = {};
	copy.hBuffer = hSource;
	copy.hTexture = hTexture;
	copy.nWidth = 4;
	copy.nHeight = 8;
	copy.nDepth = 1;
	copy.bBufferToTexture = true;
	SIRGraphicsCommandList *pUpload = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pUpload ) != SIR_GRAPHICS_SUCCESS || pUpload == nullptr ||
	     pUpload->copyBufferTexture( copy ) != SIR_GRAPHICS_SUCCESS || pUpload->close() != SIR_GRAPHICS_SUCCESS ) {
		return 21;
	}
	sirGraphicsSubmission_t uploadSubmission = {};
	uploadSubmission.ppCommandLists = &pUpload;
	uploadSubmission.nCommandListCount = 1;
	sirGraphicsCompletion_t uploaded = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, uploadSubmission, &uploaded ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( uploaded, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
		return 22;
	}
	copy.hBuffer = hReadback;
	copy.bBufferToTexture = false;
	SIRGraphicsCommandList *pImageReadback = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pImageReadback ) != SIR_GRAPHICS_SUCCESS || pImageReadback == nullptr ||
	     pImageReadback->copyBufferTexture( copy ) != SIR_GRAPHICS_SUCCESS || pImageReadback->close() != SIR_GRAPHICS_SUCCESS ) {
		return 23;
	}
	sirGraphicsSubmission_t readbackSubmission = {};
	readbackSubmission.ppCommandLists = &pImageReadback;
	readbackSubmission.nCommandListCount = 1;
	readbackSubmission.pWaits = &uploaded;
	readbackSubmission.nWaitCount = 1;
	sirGraphicsCompletion_t downloaded = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, readbackSubmission, &downloaded ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( downloaded, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
		return 24;
	}
	if ( pDevice->mapBuffer( hReadback, 0, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     memcmp( mapped.pData, szExpected, sizeof( szExpected ) ) != 0 ||
	     pDevice->unmapBuffer( hReadback, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 25;
	}
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = downloaded.nValue;
	if ( pDevice->retireResource( hView, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hTexture, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hSampler, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hView, completed ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     pDevice->retireResource( hTexture, completed ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 20;
	}
	if ( pDevice->retireResource( hSource, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hGpu, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReadback, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hSource, completed ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 13;
	}
	if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice->getDeviceGeneration() != 0 ) {
		return 14;
	}
	printf( "Vulkan shared API headless transfer passed\n" );
	return 0;
}
