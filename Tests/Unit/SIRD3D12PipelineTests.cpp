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

#include <Renderer/SIRGraphicsABIOperations.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Driver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool sirLoadPipelineDxil( const char *pszPath, void **ppData, size_t *pSize ) noexcept
{
	if ( ppData == nullptr || pSize == nullptr || pszPath == nullptr ) {
		return false;
	}
	*ppData = nullptr;
	*pSize = 0;
	FILE *pFile = nullptr;
	if ( fopen_s( &pFile, pszPath, "rb" ) != 0 || pFile == nullptr ) {
		return false;
	}
	if ( fseek( pFile, 0, SEEK_END ) != 0 ) {
		fclose( pFile );
		return false;
	}
	const long nLength = ftell( pFile );
	if ( nLength < 16 || nLength > 4 * 1024 * 1024 || fseek( pFile, 0, SEEK_SET ) != 0 ) {
		fclose( pFile );
		return false;
	}
	void *pBytes = malloc( ( size_t )nLength );
	if ( pBytes == nullptr ) {
		fclose( pFile );
		return false;
	}
	const size_t nRead = fread( pBytes, 1, ( size_t )nLength, pFile );
	fclose( pFile );
	if ( nRead != ( size_t )nLength ) {
		free( pBytes );
		return false;
	}
	*ppData = pBytes;
	*pSize = nRead;
	return true;
}

