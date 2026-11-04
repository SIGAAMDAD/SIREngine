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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool sirLoadCommandShader( const char *pszPath, void **ppData, size_t *pBytes ) noexcept
{
	*ppData = nullptr;
	*pBytes = 0;
	FILE *pFile = nullptr;
	if ( fopen_s( &pFile, pszPath, "rb" ) != 0 || pFile == nullptr ) {
		return false;
	}
	if ( fseek( pFile, 0, SEEK_END ) != 0 ) {
		fclose( pFile );
		return false;
	}
	const long nBytes = ftell( pFile );
	if ( nBytes < 16 || nBytes > 1024 * 1024 || fseek( pFile, 0, SEEK_SET ) != 0 ) {
		fclose( pFile );
		return false;
	}
	void *pSource = malloc( ( size_t )nBytes );
	if ( pSource == nullptr ) {
		fclose( pFile );
		return false;
	}
	const size_t nRead = fread( pSource, 1, ( size_t )nBytes, pFile );
	fclose( pFile );
	if ( nRead != ( size_t )nBytes ) {
		free( pSource );
		return false;
	}
	*ppData = pSource;
	*pBytes = nRead;
	return true;
}

static sirGraphicsResult_t sirTestSubmit( SIRRenderingDevice *pDevice, sirGraphicsQueue_t eQueue, SIRGraphicsCommandList *pCommands,
                                          const sirGraphicsCompletion_t *pWaits, uint32_t nWaitCount,
                                          sirGraphicsCompletion_t *pCompletion ) noexcept
{
	SIRGraphicsCommandList *szCommands[] = { pCommands };
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = szCommands;
	submission.nCommandListCount = 1;
	submission.pWaits = pWaits;
	submission.nWaitCount = nWaitCount;
	return pDevice->submit( eQueue, submission, pCompletion );
}

