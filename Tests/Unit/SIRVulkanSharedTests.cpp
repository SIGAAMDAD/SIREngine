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

	uint32_t nAdapterCount = 0;
	sirGraphicsAdapterInfo_t adapter = {};
	if ( driver.getAdapterCount( &nAdapterCount ) != SIR_GRAPHICS_SUCCESS || nAdapterCount == 0 ||
	     driver.queryAdapter( 0, &adapter ) != SIR_GRAPHICS_SUCCESS ||
	     ( adapter.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) != ( capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) ) {
		return 44;
	}
	// Instance-only physical-device enumeration must leave the logical device inactive.
	SIRVulkanDevice probe;
	sirVulkanAdapterInfo_t nativeAdapter = {};
	uint32_t nProbed = 0;
	if ( probe.probeAdapters( 0, &nProbed, &nativeAdapter ) != SIR_VULKAN_SUCCESS || probe.isReady() || probe.getAdapterCount() != 0 ||
	     nProbed != nAdapterCount || nativeAdapter.nVendorId != adapter.nVendorId || nativeAdapter.nDeviceId != adapter.nDeviceId ) {
		return 58;
	}
	if ( probe.probeAdapters( nAdapterCount, nullptr, &nativeAdapter ) != SIR_VULKAN_DEVICE_UNAVAILABLE || probe.isReady() ) {
		return 59;
	}

	sirGraphicsSelection_t selection = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	selection.nRequiredFeatures = capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE;
	selection.bAllowSoftware = true;
	SIRGraphicsDriver *pSelected = nullptr;
	sirGraphicsAdapterInfo_t selected = {};
	if ( registry.selectDriver( selection, &pSelected, &selected ) != SIR_GRAPHICS_SUCCESS || pSelected != &driver ||
	     selected.nFeatureMask != adapter.nFeatureMask ) {
		return 45;
	}

	// Minimal descriptor-free SPIR-V compute entry point:
	// OpCapability Shader, Logical GLSL450, LocalSize(1,1,1), empty main.
	const uint32_t szComputeSpirv[] = {
	    0x07230203U, 0x00010000U, 0U,          6U,          0U, 0x00020011U, 1U, 0x0003000eU, 0U, 1U,          0x0005000fU, 5U,
	    4U,          0x6e69616dU, 0U,          0x00060010U, 4U, 17U,         1U, 1U,          1U, 0x00020013U, 1U,          0x00030021U,
	    2U,          1U,          0x00050036U, 1U,          4U, 0U,          2U, 0x000200f8U, 5U, 0x000100fdU, 0x00010038U };
	sirGraphicsPipelineDesc_t compute = {};
	compute.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	compute.computeShader.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	compute.computeShader.pBytes = szComputeSpirv;
	compute.computeShader.nByteCount = sizeof( szComputeSpirv );
	compute.computeShader.pszEntryPoint = "main";
	sirGraphicsHandle_t hCompute = {};
	if ( pDevice->createPipeline( compute, &hCompute ) != SIR_GRAPHICS_SUCCESS || hCompute.eType != SIR_GRAPHICS_HANDLE_PIPELINE ||
	     hCompute.nDeviceGeneration != pDevice->getDeviceGeneration() || driver.destroyDevice( pDevice ) != SIR_GRAPHICS_BUSY ) {
		return 37;
	}
	sirGraphicsCompletionSet_t pipelineCompletion = {};
	pipelineCompletion.nDeviceGeneration = pDevice->getDeviceGeneration();
	sirGraphicsCompletion_t computeCompleted = {};
	if ( ( capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 ) {
		SIRGraphicsCommandList *pCompute = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCompute ) != SIR_GRAPHICS_SUCCESS || pCompute == nullptr ||
		     pCompute->dispatch( 1, 1, 1 ) != SIR_GRAPHICS_INVALID_HANDLE || pCompute->bindPipeline( hCompute ) != SIR_GRAPHICS_SUCCESS ||
		     pCompute->dispatch( 0, 1, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT || pCompute->dispatch( 1, 1, 1 ) != SIR_GRAPHICS_SUCCESS ||
		     pCompute->close() != SIR_GRAPHICS_SUCCESS ) {
			return 39;
		}
		sirGraphicsSubmission_t computeSubmit = {};
		computeSubmit.ppCommandLists = &pCompute;
		computeSubmit.nCommandListCount = 1;
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, computeSubmit, &computeCompleted ) != SIR_GRAPHICS_SUCCESS ) {
			return 40;
		}
		pipelineCompletion.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
		pipelineCompletion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = computeCompleted.nValue;
	}
	// Retirement invalidates immediately, even with the dispatch still executing.
	if ( pDevice->retireResource( hCompute, pipelineCompletion ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hCompute, pipelineCompletion ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 38;
	}
	if ( computeCompleted.nValue != 0 && pDevice->wait( computeCompleted, 5000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 40;
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
	gpuDesc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_STORAGE;
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
	// Multiple closed lists share one ordered native submission and completion.
	SIRGraphicsCommandList *pMultiUpload = nullptr;
	SIRGraphicsCommandList *pMultiReadback = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pMultiUpload ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pMultiReadback ) != SIR_GRAPHICS_SUCCESS || pMultiUpload == nullptr ||
	     pMultiReadback == nullptr || pMultiUpload == pMultiReadback ||
	     pMultiUpload->copyBuffer( hGpu, 0, hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pMultiReadback->copyBuffer( hReadback, 0, hGpu, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pMultiUpload->close() != SIR_GRAPHICS_SUCCESS || pMultiReadback->close() != SIR_GRAPHICS_SUCCESS ) {
		return 46;
	}
	SIRGraphicsCommandList *szDuplicateLists[2] = { pMultiUpload, pMultiUpload };
	sirGraphicsSubmission_t multiSubmit = {};
	multiSubmit.ppCommandLists = szDuplicateLists;
	multiSubmit.nCommandListCount = 2;
	sirGraphicsCompletion_t multiCompleted = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, multiSubmit, &multiCompleted ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     multiCompleted.nValue != 0 ) {
		return 47;
	}
	SIRGraphicsCommandList *szMultiLists[2] = { pMultiUpload, pMultiReadback };
	multiSubmit.ppCommandLists = szMultiLists;
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, multiSubmit, &multiCompleted ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( multiCompleted, 5000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 48;
	}
	if ( pDevice->mapBuffer( hReadback, 0, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     memcmp( mapped.pData, szExpected, sizeof( szExpected ) ) != 0 ||
	     pDevice->unmapBuffer( hReadback, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 49;
	}

	// A full eight-operation command list must submit as one GPU timeline
	// value while preserving chained upload -> device -> readback ordering.
	SIRGraphicsCommandList *pBatch = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pBatch ) != SIR_GRAPHICS_SUCCESS || pBatch == nullptr ) {
		return 28;
	}
	for ( uint32_t iCopy = 0; iCopy < SIR_VULKAN_COPY_BATCH_CAPACITY; ++iCopy ) {
		const uint64_t nOffset = ( iCopy / 2 ) * sizeof( szExpected );
		const sirGraphicsResult_t eRecord = ( iCopy & 1 ) == 0
		                                        ? pBatch->copyBuffer( hGpu, nOffset, hSource, 0, sizeof( szExpected ) )
		                                        : pBatch->copyBuffer( hReadback, nOffset, hGpu, nOffset, sizeof( szExpected ) );
		if ( eRecord != SIR_GRAPHICS_SUCCESS ) {
			return 29;
		}
	}
	if ( pBatch->copyBuffer( hGpu, 1024, hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ||
	     pBatch->close() != SIR_GRAPHICS_SUCCESS ) {
		return 30;
	}
	sirGraphicsSubmission_t batchSubmission = {};
	batchSubmission.ppCommandLists = &pBatch;
	batchSubmission.nCommandListCount = 1;
	batchSubmission.pWaits = &second;
	batchSubmission.nWaitCount = 1;
	sirGraphicsCompletion_t batchCompleted = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, batchSubmission, &batchCompleted ) != SIR_GRAPHICS_SUCCESS ||
	     batchCompleted.nValue != second.nValue + 1 || pDevice->wait( batchCompleted, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
		return 31;
	}
	const uint64_t nBatchReadOffset = 3 * sizeof( szExpected );
	if ( pDevice->mapBuffer( hReadback, nBatchReadOffset, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, nBatchReadOffset, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     memcmp( mapped.pData, szExpected, sizeof( szExpected ) ) != 0 ||
	     pDevice->unmapBuffer( hReadback, nBatchReadOffset, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 32;
	}

	sirGraphicsCompletionSet_t completed = {};
	completed.nDeviceGeneration = pDevice->getDeviceGeneration();
	completed.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = batchCompleted.nValue;

	// The new image path creates actual Vulkan images/views/samplers through
	// VMA, then verifies the image-view lifetime dependency and stale handles.
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;

	texture.nArrayLayers = 2;
	texture.nMipLevels = 2;
	texture.nSampleCount = 1;
	texture.nUsage =
	    SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_COPY_SOURCE;
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
	copy.nMip = 1;
	copy.nLayer = 1;
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
	// Exercise native synchronization2 transitions and storage UAV ordering
	// on the same uploaded mip/layer before the readback.
	sirGraphicsBarrier_t barrier = {};
	barrier.hResource = hTexture;
	barrier.nBaseMip = 1;
	barrier.nMipCount = 1;
	barrier.nBaseLayer = 1;
	barrier.nLayerCount = 1;
	const sirGraphicsState_t szBarrierBefore[3] = { SIR_GRAPHICS_STATE_COMMON, SIR_GRAPHICS_STATE_SHADER_READ,
	                                                SIR_GRAPHICS_STATE_SHADER_WRITE };
	const sirGraphicsState_t szBarrierAfter[3] = { SIR_GRAPHICS_STATE_SHADER_READ, SIR_GRAPHICS_STATE_SHADER_WRITE,
	                                               SIR_GRAPHICS_STATE_SHADER_WRITE };
	sirGraphicsCompletion_t barrierCompleted = uploaded;
	for ( uint32_t iBarrier = 0; iBarrier < 3; ++iBarrier ) {
		barrier.eType = iBarrier == 2 ? SIR_GRAPHICS_BARRIER_UAV : SIR_GRAPHICS_BARRIER_TRANSITION;
		barrier.eBefore = szBarrierBefore[iBarrier];
		barrier.eAfter = szBarrierAfter[iBarrier];
		SIRGraphicsCommandList *pBarrierCommands = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pBarrierCommands ) != SIR_GRAPHICS_SUCCESS ||
		     pBarrierCommands == nullptr || pBarrierCommands->barrier( &barrier, 1 ) != SIR_GRAPHICS_SUCCESS ||
		     pBarrierCommands->close() != SIR_GRAPHICS_SUCCESS ) {
			return 26;
		}
		sirGraphicsSubmission_t barrierSubmission = {};
		barrierSubmission.ppCommandLists = &pBarrierCommands;
		barrierSubmission.nCommandListCount = 1;
		barrierSubmission.pWaits = &barrierCompleted;
		barrierSubmission.nWaitCount = 1;
		sirGraphicsCompletion_t signaled = {};
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, barrierSubmission, &signaled ) != SIR_GRAPHICS_SUCCESS ||
		     pDevice->wait( signaled, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
			return 27;
		}
		barrierCompleted = signaled;
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
	readbackSubmission.pWaits = &barrierCompleted;
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
	// Submit an ordered mixed command list on one direct queue timeline:
	// buffer copy -> image upload -> two image transitions -> image readback.
	SIRGraphicsCommandList *pMixed = nullptr;
	sirGraphicsBufferTextureCopy_t mixedCopy = {};
	mixedCopy.hBuffer = hGpu;
	mixedCopy.hTexture = hTexture;
	mixedCopy.nBufferOffset = 2048;
	mixedCopy.nWidth = 4;
	mixedCopy.nHeight = 8;
	mixedCopy.nDepth = 1;
	mixedCopy.nMip = 0;
	mixedCopy.nLayer = 0;
	mixedCopy.bBufferToTexture = true;
	sirGraphicsBarrier_t mixedBarrier = {};
	mixedBarrier.hResource = hTexture;
	mixedBarrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	mixedBarrier.eBefore = SIR_GRAPHICS_STATE_COMMON;
	mixedBarrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	mixedBarrier.nMipCount = 1;
	mixedBarrier.nLayerCount = 1;
	sirGraphicsBarrier_t bufferBarrier = {};
	bufferBarrier.hResource = hGpu;
	bufferBarrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	bufferBarrier.eBefore = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	bufferBarrier.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	sirGraphicsBarrier_t storageBarrier = {};
	storageBarrier.hResource = hGpu;
	storageBarrier.eType = SIR_GRAPHICS_BARRIER_UAV;
	storageBarrier.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	storageBarrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pMixed ) != SIR_GRAPHICS_SUCCESS || pMixed == nullptr ||
	     pMixed->copyBuffer( hGpu, 2048, hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pMixed->barrier( &bufferBarrier, 1 ) != SIR_GRAPHICS_SUCCESS || pMixed->barrier( &storageBarrier, 1 ) != SIR_GRAPHICS_SUCCESS ||
	     pMixed->copyBufferTexture( mixedCopy ) != SIR_GRAPHICS_SUCCESS || pMixed->barrier( &mixedBarrier, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 33;
	}
	mixedBarrier.eBefore = SIR_GRAPHICS_STATE_SHADER_READ;
	mixedBarrier.eAfter = SIR_GRAPHICS_STATE_COMMON;
	mixedCopy.hBuffer = hReadback;
	mixedCopy.bBufferToTexture = false;
	if ( pMixed->barrier( &mixedBarrier, 1 ) != SIR_GRAPHICS_SUCCESS || pMixed->copyBufferTexture( mixedCopy ) != SIR_GRAPHICS_SUCCESS ||
	     pMixed->close() != SIR_GRAPHICS_SUCCESS ) {
		return 34;
	}
	sirGraphicsSubmission_t mixedSubmission = {};
	mixedSubmission.ppCommandLists = &pMixed;
	mixedSubmission.nCommandListCount = 1;
	mixedSubmission.pWaits = &downloaded;
	mixedSubmission.nWaitCount = 1;
	sirGraphicsCompletion_t mixedCompleted = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, mixedSubmission, &mixedCompleted ) != SIR_GRAPHICS_SUCCESS ||
	     mixedCompleted.nValue != downloaded.nValue + 1 || pDevice->wait( mixedCompleted, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
		return 35;
	}
	if ( pDevice->mapBuffer( hReadback, 2048, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, 2048, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     memcmp( mapped.pData, szExpected, sizeof( szExpected ) ) != 0 ||
	     pDevice->unmapBuffer( hReadback, 2048, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 36;
	}
	// Upload and read back three depth slices of a native 3D texture.
	sirGraphicsTextureDesc_t volume = texture;
	volume.nWidth = 4;
	volume.nHeight = 2;
	volume.nDepth = 4;
	volume.nArrayLayers = 1;
	volume.nMipLevels = 1;
	volume.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	sirGraphicsHandle_t hVolume = {};
	if ( pDevice->createTexture( volume, &hVolume ) != SIR_GRAPHICS_SUCCESS ) {
		return 60;
	}
	sirGraphicsBufferTextureCopy_t volumeCopy = {};
	volumeCopy.hBuffer = hSource;
	volumeCopy.hTexture = hVolume;
	volumeCopy.nWidth = 4;
	volumeCopy.nHeight = 2;
	volumeCopy.nZ = 1;
	volumeCopy.nDepth = 3;
	volumeCopy.bBufferToTexture = true;
	SIRGraphicsCommandList *pVolumeUpload = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pVolumeUpload ) != SIR_GRAPHICS_SUCCESS ||
	     pVolumeUpload == nullptr ) {
		return 61;
	}
	volumeCopy.nZ = 3;
	if ( pVolumeUpload->copyBufferTexture( volumeCopy ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 62;
	}
	volumeCopy.nZ = 1;
	if ( pVolumeUpload->copyBufferTexture( volumeCopy ) != SIR_GRAPHICS_SUCCESS ||
	     pVolumeUpload->close() != SIR_GRAPHICS_SUCCESS ) {
		return 63;
	}
	sirGraphicsSubmission_t volumeSubmission = {};
	volumeSubmission.ppCommandLists = &pVolumeUpload;
	volumeSubmission.nCommandListCount = 1;
	sirGraphicsCompletion_t volumeUploaded = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, volumeSubmission, &volumeUploaded ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( volumeUploaded, 5000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 64;
	}
	volumeCopy.hBuffer = hReadback;
	volumeCopy.bBufferToTexture = false;
	SIRGraphicsCommandList *pVolumeReadback = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pVolumeReadback ) != SIR_GRAPHICS_SUCCESS ||
	     pVolumeReadback == nullptr || pVolumeReadback->copyBufferTexture( volumeCopy ) != SIR_GRAPHICS_SUCCESS ||
	     pVolumeReadback->close() != SIR_GRAPHICS_SUCCESS ) {
		return 65;
	}
	volumeSubmission.ppCommandLists = &pVolumeReadback;
	sirGraphicsCompletion_t volumeDownloaded = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, volumeSubmission, &volumeDownloaded ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( volumeDownloaded, 5000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 66;
	}
	constexpr uint64_t VOLUME_BYTES = 4ULL * 2ULL * 3ULL * 4ULL;
	if ( pDevice->mapBuffer( hReadback, 0, VOLUME_BYTES, &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->invalidateBuffer( hReadback, 0, VOLUME_BYTES ) != SIR_GRAPHICS_SUCCESS ||
	     memcmp( mapped.pData, szExpected, VOLUME_BYTES ) != 0 ||
	     pDevice->unmapBuffer( hReadback, 0, VOLUME_BYTES ) != SIR_GRAPHICS_SUCCESS ) {
		return 67;
	}
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = volumeDownloaded.nValue;
	if ( pDevice->retireResource( hVolume, completed ) != SIR_GRAPHICS_SUCCESS ) {
		return 68;
	}
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = volumeDownloaded.nValue;
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
	// Transition a complete 2x2 mip/layer image range in one GPU command.
	sirGraphicsTextureDesc_t rangeDesc = texture;
	rangeDesc.nUsage = SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_SAMPLED;
	sirGraphicsHandle_t hRange = {};
	if ( pDevice->createTexture( rangeDesc, &hRange ) != SIR_GRAPHICS_SUCCESS ) {
		return 50;
	}
	SIRGraphicsCommandList *pRange = nullptr;
	sirGraphicsBarrier_t rangeBarrier = {};
	rangeBarrier.hResource = hRange;
	rangeBarrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	rangeBarrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	rangeBarrier.eAfter = SIR_GRAPHICS_STATE_COMMON;
	rangeBarrier.nMipCount = 2;
	rangeBarrier.nLayerCount = 2;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pRange ) != SIR_GRAPHICS_SUCCESS || pRange == nullptr ||
	     pRange->barrier( &rangeBarrier, 1 ) != SIR_GRAPHICS_SUCCESS || pRange->close() != SIR_GRAPHICS_SUCCESS ) {
		return 51;
	}
	sirGraphicsSubmission_t rangeSubmit = {};
	rangeSubmit.ppCommandLists = &pRange;
	rangeSubmit.nCommandListCount = 1;
	sirGraphicsCompletion_t rangeComplete = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, rangeSubmit, &rangeComplete ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( rangeComplete, 5000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 52;
	}
	completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = rangeComplete.nValue;
	if ( pDevice->retireResource( hRange, completed ) != SIR_GRAPHICS_SUCCESS ) {
		return 53;
	}
	if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice->getDeviceGeneration() != 0 ) {
		return 14;
	}
	// An adapter index must select the same physical device reported by enumeration.
	if ( nAdapterCount > 1 ) {
		sirGraphicsAdapterInfo_t secondAdapter = {};
		if ( driver.queryAdapter( 1, &secondAdapter ) != SIR_GRAPHICS_SUCCESS || secondAdapter.nAdapterIndex != 1 ) {
			return 54;
		}
		create.nAdapterIndex = 1;
		pDevice = nullptr;
		sirGraphicsCapabilities_t secondCapabilities = {};
		if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ||
		     pDevice->queryCapabilities( &secondCapabilities ) != SIR_GRAPHICS_SUCCESS ||
		     ( secondCapabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) !=
		         ( secondAdapter.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) ) {
			return 55;
		}
		if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS ) {
			return 56;
		}
		create.nAdapterIndex = 0;
	}
	if ( driver.queryAdapter( nAdapterCount, &adapter ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 57;
	}
	if ( ( capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 ) {
		create.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
		pDevice = nullptr;
		if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ||
		     driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS ) {
			return 43;
		}
	}
	printf( "Vulkan shared API headless transfer passed\n" );
	return 0;
}