static int sirTestGeneralPipelines( SIRRenderingDevice *pDevice, uint32_t nGeneration, const sirGraphicsShaderCode_t& computeShader,
                                    const sirGraphicsShaderCode_t& vertexShader, const sirGraphicsShaderCode_t& pixelShader ) noexcept
{
	sirGraphicsCompletionSet_t completed = {};
	completed.nDeviceGeneration = nGeneration;

	sirGraphicsBindingDesc_t szBindings[2] = {};
	szBindings[0].nBinding = 0;
	szBindings[0].nSpace = 0;
	szBindings[0].nArrayCount = 1;
	szBindings[0].nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	szBindings[0].eType = SIR_GRAPHICS_BINDING_STORAGE_BUFFER;
	szBindings[1].nBinding = 1;
	szBindings[1].nSpace = 0;
	szBindings[1].nArrayCount = 1;
	szBindings[1].nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	szBindings[1].eType = SIR_GRAPHICS_BINDING_CONSTANT_BUFFER;

	sirGraphicsPipelineDesc_t compute = {};
	sirGraphicsABIModernRecipe_t computeRecipe = {};
	computeRecipe.nSize = sizeof( computeRecipe );
	computeRecipe.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	computeRecipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	compute.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	compute.pModern = &computeRecipe;
	compute.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	compute.nSampleCount = 1;
	compute.computeShader = computeShader;
	compute.pBindings = szBindings;
	compute.nBindingCount = 2;
	sirGraphicsHandle_t hCompute = {};
	if ( pDevice->createPipeline( compute, &hCompute ) != SIR_GRAPHICS_SUCCESS || hCompute.eType != SIR_GRAPHICS_HANDLE_PIPELINE ||
	     hCompute.nDeviceGeneration != nGeneration || hCompute.nGeneration == 0 ) {
		return 10;
	}

	// Different UAV binding types collide in register u0/space0.
	szBindings[1].nBinding = 0;
	szBindings[1].eType = SIR_GRAPHICS_BINDING_STORAGE_TEXTURE;
	sirGraphicsHandle_t hRejected = {};
	if ( pDevice->createPipeline( compute, &hRejected ) != SIR_GRAPHICS_INVALID_ARGUMENT || hRejected.nGeneration != 0 ) {
		return 11;
	}
	szBindings[1].eType = SIR_GRAPHICS_BINDING_CONSTANT_BUFFER;
	szBindings[1].nBinding = 1;
	szBindings[1].nArrayCount = 129;
	if ( pDevice->createPipeline( compute, &hRejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 12;
	}
	szBindings[1].nArrayCount = 1;
	compute.computeShader.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	if ( pDevice->createPipeline( compute, &hRejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 13;
	}
	compute.computeShader = computeShader;

	// The common attribute layout uses POSITION0 and COLOR0.
	// Four-component formats are accepted by the current POSITION.xyz shader.
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
	sirGraphicsPipelineDesc_t graphics = {};
	sirGraphicsABIModernRecipe_t graphicsRecipe = {};
	graphicsRecipe.nSize = sizeof( graphicsRecipe );
	graphicsRecipe.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	graphicsRecipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	graphics.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	graphics.pModern = &graphicsRecipe;
	graphics.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	graphics.vertexShader = vertexShader;
	graphics.pixelShader = pixelShader;
	graphics.pAttributes = szAttributes;
	graphics.nAttributeCount = 2;
	graphics.nColorFormatCount = 1;
	graphics.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	graphics.szBlendStates[0].nColorWriteMask = 0xFU;
	graphics.nSampleCount = 1;
	graphics.eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	graphics.eCullMode = SIR_GRAPHICS_CULL_NONE;
	sirGraphicsHandle_t hGraphics = {};
	if ( pDevice->createPipeline( graphics, &hGraphics ) != SIR_GRAPHICS_SUCCESS || hGraphics.eType != SIR_GRAPHICS_HANDLE_PIPELINE ||
	     hGraphics.nGeneration == 0 ) {
		return 14;
	}

	graphics.nSampleCount = 64;
	if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_UNSUPPORTED || hRejected.nGeneration != 0 ) {
		return 26;
	}
	graphics.nSampleCount = 1;
	graphics.szColorFormats[0] = SIR_GRAPHICS_FORMAT_D16_UNORM;
	if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_UNSUPPORTED || hRejected.nGeneration != 0 ) {
		return 27;
	}
	graphics.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	sirGraphicsCapabilities_t capabilities = {};
	if ( pDevice->queryCapabilities( &capabilities ) != SIR_GRAPHICS_SUCCESS ) {
		return 28;
	}
	if ( capabilities.nMaximumSampleCount > 1 ) {
		graphics.nSampleCount = capabilities.nMaximumSampleCount;
		if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_SUCCESS ||
		     pDevice->retireResource( hRejected, completed ) != SIR_GRAPHICS_SUCCESS ) {
			return 29;
		}
		graphics.nSampleCount = 1;
	}

	szAttributes[1].nStrideBytes = 24;
	if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 15;
	}
	szAttributes[1].nStrideBytes = 32;
	graphics.eTopology = SIR_GRAPHICS_TOPOLOGY_PATCHES;
	if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 16;
	}
	graphics.eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	graphics.bDepthTest = true;
	if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 17;
	}
	graphics.bDepthTest = false;
	graphics.vertexShader.eFormat = SIR_GRAPHICS_SHADER_DXBC;
	if ( pDevice->createPipeline( graphics, &hRejected ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 18;
	}

	// A forged future fence must never destroy a live pipeline.
	sirGraphicsCompletionSet_t future = completed;
	future.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	future.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = UINT64_MAX;
	if ( pDevice->retireResource( hCompute, future ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pDevice->retireResource( hCompute, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hCompute, completed ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 19;
	}
	sirGraphicsHandle_t hReused = {};
	if ( pDevice->createPipeline( compute, &hReused ) != SIR_GRAPHICS_SUCCESS || hReused.nSlot != hCompute.nSlot ||
	     hReused.nGeneration == hCompute.nGeneration ) {
		return 20;
	}
	if ( pDevice->retireResource( hGraphics, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReused, completed ) != SIR_GRAPHICS_SUCCESS ) {
		return 21;
	}

	// Native PSO capacity is sealed. Saturation returns a typed result.
	sirGraphicsHandle_t szPipelines[64] = {};
	for ( uint32_t iPipeline = 0; iPipeline < 64; ++iPipeline ) {
		if ( pDevice->createPipeline( compute, &szPipelines[iPipeline] ) != SIR_GRAPHICS_SUCCESS ) {
			return 22;
		}
	}
	if ( pDevice->createPipeline( compute, &hRejected ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || hRejected.nGeneration != 0 ) {
		return 23;
	}
	for ( uint32_t iPipeline = 0; iPipeline < 64; ++iPipeline ) {
		if ( pDevice->retireResource( szPipelines[iPipeline], completed ) != SIR_GRAPHICS_SUCCESS ) {
			return 24;
		}
	}
	if ( pDevice->createPipeline( compute, &hReused ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReused, completed ) != SIR_GRAPHICS_SUCCESS ) {
		return 25;
	}
	return 0;
}

static int sirTestNativePipelineCache( SIRD3D12Driver& driver, SIRRenderingDevice **ppDevice,
                                       const sirGraphicsDeviceCreateDesc_t& config,
                                       const sirGraphicsShaderCode_t& shader ) noexcept
{
	if ( ppDevice == nullptr || *ppDevice == nullptr ) {
		return 30;
	}
	SIRD3D12RenderingDevice *pNative = static_cast<SIRD3D12RenderingDevice *>( *ppDevice );
	sirD3D12PipelineCacheStats_t stats = {};
	if ( pNative->queryNativePipelineCacheStats( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pNative->queryNativePipelineCacheStats( &stats ) != SIR_GRAPHICS_SUCCESS ) {
		return 31;
	}
	if ( stats.nRootReuses == 0 ) {
		return 45;
	}
	if ( !stats.bAvailable ) {
		uint64_t nSize = 0;
		return pNative->serializeNativePipelineCache( nullptr, &nSize ) == SIR_GRAPHICS_UNSUPPORTED ? 0 : 32;
	}
	if ( stats.nCacheStores == 0 || stats.nCacheHits == 0 || stats.nCacheMisses == 0 ) {
		return 33;
	}
	uint64_t nSize = 0;
	if ( pNative->serializeNativePipelineCache( nullptr, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pNative->serializeNativePipelineCache( nullptr, &nSize ) != SIR_GRAPHICS_SUCCESS || nSize == 0 ||
	     nSize > 16ULL * 1024ULL * 1024ULL ) {
		return 34;
	}
	void *pBlob = malloc( ( size_t )nSize );
	if ( pBlob == nullptr ) {
		return 35;
	}
	uint64_t nShortSize = nSize - 1;
	if ( pNative->serializeNativePipelineCache( pBlob, &nShortSize ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ||
	     nShortSize != nSize || pNative->serializeNativePipelineCache( pBlob, &nSize ) != SIR_GRAPHICS_SUCCESS ||
	     pNative->loadNativePipelineCache( pBlob, nSize ) != SIR_GRAPHICS_BUSY ) {
		free( pBlob );
		return 36;
	}
	if ( driver.destroyDevice( *ppDevice ) != SIR_GRAPHICS_SUCCESS ) {
		free( pBlob );
		return 37;
	}
	*ppDevice = nullptr;
	if ( driver.createDevice( config, ppDevice ) != SIR_GRAPHICS_SUCCESS || *ppDevice == nullptr ) {
		free( pBlob );
		return 38;
	}
	pNative = static_cast<SIRD3D12RenderingDevice *>( *ppDevice );
	if ( pNative->loadNativePipelineCache( nullptr, nSize ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pNative->loadNativePipelineCache( pBlob, 0 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		free( pBlob );
		return 39;
	}
	uint8_t *pInvalid = static_cast<uint8_t *>( malloc( ( size_t )nSize ) );
	if ( pInvalid == nullptr ) {
		free( pBlob );
		return 40;
	}
	memset( pInvalid, 0, ( size_t )nSize );
	const sirGraphicsResult_t eCorrupted = pNative->loadNativePipelineCache( pInvalid, nSize );
	free( pInvalid );
	if ( eCorrupted == SIR_GRAPHICS_SUCCESS ) {
		free( pBlob );
		return 41;
	}
	if ( pNative->loadNativePipelineCache( pBlob, nSize ) != SIR_GRAPHICS_SUCCESS ) {
		free( pBlob );
		return 42;
	}
	free( pBlob );
	sirGraphicsABIModernRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsBindingDesc_t bindings[2] = {};
	bindings[0].nBinding = 0;
	bindings[0].nSpace = 0;
	bindings[0].nArrayCount = 1;
	bindings[0].nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	bindings[0].eType = SIR_GRAPHICS_BINDING_STORAGE_BUFFER;
	bindings[1].nBinding = 1;
	bindings[1].nSpace = 0;
	bindings[1].nArrayCount = 1;
	bindings[1].nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	bindings[1].eType = SIR_GRAPHICS_BINDING_CONSTANT_BUFFER;
	sirGraphicsPipelineDesc_t compute = {};
	compute.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	compute.pModern = &recipe;
	compute.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	compute.nSampleCount = 1;
	compute.computeShader = shader;
	compute.pBindings = bindings;
	compute.nBindingCount = 2;
	sirGraphicsHandle_t hPipeline = {};
	if ( ( *ppDevice )->createPipeline( compute, &hPipeline ) != SIR_GRAPHICS_SUCCESS ||
	     pNative->queryNativePipelineCacheStats( &stats ) != SIR_GRAPHICS_SUCCESS || stats.nCacheHits == 0 ) {
		return 43;
	}
	sirGraphicsCompletionSet_t completed = {};
	completed.nDeviceGeneration = ( *ppDevice )->getDeviceGeneration();
	if ( ( *ppDevice )->retireResource( hPipeline, completed ) != SIR_GRAPHICS_SUCCESS ) {
		return 44;
	}
	return 0;
}

int main( void )
{
	SIRD3D12Driver driver;
	uint32_t nCount = 0;
	if ( driver.getAdapterCount( &nCount ) != SIR_GRAPHICS_SUCCESS || nCount == 0 ) {
		return 1;
	}
	uint32_t nWarpIndex = UINT32_MAX;
	for ( uint32_t iAdapter = 0; iAdapter < nCount; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS ) {
			return 2;
		}
		if ( info.bSoftwareAdapter ) {
			nWarpIndex = iAdapter;
		}
	}
	if ( nWarpIndex == UINT32_MAX ) {
		return 3;
	}

	void *pCompute = nullptr;
	void *pVertex = nullptr;
	void *pPixel = nullptr;
	size_t nComputeBytes = 0;
	size_t nVertexBytes = 0;
	size_t nPixelBytes = 0;
	const bool bCompute = sirLoadPipelineDxil( SIR_D3D12_SMOKE_DXIL_PATH, &pCompute, &nComputeBytes );
	const bool bVertex = sirLoadPipelineDxil( SIR_D3D12_VS_DXIL_PATH, &pVertex, &nVertexBytes );
	const bool bPixel = sirLoadPipelineDxil( SIR_D3D12_PS_DXIL_PATH, &pPixel, &nPixelBytes );
	if ( !bCompute || !bVertex || !bPixel ) {
		free( pCompute );
		free( pVertex );
		free( pPixel );
		return 4;
	}
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nWarpIndex;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bHeadless = true;
	config.bAllowSoftware = true;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		free( pCompute );
		free( pVertex );
		free( pPixel );
		return 5;
	}
	sirGraphicsShaderCode_t computeShader = {};
	computeShader.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	computeShader.pBytes = pCompute;
	computeShader.nByteCount = nComputeBytes;
	sirGraphicsShaderCode_t vertexShader = {};
	vertexShader.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	vertexShader.pBytes = pVertex;
	vertexShader.nByteCount = nVertexBytes;
	sirGraphicsShaderCode_t pixelShader = {};
	pixelShader.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	pixelShader.pBytes = pPixel;
	pixelShader.nByteCount = nPixelBytes;

	const uint32_t nGeneration = pDevice->getDeviceGeneration();
	const int nResult = sirTestGeneralPipelines( pDevice, nGeneration, computeShader, vertexShader, pixelShader );
	const int nCacheResult = nResult == 0 ? sirTestNativePipelineCache( driver, &pDevice, config, computeShader ) : 0;
	const sirGraphicsResult_t eShutdown = pDevice == nullptr ? SIR_GRAPHICS_SUCCESS : driver.destroyDevice( pDevice );
	free( pCompute );
	free( pVertex );
	free( pPixel );
	if ( nResult != 0 ) {
		fprintf( stderr, "General D3D12 PSO test failed at stage %d\\n", nResult );
		return nResult;
	}
	if ( nCacheResult != 0 ) {
		fprintf( stderr, "Native D3D12 pipeline-cache test failed at stage %d\\n", nCacheResult );
		return nCacheResult;
	}
	return eShutdown == SIR_GRAPHICS_SUCCESS ? 0 : 6;
}
