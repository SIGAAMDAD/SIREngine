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
#include <Core/Memory/SIRGeneralHeap.h>
#include <stdio.h>

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
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nWarp;
	config.nFramesInFlight = 3;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bAllowSoftware = true;
	config.bHeadless = true;
	SIRRenderingDevice *pDevice = nullptr;

	// Native transfer reservations belong to the same GPU resident budget.
	config.nResidentBudgetBytes = 1;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || pDevice != nullptr ) {
		return 4;
	}

	config.nResidentBudgetBytes = 0;
	SIRGeneralHeap uninitialized;
	sirD3D12NativeDeviceConfig_t nativeConfig = {};
	nativeConfig.pCpuHeap = &uninitialized;
	if ( driver.createDeviceWithConfig( config, nativeConfig, &pDevice ) != SIR_GRAPHICS_DRIVER_ERROR || pDevice != nullptr ) {
		return 5;
	}
	nativeConfig.pCpuHeap = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 6;
	}
	sirGraphicsMemoryBudget_t baseline = {};
	if ( pDevice->queryMemoryBudget( &baseline ) != SIR_GRAPHICS_SUCCESS || baseline.nCommittedBytes == 0 ||
	     baseline.nPeakCommittedBytes < baseline.nCommittedBytes || baseline.nUploadMappedBytes < 3 * 64 * 1024 ||
	     baseline.nReadbackMappedBytes < 3 * 64 * 1024 ) {
		return 7;
	}
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = 4096;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hBuffer = {};
	if ( pDevice->createBuffer( desc, &hBuffer ) != SIR_GRAPHICS_SUCCESS ) {
		return 8;
	}
	sirGraphicsMemoryBudget_t allocated = {};
	if ( pDevice->queryMemoryBudget( &allocated ) != SIR_GRAPHICS_SUCCESS || allocated.nCommittedBytes < baseline.nCommittedBytes ||
	     allocated.nPeakCommittedBytes < allocated.nCommittedBytes ) {
		return 9;
	}
	sirGraphicsCompletionSet_t immediate = {};
	immediate.nDeviceGeneration = pDevice->getDeviceGeneration();
	if ( pDevice->retireResource( hBuffer, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->retireResource( hBuffer, immediate ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 10;
	}
	sirGraphicsMemoryBudget_t released = {};
	if ( pDevice->queryMemoryBudget( &released ) != SIR_GRAPHICS_SUCCESS || released.nCommittedBytes > allocated.nCommittedBytes ||
	     released.nPeakCommittedBytes < allocated.nPeakCommittedBytes ) {
		return 11;
	}
	if ( driver.destroyDevice( pDevice ) != SIR_GRAPHICS_SUCCESS ) {
		return 12;
	}

	// Rejected D3D12MA allocations must report failure, with no owned allocation.
	Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pWarp;
	Microsoft::WRL::ComPtr<ID3D12Device> pNativeDevice;
	if ( FAILED( CreateDXGIFactory2( 0, IID_PPV_ARGS( pFactory.GetAddressOf() ) ) ) ||
	     FAILED( pFactory->EnumWarpAdapter( IID_PPV_ARGS( pWarp.GetAddressOf() ) ) ) ||
	     FAILED( D3D12CreateDevice( pWarp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( pNativeDevice.GetAddressOf() ) ) ) ) {
		return 13;
	}
	SIRD3D12Memory memory;
	if ( !memory.initialize( pWarp.Get(), pNativeDevice.Get() ) || !memory.configureResidentBudget( 1 ) ) {
		return 14;
	}
	D3D12MA::Allocation *pRejected = nullptr;
	const HRESULT nRejected = memory.createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 4096, &pRejected );
	if ( nRejected != E_OUTOFMEMORY || pRejected != nullptr || memory.getLastError() != E_OUTOFMEMORY ) {
		if ( pRejected != nullptr ) {
			pRejected->Release();
		}
		return 15;
	}
	memory.shutdown();
	// Fixed pools must never grow their physical backing after startup.
	SIRD3D12Memory sealed;
	if ( !sealed.initialize( pWarp.Get(), pNativeDevice.Get() ) ) {
		return 16;
	}
	sirD3D12SealedPoolConfig_t profile = {};
	if ( sealed.configureSealedPools( profile ) || sealed.getLastError() != E_INVALIDARG ) {
		return 17;
	}
	profile.nDeviceBufferBytes = 4 * 1024 * 1024;
	profile.nSampledTextureBytes = 4 * 1024 * 1024;
	profile.nAttachmentTextureBytes = 4 * 1024 * 1024;
	profile.nUploadBufferBytes = 4 * 1024 * 1024;
	profile.nReadbackBufferBytes = 4 * 1024 * 1024;
	if ( !sealed.configureSealedPools( profile ) || sealed.configureSealedPools( profile ) || sealed.getLastError() != E_INVALIDARG ) {
		return 18;
	}
	sirD3D12AllocationLedger_t reserved = {};
	if ( !sealed.queryAllocationLedger( &reserved ) || reserved.nBlockBytes < 20ULL * 1024 * 1024 ) {
		return 19;
	}
	if ( sealed.reserveExternalBytes( 4096 ) ) {
		return 34;
	}
	D3D12MA::Allocation *szAllocations[5] = {};
	const bool bCreated =
	    SUCCEEDED( sealed.createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 4096, &szAllocations[0] ) ) &&
	    SUCCEEDED( sealed.createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, 4096, &szAllocations[1] ) ) &&
	    SUCCEEDED( sealed.createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, 4096, &szAllocations[2] ) ) &&
	    SUCCEEDED( sealed.createTexture2D( 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON,
		                                   nullptr, &szAllocations[3] ) ) &&
	    SUCCEEDED( sealed.createTexture2D( 32, 32, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		                                   D3D12_RESOURCE_STATE_COMMON, nullptr, &szAllocations[4] ) );
	bool bPlaced = bCreated;
	for ( uint32_t iResource = 0; iResource < 5; ++iResource ) {
		bPlaced = bPlaced && szAllocations[iResource] != nullptr && szAllocations[iResource]->GetHeap() != nullptr;
	}
	D3D12MA::Allocation *pExcess = nullptr;
	const HRESULT nOversized = sealed.createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, 64ULL * 1024 * 1024, &pExcess );
	D3D12MA::Allocation *pAbsentPool = nullptr;
	const HRESULT nMissing = sealed.createBuffer( D3D12_HEAP_TYPE_GPU_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, 4096, &pAbsentPool );
	sirD3D12AllocationLedger_t stable = {};
	const bool bStable = sealed.queryAllocationLedger( &stable ) && stable.nBlockBytes == reserved.nBlockBytes &&
	                     stable.nCommittedBytes == reserved.nCommittedBytes;
	if ( pExcess != nullptr ) {
		pExcess->Release();
	}
	for ( uint32_t iResource = 0; iResource < 5; ++iResource ) {
		if ( szAllocations[iResource] != nullptr ) {
			szAllocations[iResource]->Release();
		}
	}
	sealed.shutdown();
	if ( !bPlaced || nOversized != E_OUTOFMEMORY || pExcess != nullptr || nMissing != E_OUTOFMEMORY || pAbsentPool != nullptr ||
	     !bStable ) {
		return 20;
	}

	// Native renderer startup must seal the resource pools before exposing the device.
	sirD3D12SealedPoolConfig_t sealedProfile = {};
	sealedProfile.nDeviceBufferBytes = 4 * 1024 * 1024;
	sealedProfile.nSampledTextureBytes = 4 * 1024 * 1024;
	sealedProfile.nAttachmentTextureBytes = 4 * 1024 * 1024;
	sealedProfile.nUploadBufferBytes = 4 * 1024 * 1024;
	sealedProfile.nReadbackBufferBytes = 4 * 1024 * 1024;

	SIRRenderingDevice *pSealedDevice = nullptr;
	if ( driver.createSealedDevice( config, {}, &pSealedDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT || pSealedDevice != nullptr ) {
		return 21;
	}
	config.bHeadless = false;
	if ( driver.createSealedDevice( config, sealedProfile, &pSealedDevice ) != SIR_GRAPHICS_UNSUPPORTED || pSealedDevice != nullptr ) {
		return 22;
	}
	config.bHeadless = true;
	config.nResidentBudgetBytes = 1;
	if ( driver.createSealedDevice( config, sealedProfile, &pSealedDevice ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ||
	     pSealedDevice != nullptr ) {
		return 23;
	}
	config.nResidentBudgetBytes = 0;
	if ( driver.createSealedDevice( config, sealedProfile, &pSealedDevice ) != SIR_GRAPHICS_SUCCESS || pSealedDevice == nullptr ) {
		return 24;
	}
	sirGraphicsMemoryBudget_t sealedBaseline = {};
	if ( pSealedDevice->queryMemoryBudget( &sealedBaseline ) != SIR_GRAPHICS_SUCCESS ||
	     sealedBaseline.nCommittedBytes < 20ULL * 1024 * 1024 ) {
		return 25;
	}
	sirGraphicsNativeSurface_t surface = {};
	sirGraphicsPresentDesc_t present = {};
	sirGraphicsHandle_t hSurface = {};
	if ( pSealedDevice->createSurface( surface, present, &hSurface ) != SIR_GRAPHICS_UNSUPPORTED || hSurface.nGeneration != 0 ) {
		return 26;
	}
	sirGraphicsBufferDesc_t resourceBuffer = {};
	resourceBuffer.nSizeBytes = 4096;
	resourceBuffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	resourceBuffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hGpuBuffer = {};
	if ( pSealedDevice->createBuffer( resourceBuffer, &hGpuBuffer ) != SIR_GRAPHICS_SUCCESS ) {
		return 27;
	}
	sirGraphicsTextureDesc_t resourceTexture = {};
	resourceTexture.nWidth = 32;
	resourceTexture.nHeight = 32;
	resourceTexture.nDepth = 1;
	resourceTexture.nArrayLayers = 1;
	resourceTexture.nMipLevels = 1;
	resourceTexture.nSampleCount = 1;
	resourceTexture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	resourceTexture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	resourceTexture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	resourceTexture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t hSampledTexture = {};
	if ( pSealedDevice->createTexture( resourceTexture, &hSampledTexture ) != SIR_GRAPHICS_SUCCESS ) {
		return 28;
	}
	resourceTexture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirGraphicsHandle_t hAttachmentTexture = {};
	if ( pSealedDevice->createTexture( resourceTexture, &hAttachmentTexture ) != SIR_GRAPHICS_SUCCESS ) {
		return 29;
	}
	resourceBuffer.nSizeBytes = 64ULL * 1024 * 1024;
	sirGraphicsHandle_t hOversized = {};
	if ( pSealedDevice->createBuffer( resourceBuffer, &hOversized ) != SIR_GRAPHICS_OUT_OF_MEMORY || hOversized.nGeneration != 0 ) {
		return 30;
	}
	sirGraphicsMemoryBudget_t sealedAllocated = {};
	if ( pSealedDevice->queryMemoryBudget( &sealedAllocated ) != SIR_GRAPHICS_SUCCESS ||
	     sealedAllocated.nCommittedBytes != sealedBaseline.nCommittedBytes ) {
		return 31;
	}
	sirGraphicsCompletionSet_t sealedComplete = {};
	sealedComplete.nDeviceGeneration = pSealedDevice->getDeviceGeneration();
	if ( pSealedDevice->retireResource( hGpuBuffer, sealedComplete ) != SIR_GRAPHICS_SUCCESS ||
	     pSealedDevice->retireResource( hSampledTexture, sealedComplete ) != SIR_GRAPHICS_SUCCESS ||
	     pSealedDevice->retireResource( hAttachmentTexture, sealedComplete ) != SIR_GRAPHICS_SUCCESS ) {
		return 32;
	}
	sirGraphicsMemoryBudget_t sealedRetired = {};
	if ( pSealedDevice->queryMemoryBudget( &sealedRetired ) != SIR_GRAPHICS_SUCCESS ||
	     sealedRetired.nCommittedBytes != sealedBaseline.nCommittedBytes ||
	     driver.destroyDevice( pSealedDevice ) != SIR_GRAPHICS_SUCCESS ) {
		return 33;
	}
	return 0;
}
