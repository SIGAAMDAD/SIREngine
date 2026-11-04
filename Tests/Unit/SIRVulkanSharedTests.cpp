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
	if ( driver.getBackend() != SIR_GRAPHICS_BACKEND_VULKAN ||
	     registry.registerDriver( &driver ) != SIR_GRAPHICS_SUCCESS ||
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
	create.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
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
	if ( pDevice->getDeviceGeneration() == 0 ||
	     pDevice->queryCapabilities( &capabilities ) != SIR_GRAPHICS_SUCCESS ||
	     capabilities.eBackend != SIR_GRAPHICS_BACKEND_VULKAN ||
	     ( capabilities.nFeatureMask & SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ||
	     ( capabilities.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return 4;
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
	if ( pDevice->mapBuffer( hSource, 0, sizeof( szExpected ), &mapped ) != SIR_GRAPHICS_SUCCESS ||
	     mapped.pData == nullptr || mapped.nBytes != sizeof( szExpected ) ) {
		return 6;
	}
	memcpy( mapped.pData, szExpected, sizeof( szExpected ) );
	if ( pDevice->flushBuffer( hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->unmapBuffer( hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ) {
		return 7;
	}

	SIRGraphicsCommandList *pFirst = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pFirst ) != SIR_GRAPHICS_SUCCESS ||
	     pFirst == nullptr || pFirst->copyBuffer( hGpu, 0, hSource, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pFirst->close() != SIR_GRAPHICS_SUCCESS ) {
		return 8;
	}
	sirGraphicsSubmission_t firstSubmission = {};
	firstSubmission.ppCommandLists = &pFirst;
	firstSubmission.nCommandListCount = 1;
	sirGraphicsCompletion_t first = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, firstSubmission, &first ) != SIR_GRAPHICS_SUCCESS ||
	     first.nValue == 0 ) {
		return 9;
	}

	SIRGraphicsCommandList *pSecond = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pSecond ) != SIR_GRAPHICS_SUCCESS ||
	     pSecond == nullptr || pSecond->copyBuffer( hReadback, 0, hGpu, 0, sizeof( szExpected ) ) != SIR_GRAPHICS_SUCCESS ||
	     pSecond->close() != SIR_GRAPHICS_SUCCESS ) {
		return 10;
	}
	sirGraphicsSubmission_t secondSubmission = {};
	secondSubmission.ppCommandLists = &pSecond;
	secondSubmission.nCommandListCount = 1;
	secondSubmission.pWaits = &first;
	secondSubmission.nWaitCount = 1;
	sirGraphicsCompletion_t second = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, secondSubmission, &second ) != SIR_GRAPHICS_SUCCESS ||
	     second.nValue <= first.nValue || pDevice->wait( second, 5000000000ull ) != SIR_GRAPHICS_SUCCESS ) {
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
	if ( pDevice->retireResource( hSource, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hGpu, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hReadback, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hSource, completed ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 13;
	}
	if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->getDeviceGeneration() != 0 ) {
		return 14;
	}
	printf( "Vulkan shared API headless transfer passed\n" );
	return 0;
}
