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

static bool sirD3D12CheckStatusMapping( void ) noexcept
{
	return sirD3D12ClassifyStatus( S_OK ) == SIR_GRAPHICS_SUCCESS && sirD3D12ClassifyStatus( S_FALSE ) == SIR_GRAPHICS_SUCCESS &&
	       sirD3D12ClassifyStatus( DXGI_ERROR_DEVICE_REMOVED ) == SIR_GRAPHICS_DEVICE_LOST &&
	       sirD3D12ClassifyStatus( DXGI_ERROR_DEVICE_RESET ) == SIR_GRAPHICS_DEVICE_LOST &&
	       sirD3D12ClassifyStatus( DXGI_ERROR_DEVICE_HUNG ) == SIR_GRAPHICS_DEVICE_LOST &&
	       sirD3D12ClassifyStatus( DXGI_ERROR_DRIVER_INTERNAL_ERROR ) == SIR_GRAPHICS_DEVICE_LOST &&
	       sirD3D12ClassifyStatus( E_INVALIDARG ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirD3D12ClassifyStatus( E_POINTER ) == SIR_GRAPHICS_INVALID_ARGUMENT &&
	       sirD3D12ClassifyStatus( E_OUTOFMEMORY ) == SIR_GRAPHICS_OUT_OF_MEMORY &&
	       sirD3D12ClassifyStatus( HRESULT_FROM_WIN32( ERROR_TIMEOUT ) ) == SIR_GRAPHICS_TIMEOUT &&
	       sirD3D12ClassifyStatus( E_FAIL ) == SIR_GRAPHICS_DRIVER_ERROR;
}

static int sirD3D12RunStress( SIRRenderingDevice *pDevice, uint32_t nCycles ) noexcept
{
	const uint32_t nGeneration = pDevice->getDeviceGeneration();
	sirGraphicsMemoryBudget_t before = {};
	if ( pDevice->queryMemoryBudget( &before ) != SIR_GRAPHICS_SUCCESS || before.nCommittedBytes == 0 ) {
		return 20;
	}
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = 4096;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t hUpload = {};
	if ( pDevice->createBuffer( desc, &hUpload ) != SIR_GRAPHICS_SUCCESS ) {
		return 21;
	}
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirGraphicsHandle_t hReadback = {};
	if ( pDevice->createBuffer( desc, &hReadback ) != SIR_GRAPHICS_SUCCESS ) {
		return 22;
	}
	sirGraphicsMappedBuffer_t upload = {};
	sirGraphicsMappedBuffer_t readback = {};
	if ( pDevice->mapBuffer( hUpload, 0, 4096, &upload ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->mapBuffer( hReadback, 0, 4096, &readback ) != SIR_GRAPHICS_SUCCESS ) {
		return 23;
	}
	for ( uint32_t iCycle = 0; iCycle < nCycles; ++iCycle ) {
		const uint32_t nSeed = iCycle * 2654435761U ^ 0xA5A511EEU;
		for ( uint32_t iByte = 0; iByte < 4096; ++iByte ) {
			( ( uint8_t * )upload.pData )[iByte] = ( uint8_t )( nSeed ^ iByte ^ ( iByte >> 3 ) );
		}
		SIRGraphicsCommandList *pCopy = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COPY, &pCopy ) != SIR_GRAPHICS_SUCCESS || pCopy == nullptr ||
		     pCopy->copyBuffer( hReadback, 0, hUpload, 0, 4096 ) != SIR_GRAPHICS_SUCCESS || pCopy->close() != SIR_GRAPHICS_SUCCESS ) {
			return 24;
		}
		sirGraphicsSubmission_t submission = {};
		submission.ppCommandLists = &pCopy;
		submission.nCommandListCount = 1;
		sirGraphicsCompletion_t copyDone = {};
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_COPY, submission, &copyDone ) != SIR_GRAPHICS_SUCCESS ) {
			return 25;
		}
		SIRGraphicsCommandList *pCompute = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_COMPUTE, &pCompute ) != SIR_GRAPHICS_SUCCESS || pCompute == nullptr ||
		     pCompute->close() != SIR_GRAPHICS_SUCCESS ) {
			return 26;
		}
		submission.ppCommandLists = &pCompute;
		submission.pWaits = &copyDone;
		submission.nWaitCount = 1;
		sirGraphicsCompletion_t computeDone = {};
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_COMPUTE, submission, &computeDone ) != SIR_GRAPHICS_SUCCESS ) {
			return 27;
		}
		SIRGraphicsCommandList *pDirect = nullptr;
		if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pDirect ) != SIR_GRAPHICS_SUCCESS || pDirect == nullptr ||
		     pDirect->close() != SIR_GRAPHICS_SUCCESS ) {
			return 28;
		}
		submission.ppCommandLists = &pDirect;
		submission.pWaits = &computeDone;
		sirGraphicsCompletion_t directDone = {};
		if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &directDone ) != SIR_GRAPHICS_SUCCESS ||
		     pDevice->wait( directDone, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
			return 29;
		}
		bool bCopyDone = false;
		bool bComputeDone = false;
		if ( pDevice->poll( copyDone, &bCopyDone ) != SIR_GRAPHICS_SUCCESS || !bCopyDone ||
		     pDevice->poll( computeDone, &bComputeDone ) != SIR_GRAPHICS_SUCCESS || !bComputeDone ) {
			return 30;
		}
		for ( uint32_t iByte = 0; iByte < 4096; ++iByte ) {
			const uint8_t nExpected = ( uint8_t )( nSeed ^ iByte ^ ( iByte >> 3 ) );
			if ( ( ( const uint8_t * )readback.pData )[iByte] != nExpected ) {
				return 31;
			}
		}
	}
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = nGeneration;
	if ( pDevice->retireResource( hReadback, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hUpload, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hUpload, immediate ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 32;
	}
	sirGraphicsMemoryBudget_t after = {};
	if ( pDevice->queryMemoryBudget( &after ) != SIR_GRAPHICS_SUCCESS || after.nCommittedBytes > after.nPeakCommittedBytes ||
	     after.nPeakCommittedBytes < before.nPeakCommittedBytes ) {
		return 33;
	}
	return 0;
}