static int sirTestCopies( SIRRenderingDevice *pDevice, uint32_t nGeneration, sirGraphicsCompletion_t *pCopyCompletion ) noexcept
{
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = 4096;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t hUpload = {};
	if ( pDevice->createBuffer( desc, &hUpload ) != SIR_GRAPHICS_SUCCESS ) {
		return 10;
	}
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( desc, &hReadback ) != SIR_GRAPHICS_SUCCESS ) {
		return 11;
	}
	sirGraphicsMappedBuffer_t mapped = {};
	if ( pDevice->mapBuffer( hUpload, 0, 4096, &mapped ) != SIR_GRAPHICS_SUCCESS ) {
		return 12;
	}
	for ( uint32_t iByte = 0; iByte < 4096; ++iByte ) {
		( ( uint8_t * )mapped.pData )[iByte] = ( uint8_t )( iByte ^ 0xA5 );
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pCommands ) != SIR_GRAPHICS_SUCCESS || pCommands == nullptr ||
	     pCommands->copyBuffer( hReadback, 0, hUpload, 0, 4096 ) != SIR_GRAPHICS_SUCCESS ) {
		return 13;
	}
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = nGeneration;
	if ( pDevice->retireResource( hUpload, immediate ) != SIR_GRAPHICS_BUSY ) {
		return 14;
	}
	if ( pCommands->close() != SIR_GRAPHICS_SUCCESS ||
	     sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_COPY, pCommands, nullptr, 0, pCopyCompletion ) != SIR_GRAPHICS_SUCCESS ||
	     pCopyCompletion->eQueue != SIR_GRAPHICS_QUEUE_COPY || pCopyCompletion->nDeviceGeneration != nGeneration ||
	     pCopyCompletion->nValue == 0 ) {
		return 15;
	}
	if ( pDevice->retireResource( hReadback, immediate ) != SIR_GRAPHICS_BUSY ) {
		return 16;
	}
	if ( pDevice->wait( *pCopyCompletion, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 17;
	}
	bool bCompleted = false;
	if ( pDevice->poll( *pCopyCompletion, &bCompleted ) != SIR_GRAPHICS_SUCCESS || !bCompleted ||
	     pDevice->mapBuffer( hReadback, 0, 4096, &mapped ) != SIR_GRAPHICS_SUCCESS ) {
		return 18;
	}
	for ( uint32_t iByte = 0; iByte < 4096; ++iByte ) {
		if ( ( ( uint8_t * )mapped.pData )[iByte] != ( uint8_t )( iByte ^ 0xA5 ) ) {
			return 19;
		}
	}
	if ( pDevice->retireResource( hReadback, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hUpload, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 20;
	}
	return 0;
}

static int sirTestQueueSubmission( SIRRenderingDevice *pDevice, uint32_t nGeneration, const sirGraphicsShaderCode_t &computeCode,
                                   const sirGraphicsCompletion_t &copyCompletion ) noexcept
{
	sirGraphicsPipelineDesc_t desc = {};
	desc.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	desc.computeShader = computeCode;
	sirGraphicsHandle_t hPipeline = {};
	if ( pDevice->createPipeline( desc, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return 30;
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COMPUTE, &pCommands ) != SIR_GRAPHICS_SUCCESS || pCommands == nullptr ||
	     pCommands->bindPipeline( hPipeline ) != SIR_GRAPHICS_SUCCESS || pCommands->dispatch( 1, 1, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 31;
	}
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = nGeneration;
	if ( pDevice->retireResource( hPipeline, immediate ) != SIR_GRAPHICS_BUSY ) {
		return 32;
	}
	sirGraphicsCompletion_t future = copyCompletion;
	++future.nValue;
	sirGraphicsCompletion_t rejected = {};
	if ( pCommands->close() != SIR_GRAPHICS_SUCCESS ||
	     sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_COMPUTE, pCommands, &future, 1, &rejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 33;
	}
	sirGraphicsCompletion_t computeComplete = {};
	if ( sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_COMPUTE, pCommands, &copyCompletion, 1, &computeComplete ) != SIR_GRAPHICS_SUCCESS ) {
		return 34;
	}
	if ( sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_COMPUTE, pCommands, nullptr, 0, &rejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 35;
	}
	if ( pDevice->wait( computeComplete, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 36;
	}
	SIRGraphicsCommandList *pDirect = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pDirect ) != SIR_GRAPHICS_SUCCESS || pDirect == nullptr ||
	     pDirect->close() != SIR_GRAPHICS_SUCCESS ) {
		return 37;
	}
	sirGraphicsCompletion_t directComplete = {};
	if ( sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, pDirect, &computeComplete, 1, &directComplete ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( directComplete, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 38;
	}
	if ( pDevice->retireResource( hPipeline, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 39;
	}

	SIRGraphicsCommandList *szPending[4] = {};
	for ( uint32_t iSlot = 0; iSlot < 4; ++iSlot ) {
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &szPending[iSlot] ) != SIR_GRAPHICS_SUCCESS ) {
			return 40;
		}
	}
	SIRGraphicsCommandList *pOverflow = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pOverflow ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || pOverflow != nullptr ) {
		return 41;
	}
	for ( uint32_t iSlot = 0; iSlot < 4; ++iSlot ) {
		if ( szPending[iSlot]->close() != SIR_GRAPHICS_SUCCESS ) {
			return 42;
		}
	}
	// A closed but not yet submitted command list must retain its slot.
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pOverflow ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || pOverflow != nullptr ) {
		return 45;
	}
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = szPending;
	submission.nCommandListCount = 4;
	sirGraphicsCompletion_t fourComplete = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_COPY, submission, &fourComplete ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( fourComplete, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pOverflow ) != SIR_GRAPHICS_SUCCESS || pOverflow == nullptr ) {
		return 43;
	}
	if ( pOverflow->close() != SIR_GRAPHICS_SUCCESS ) {
		return 44;
	}
	return 0;
}

