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
#include <Renderer/SIRGraphicsRegistry.h>
#include <stdio.h>

int main( void )
{
	SIRD3D12Driver driver;
	SIRGraphicsRegistry registry;
	if ( registry.registerDriver( &driver ) != SIR_GRAPHICS_SUCCESS || registry.findDriver( SIR_GRAPHICS_BACKEND_D3D12 ) != &driver ) {
		return 1;
	}

	uint32_t nAdapters = 0;
	if ( driver.getAdapterCount( &nAdapters ) != SIR_GRAPHICS_SUCCESS || nAdapters == 0 || nAdapters > 2 ) {
		return 2;
	}
	uint32_t nSoftwareIndex = UINT32_MAX;
	for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS || info.nAdapterIndex != iAdapter ||
		     info.nApiVersion.nMajor != 12 ) {
			return 3;
		}
		if ( info.bSoftwareAdapter ) {
			nSoftwareIndex = iAdapter;
		}
	}
	if ( nSoftwareIndex == UINT32_MAX ) {
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
	SIRRenderingDevice *pDuplicate = pDevice;
	if ( driver.createDevice( config, &pDuplicate ) != SIR_GRAPHICS_BUSY || pDuplicate != nullptr ) {
		return 7;
	}
	sirGraphicsCapabilities_t capabilities = {};
	if ( pDevice->getBackend() != SIR_GRAPHICS_BACKEND_D3D12 || pDevice->queryCapabilities( &capabilities ) != SIR_GRAPHICS_SUCCESS ||
	     capabilities.nDeviceGeneration == 0 || ( capabilities.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_COPY ) ) == 0 ) {
		return 8;
	}
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 4096;
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t handle = {};
	if ( pDevice->createBuffer( buffer, &handle ) != SIR_GRAPHICS_UNSUPPORTED || handle.nGeneration != 0 ||
	     handle.nDeviceGeneration != 0 ) {
		return 9;
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
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommands ) != SIR_GRAPHICS_UNSUPPORTED || pCommands != nullptr ) {
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
	return 0;
}