int main( int argc, char **argv )
{
	if ( !sirD3D12CheckStatusMapping() ) {
		return 1;
	}
	bool bHardware = false;
	bool bDebug = false;
	bool bDred = false;
	bool bGpuValidation = false;
	uint32_t nCycles = 64;
	for ( int iArgument = 1; iArgument < argc; ++iArgument ) {
		if ( strcmp( argv[iArgument], "--hardware" ) == 0 ) {
			bHardware = true;
		} else if ( strcmp( argv[iArgument], "--debug" ) == 0 ) {
			bDebug = true;
		} else if ( strcmp( argv[iArgument], "--dred" ) == 0 ) {
			bDred = true;
		} else if ( strcmp( argv[iArgument], "--gpu-validation" ) == 0 ) {
			bDebug = true;
			bGpuValidation = true;
		} else if ( strcmp( argv[iArgument], "--cycles" ) == 0 && iArgument + 1 < argc ) {
			const unsigned long nParsed = strtoul( argv[++iArgument], nullptr, 10 );
			if ( nParsed == 0 || nParsed > 8192 ) {
				return 2;
			}
			nCycles = ( uint32_t )nParsed;
		} else {
			return 3;
		}
	}
	SIRD3D12Driver driver;
	uint32_t nAdapters = 0;
	if ( driver.getAdapterCount( &nAdapters ) != SIR_GRAPHICS_SUCCESS ) {
		return 4;
	}
	uint32_t nSelected = UINT32_MAX;
	for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS ) {
			return 5;
		}
		if ( ( info.bSoftwareAdapter == false ) == bHardware ) {
			nSelected = iAdapter;
			break;
		}
	}
	if ( nSelected == UINT32_MAX ) {
		fprintf( stderr, "Requested D3D12 adapter type unavailable\n" );
		return bHardware ? 6 : 77;
	}
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nSelected;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bAllowSoftware = !bHardware;
	config.bHeadless = true;
	SIRRenderingDevice *pDevice = nullptr;
	config.bEnableGpuValidation = true;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT || pDevice != nullptr ) {
		return 7;
	}
	config.bEnableGpuValidation = bGpuValidation;
	config.bEnableApiValidation = bDebug;
	config.bEnableDeviceFaultDiagnostics = bDred;
	const sirGraphicsResult_t eCreated = driver.createDevice( config, &pDevice );
	if ( ( bDebug || bDred ) && eCreated == SIR_GRAPHICS_UNSUPPORTED && pDevice == nullptr ) {
		fprintf( stderr, "Requested D3D12 Graphics Tools diagnostics unavailable\n" );
		return 77;
	}
	if ( eCreated != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 8;
	}
	SIRD3D12RenderingDevice *pNative = ( SIRD3D12RenderingDevice * )pDevice;
	sirD3D12DiagnosticSnapshot_t snapshot = {};
	if ( pNative->queryNativeDiagnostics( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pNative->queryNativeDiagnostics( &snapshot ) != SIR_GRAPHICS_SUCCESS || snapshot.bDeviceRemoved ||
	     snapshot.bDredEnabled != bDred || snapshot.bDebugLayerEnabled != bDebug || snapshot.bGpuValidationEnabled != bGpuValidation ||
	     snapshot.nDeviceRemovedReason != S_OK ) {
		return 9;
	}
	int nFailure = sirD3D12RunStress( pDevice, nCycles );
	snapshot = {};
	if ( nFailure == 0 && ( pNative->queryNativeDiagnostics( &snapshot ) != SIR_GRAPHICS_SUCCESS || snapshot.bDeviceRemoved ) ) {
		nFailure = 10;
	}
	const sirGraphicsResult_t eShutdown = driver.destroyDevice( pDevice );
	if ( nFailure != 0 || eShutdown != SIR_GRAPHICS_SUCCESS ) {
		fprintf( stderr, "D3D12 phase06 failed at stage %d, shutdown %u\n", nFailure, ( unsigned )eShutdown );
		return nFailure != 0 ? nFailure : 11;
	}
	printf( "D3D12 phase06 %s correctness: %u COPY->COMPUTE->DIRECT cycles passed\n", bHardware ? "hardware" : "WARP", nCycles );
	return 0;
}