static int sirTestRaster( SIRRenderingDevice *pDevice, uint32_t nGeneration, const sirGraphicsShaderCode_t &vertexCode,
                          const sirGraphicsShaderCode_t &pixelCode ) noexcept
{
	sirGraphicsBufferDesc_t upload = {};
	upload.nSizeBytes = 3 * 8 * sizeof( float );
	upload.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	upload.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t hVertices = {};
	if ( pDevice->createBuffer( upload, &hVertices ) != SIR_GRAPHICS_SUCCESS ) {
		return 50;
	}
	const float szVertices[24] = { -0.7f, -0.7f, 0.0f, 1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.7f, 0.0f, 1.0f,
	                               1.0f,  0.0f,  0.0f, 1.0f, 0.7f, -0.7f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	sirGraphicsMappedBuffer_t mapped = {};
	if ( pDevice->mapBuffer( hVertices, 0, sizeof( szVertices ), &mapped ) != SIR_GRAPHICS_SUCCESS ) {
		return 51;
	}
	memcpy( mapped.pData, szVertices, sizeof( szVertices ) );
	sirGraphicsBufferDesc_t indices = {};
	indices.nSizeBytes = 6;
	indices.nUsage = SIR_GRAPHICS_USAGE_INDEX;
	indices.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t hIndices = {};
	if ( pDevice->createBuffer( indices, &hIndices ) != SIR_GRAPHICS_SUCCESS ) {
		return 52;
	}
	if ( pDevice->mapBuffer( hIndices, 0, 6, &mapped ) != SIR_GRAPHICS_SUCCESS ) {
		return 53;
	}
	const uint16_t szIndices[3] = { 0, 1, 2 };
	memcpy( mapped.pData, szIndices, 6 );
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 16;
	texture.nHeight = 16;
	texture.nDepth = 1;
	texture.nArrayLayers = 2;
	texture.nMipLevels = 4;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hTexture = {};
	if ( pDevice->createTexture( texture, &hTexture ) != SIR_GRAPHICS_SUCCESS ) {
		return 54;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = hTexture;
	view.eFormat = texture.eFormat;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirGraphicsHandle_t hRtv = {};
	if ( pDevice->createView( view, &hRtv ) != SIR_GRAPHICS_SUCCESS ) {
		return 55;
	}
	sirGraphicsVertexAttribute_t szAttributes[2] = {};
	szAttributes[0].nLocation = 0;
	szAttributes[0].nBufferSlot = 0;
	szAttributes[0].nOffsetBytes = 0;
	szAttributes[0].nStrideBytes = 32;
	szAttributes[0].eFormat = SIR_GRAPHICS_FORMAT_RGBA32_FLOAT;
	szAttributes[1].nLocation = 1;
	szAttributes[1].nBufferSlot = 0;
	szAttributes[1].nOffsetBytes = 16;
	szAttributes[1].nStrideBytes = 32;
	szAttributes[1].eFormat = SIR_GRAPHICS_FORMAT_RGBA32_FLOAT;
	sirGraphicsPipelineDesc_t pipeline = {};
	pipeline.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.vertexShader = vertexCode;
	pipeline.pixelShader = pixelCode;
	pipeline.pAttributes = szAttributes;
	pipeline.nAttributeCount = 2;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = texture.eFormat;
	pipeline.szBlendStates[0].nColorWriteMask = 0xFU;
	pipeline.nSampleCount = 1;
	pipeline.eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	pipeline.eCullMode = SIR_GRAPHICS_CULL_NONE;
	sirGraphicsHandle_t hPipeline = {};
	if ( pDevice->createPipeline( pipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return 56;
	}
	// Exercise direct-queue GPU-local UAV ordering independently of the raster target.
	sirGraphicsBufferDesc_t storage = {};
	storage.nSizeBytes = 4096;
	storage.nUsage = SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	storage.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hStorage = {};
	if ( pDevice->createBuffer( storage, &hStorage ) != SIR_GRAPHICS_SUCCESS ) {
		return 74;
	}
	SIRGraphicsCommandList *pDirect = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pDirect ) != SIR_GRAPHICS_SUCCESS ) {
		return 57;
	}
	// A buffer cannot transition to an attachment layout.
	sirGraphicsBarrier_t invalidBuffer = {};
	invalidBuffer.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	invalidBuffer.hResource = hVertices;
	invalidBuffer.eBefore = SIR_GRAPHICS_STATE_COMMON;
	invalidBuffer.eAfter = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;
	if ( pDirect->barrier( &invalidBuffer, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 72;
	}
	sirGraphicsBarrier_t invalidUav = {};
	invalidUav.eType = SIR_GRAPHICS_BARRIER_UAV;
	invalidUav.hResource = hVertices;
	if ( pDirect->barrier( &invalidUav, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 75;
	}
	sirGraphicsBarrier_t storageTransition = {};
	storageTransition.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	storageTransition.hResource = hStorage;
	storageTransition.eBefore = SIR_GRAPHICS_STATE_COMMON;
	storageTransition.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	sirGraphicsBarrier_t storageUav = {};
	storageUav.eType = SIR_GRAPHICS_BARRIER_UAV;
	storageUav.hResource = hStorage;
	if ( pDirect->barrier( &storageTransition, 1 ) != SIR_GRAPHICS_SUCCESS ||
	     pDirect->barrier( &storageUav, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 76;
	}
	storageTransition.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	storageTransition.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	if ( pDirect->barrier( &storageTransition, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 77;
	}
	sirGraphicsBarrier_t transition = {};
	transition.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	transition.hResource = hTexture;
	transition.eBefore = SIR_GRAPHICS_STATE_COMMON;
	transition.eAfter = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;

	// Exercise a nonzero mip/layer and reject a range beyond the image.
	sirGraphicsBarrier_t partial = transition;
	partial.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	partial.nBaseMip = 4;
	partial.nMipCount = 1;
	partial.nBaseLayer = 1;
	partial.nLayerCount = 1;
	if ( pDirect->barrier( &partial, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 70;
	}
	partial.nBaseMip = 2;
	partial.nBaseLayer = 2;
	if ( pDirect->barrier( &partial, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 73;
	}
	partial.nBaseLayer = 1;
	if ( pDirect->barrier( &partial, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 71;
	}
	partial.eBefore = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	partial.eAfter = SIR_GRAPHICS_STATE_COMMON;
	if ( pDirect->barrier( &partial, 1 ) != SIR_GRAPHICS_SUCCESS || pDirect->barrier( &transition, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 58;
	}
	sirGraphicsColorAttachment_t color = {};
	color.hView = hRtv;
	color.bClear = true;
	color.bStore = true;
	color.szClearColor[3] = 1.0f;
	sirGraphicsRenderPass_t pass = {};
	pass.pColors = &color;
	pass.nColorCount = 1;
	if ( pDirect->beginRenderPass( pass ) != SIR_GRAPHICS_SUCCESS || pDirect->bindPipeline( hPipeline ) != SIR_GRAPHICS_SUCCESS ||
	     pDirect->bindVertexBuffer( 0, hVertices, 0, 32 ) != SIR_GRAPHICS_SUCCESS ||
	     pDirect->bindIndexBuffer( hIndices, 0, false ) != SIR_GRAPHICS_SUCCESS ) {
		return 59;
	}
	sirGraphicsViewport_t viewport = {};
	viewport.fWidth = 16.0f;
	viewport.fHeight = 16.0f;
	viewport.fMaximumDepth = 1.0f;
	sirGraphicsRect_t rect = { 0, 0, 16, 16 };
	if ( pDirect->setViewport( viewport, rect ) != SIR_GRAPHICS_SUCCESS || pDirect->drawIndexed( 3, 1, 0, 0, 0 ) != SIR_GRAPHICS_SUCCESS ||
	     pDirect->endRenderPass() != SIR_GRAPHICS_SUCCESS ) {
		return 60;
	}
	transition.eBefore = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;
	transition.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	if ( pDirect->barrier( &transition, 1 ) != SIR_GRAPHICS_SUCCESS || pDirect->close() != SIR_GRAPHICS_SUCCESS ) {
		return 61;
	}
	sirGraphicsCompletion_t directDone = {};
	if ( sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_DIRECT, pDirect, nullptr, 0, &directDone ) != SIR_GRAPHICS_SUCCESS ) {
		return 62;
	}

	sirGraphicsBufferDesc_t readback = {};
	readback.nSizeBytes = 256 * 16;
	readback.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	readback.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( readback, &hReadback ) != SIR_GRAPHICS_SUCCESS ) {
		return 63;
	}
	SIRGraphicsCommandList *pCopy = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pCopy ) != SIR_GRAPHICS_SUCCESS ) {
		return 64;
	}
	sirGraphicsBufferTextureCopy_t region = {};
	region.hBuffer = hReadback;
	region.hTexture = hTexture;
	region.nBytesPerRow = 256;
	region.nRowsPerImage = 16;
	region.nWidth = 16;
	region.nHeight = 16;
	region.nDepth = 1;
	region.bBufferToTexture = false;
	if ( pCopy->copyBufferTexture( region ) != SIR_GRAPHICS_SUCCESS || pCopy->close() != SIR_GRAPHICS_SUCCESS ) {
		return 65;
	}
	sirGraphicsCompletion_t copyDone = {};
	if ( sirTestSubmit( pDevice, SIR_GRAPHICS_QUEUE_COPY, pCopy, &directDone, 1, &copyDone ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( copyDone, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 66;
	}
	if ( pDevice->mapBuffer( hReadback, 0, readback.nSizeBytes, &mapped ) != SIR_GRAPHICS_SUCCESS ) {
		return 67;
	}
	const uint8_t *pPixel = ( const uint8_t * )mapped.pData + 8 * 256 + 8 * 4;
	if ( pPixel[0] < 150 || pPixel[1] > 64 || pPixel[2] > 64 ) {
		return 68;
	}
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = nGeneration;
	if ( pDevice->retireResource( hRtv, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hTexture, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReadback, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hVertices, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hIndices, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hStorage, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hPipeline, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 69;
	}
	return 0;
}

int main( void )
{
	SIRD3D12Driver driver;
	uint32_t nAdapters = 0;
	if ( driver.getAdapterCount( &nAdapters ) != SIR_GRAPHICS_SUCCESS ) {
		return 1;
	}
	uint32_t nWarp = UINT32_MAX;
	for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS ) {
			return 2;
		}
		if ( info.bSoftwareAdapter ) {
			nWarp = iAdapter;
		}
	}
	if ( nWarp == UINT32_MAX ) {
		return 3;
	}
	void *pComputeDxil = nullptr;
	void *pVertexDxil = nullptr;
	void *pPixelDxil = nullptr;
	size_t nComputeBytes = 0;
	size_t nVertexBytes = 0;
	size_t nPixelBytes = 0;
	if ( !sirLoadCommandShader( SIR_D3D12_COMMANDS_DXIL_PATH, &pComputeDxil, &nComputeBytes ) ||
	     !sirLoadCommandShader( SIR_D3D12_VS_DXIL_PATH, &pVertexDxil, &nVertexBytes ) ||
	     !sirLoadCommandShader( SIR_D3D12_PS_DXIL_PATH, &pPixelDxil, &nPixelBytes ) ) {
		free( pComputeDxil );
		free( pVertexDxil );
		free( pPixelDxil );
		return 4;
	}
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nWarp;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bHeadless = true;
	config.bAllowSoftware = true;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 5;
	}
	sirGraphicsShaderCode_t compute = {};
	compute.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	compute.pBytes = pComputeDxil;
	compute.nByteCount = nComputeBytes;
	sirGraphicsShaderCode_t vertex = {};
	vertex.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	vertex.pBytes = pVertexDxil;
	vertex.nByteCount = nVertexBytes;
	sirGraphicsShaderCode_t pixel = {};
	pixel.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	pixel.pBytes = pPixelDxil;
	pixel.nByteCount = nPixelBytes;
	const uint32_t nGeneration = pDevice->getDeviceGeneration();
	sirGraphicsCompletion_t copied = {};
	int nFailure = sirTestCopies( pDevice, nGeneration, &copied );
	if ( nFailure == 0 ) {
		nFailure = sirTestQueueSubmission( pDevice, nGeneration, compute, copied );
	}
	if ( nFailure == 0 ) {
		nFailure = sirTestRaster( pDevice, nGeneration, vertex, pixel );
	}
	const sirGraphicsResult_t eShutdown = driver.destroyDevice( pDevice );
	free( pComputeDxil );
	free( pVertexDxil );
	free( pPixelDxil );
	if ( nFailure != 0 ) {
		fprintf( stderr, "General D3D12 command test failed at stage %d\\n", nFailure );
		return nFailure;
	}
	return eShutdown == SIR_GRAPHICS_SUCCESS ? 0 : 6;
}
