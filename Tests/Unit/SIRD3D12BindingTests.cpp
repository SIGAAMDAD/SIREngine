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

static bool sirLoadBindingShader( const char *pszFile, void **ppBytes, size_t *pLength ) noexcept
{
	*ppBytes = nullptr;
	*pLength = 0;
	FILE *pFile = nullptr;
	if ( fopen_s( &pFile, pszFile, "rb" ) != 0 || pFile == nullptr ) {
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
	*ppBytes = pSource;
	*pLength = nRead;
	return true;
}

static int sirTestNativeBindingSet( SIRRenderingDevice *pDevice, const sirGraphicsShaderCode_t &shader ) noexcept
{
	sirGraphicsCompletionSet_t idle = {};
	idle.nDeviceGeneration = pDevice->getDeviceGeneration();
	sirGraphicsBufferDesc_t output = {};
	output.nSizeBytes = 64 * sizeof( uint32_t );
	output.nUsage = SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	output.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hOutput = {};
	if ( pDevice->createBuffer( output, &hOutput ) != SIR_GRAPHICS_SUCCESS ) {
		return 10;
	}
	sirGraphicsBufferDesc_t readback = {};
	readback.nSizeBytes = output.nSizeBytes;
	readback.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	readback.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( readback, &hReadback ) != SIR_GRAPHICS_SUCCESS ) {
		return 11;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = hOutput;
	view.eFormat = SIR_GRAPHICS_FORMAT_R32_UINT;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirGraphicsHandle_t hUav = {};
	if ( pDevice->createView( view, &hUav ) != SIR_GRAPHICS_SUCCESS ) {
		return 12;
	}
	sirGraphicsBindingDesc_t binding = {};
	binding.nBinding = 0;
	binding.nSpace = 0;
	binding.nArrayCount = 1;
	binding.nShaderStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	binding.eType = SIR_GRAPHICS_BINDING_STORAGE_BUFFER;
	sirGraphicsPipelineDesc_t pipeline = {};
	pipeline.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.computeShader = shader;
	pipeline.pBindings = &binding;
	pipeline.nBindingCount = 1;
	sirGraphicsHandle_t hPipeline = {};
	if ( pDevice->createPipeline( pipeline, &hPipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return 13;
	}
	sirGraphicsHandle_t hBad = {};
	if ( pDevice->createBindingSet( hPipeline, 1, &hBad ) != SIR_GRAPHICS_INVALID_ARGUMENT || hBad.nGeneration != 0 ) {
		return 14;
	}
	sirGraphicsHandle_t hSet = {};
	if ( pDevice->createBindingSet( hPipeline, 0, &hSet ) != SIR_GRAPHICS_SUCCESS || hSet.eType != SIR_GRAPHICS_HANDLE_BINDING_SET ) {
		return 15;
	}
	if ( pDevice->retireResource( hPipeline, idle ) != SIR_GRAPHICS_BUSY ) {
		return 16;
	}
	sirGraphicsDescriptorWrite_t write = {};
	write.hResource = hUav;
	write.nBinding = 0;
	write.nArrayIndex = 0;
	write.nOffsetBytes = 1;
	if ( pDevice->updateBindings( hSet, &write, 1 ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 17;
	}
	write.nOffsetBytes = 0;
	write.nBinding = 1;
	if ( pDevice->updateBindings( hSet, &write, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 18;
	}
	write.nBinding = 0;
	if ( pDevice->updateBindings( hSet, &write, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 19;
	}
	if ( pDevice->retireResource( hUav, idle ) != SIR_GRAPHICS_BUSY ) {
		return 29;
	}
	SIRGraphicsCommandList *pCompute = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COMPUTE, &pCompute ) != SIR_GRAPHICS_SUCCESS || pCompute == nullptr ) {
		return 20;
	}
	sirGraphicsBarrier_t transition = {};
	transition.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	transition.hResource = hOutput;
	transition.eBefore = SIR_GRAPHICS_STATE_COMMON;
	transition.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( pCompute->barrier( &transition, 1 ) != SIR_GRAPHICS_SUCCESS || pCompute->bindPipeline( hPipeline ) != SIR_GRAPHICS_SUCCESS ||
	     pCompute->dispatch( 1, 1, 1 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pCompute->bindBindingSet( 1, hSet ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pCompute->bindBindingSet( 0, hSet ) != SIR_GRAPHICS_SUCCESS || pCompute->dispatch( 1, 1, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 21;
	}
	if ( pDevice->updateBindings( hSet, &write, 1 ) != SIR_GRAPHICS_BUSY || pDevice->retireResource( hSet, idle ) != SIR_GRAPHICS_BUSY ) {
		return 22;
	}
	sirGraphicsBarrier_t uavBarrier = {};
	uavBarrier.eType = SIR_GRAPHICS_BARRIER_UAV;
	uavBarrier.hResource = hOutput;
	transition.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	transition.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	if ( pCompute->barrier( &uavBarrier, 1 ) != SIR_GRAPHICS_SUCCESS || pCompute->barrier( &transition, 1 ) != SIR_GRAPHICS_SUCCESS ||
	     pCompute->copyBuffer( hReadback, 0, hOutput, 0, output.nSizeBytes ) != SIR_GRAPHICS_SUCCESS ||
	     pCompute->close() != SIR_GRAPHICS_SUCCESS ) {
		return 23;
	}
	SIRGraphicsCommandList *szCommands[] = { pCompute };
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = szCommands;
	submission.nCommandListCount = 1;
	sirGraphicsCompletion_t completed = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_COMPUTE, submission, &completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( completed, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 24;
	}
	sirGraphicsMappedBuffer_t mapped = {};
	if ( pDevice->mapBuffer( hReadback, 0, output.nSizeBytes, &mapped ) != SIR_GRAPHICS_SUCCESS ) {
		return 25;
	}
	for ( uint32_t i = 0; i < 64; ++i ) {
		if ( ( ( const uint32_t * )mapped.pData )[i] != 0x50000000U + 7U * i ) {
			return 26;
		}
	}
	if ( pDevice->retireResource( hSet, idle ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->updateBindings( hSet, &write, 1 ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     pDevice->retireResource( hSet, idle ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 27;
	}
	if ( pDevice->retireResource( hUav, idle ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hPipeline, idle ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hOutput, idle ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReadback, idle ) != SIR_GRAPHICS_SUCCESS ) {
		return 28;
	}
	return 0;
}

int main( void )
{
	void *pBytes = nullptr;
	size_t nBytes = 0;
	if ( !sirLoadBindingShader( SIR_D3D12_BINDING_DXIL_PATH, &pBytes, &nBytes ) ) {
		return 1;
	}
	sirGraphicsShaderCode_t shader = {};
	shader.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	shader.pBytes = pBytes;
	shader.nByteCount = nBytes;
	SIRD3D12Driver driver;
	uint32_t nAdapterCount = 0;
	uint32_t nWarp = UINT32_MAX;
	if ( driver.getAdapterCount( &nAdapterCount ) != SIR_GRAPHICS_SUCCESS ) {
		return 2;
	}
	for ( uint32_t iAdapter = 0; iAdapter < nAdapterCount; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS ) {
			return 3;
		}
		if ( info.bSoftwareAdapter ) {
			nWarp = iAdapter;
		}
	}
	if ( nWarp == UINT32_MAX ) {
		return 4;
	}
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nWarp;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bAllowSoftware = true;
	config.bHeadless = true;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 5;
	}
	const int nResult = sirTestNativeBindingSet( pDevice, shader );
	const sirGraphicsResult_t eShutdown = driver.destroyDevice( pDevice );
	free( pBytes );
	if ( nResult != 0 || eShutdown != SIR_GRAPHICS_SUCCESS ) {
		fprintf( stderr, "D3D12 binding-set regression failed at step %d, shutdown %u\n", nResult, ( unsigned )eShutdown );
		return nResult != 0 ? nResult : 6;
	}
	return 0;
}
