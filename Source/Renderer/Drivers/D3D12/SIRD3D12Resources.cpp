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

#include <Renderer/Drivers/D3D12/SIRD3D12Resources.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>
#include <string.h>

static DXGI_FORMAT sirD3D12Format( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case SIR_GRAPHICS_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM;
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM;
	case SIR_GRAPHICS_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_UNORM;
	case SIR_GRAPHICS_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static uint32_t sirD3D12FormatBytes( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return 1;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return 2;
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
	case SIR_GRAPHICS_FORMAT_R32_UINT:
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return 4;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return 8;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return 16;
	default:
		return 0;
	}
}

static bool sirD3D12DepthFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	return eFormat == SIR_GRAPHICS_FORMAT_D16_UNORM || eFormat == SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ||
	       eFormat == SIR_GRAPHICS_FORMAT_D32_FLOAT;
}

static bool sirD3D12BlockCompressed( sirGraphicsFormat_t eFormat ) noexcept
{
	return eFormat >= SIR_GRAPHICS_FORMAT_BC1_UNORM && eFormat <= SIR_GRAPHICS_FORMAT_BC7_UNORM;
}

static D3D12_TEXTURE_ADDRESS_MODE sirD3D12Address( sirGraphicsAddressMode_t eMode ) noexcept
{
	switch ( eMode ) {
	case SIR_GRAPHICS_ADDRESS_MIRROR:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SIR_GRAPHICS_ADDRESS_CLAMP:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SIR_GRAPHICS_ADDRESS_BORDER:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	default:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

SIRD3D12Resources::~SIRD3D12Resources( void ) noexcept
{
	// The owning shared device must prove queue completion before destruction.
	// Unknown completion means retaining the entire native allocation chain.
	shutdown( false );
}

bool SIRD3D12Resources::initialize( SIRD3D12Device *pNative, uint32_t nGeneration, uint64_t nBudgetBytes ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || !pNative->isReady() || nGeneration == 0 ) {
		return false;
	}
	m_pNative = pNative;
	m_pDevice = pNative->getNativeDevice();
	m_pMemory = pNative->getMemory();
	m_nDeviceGeneration = nGeneration;
	m_nBudgetBytes = nBudgetBytes;
	if ( m_pDevice == nullptr || m_pMemory == nullptr ) {
		shutdown( true );
		return false;
	}

	for ( uint32_t iHeap = 0; iHeap < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++iHeap ) {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = ( D3D12_DESCRIPTOR_HEAP_TYPE )iHeap;
		desc.NumDescriptors = s_nDescriptorCapacity;
		desc.Flags =
		    iHeap <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		if ( FAILED( m_pDevice->CreateDescriptorHeap( &desc, IID_PPV_ARGS( m_szDescriptorHeaps[iHeap].GetAddressOf() ) ) ) ) {
			shutdown( true );
			return false;
		}
		m_szDescriptorSizes[iHeap] = m_pDevice->GetDescriptorHandleIncrementSize( desc.Type );
	}
	return true;
}

void SIRD3D12Resources::shutdown( bool bGpuComplete ) noexcept
{
	if ( bGpuComplete ) {
		for ( uint32_t iSlot = 0; iSlot < s_nSlotCapacity; ++iSlot ) {
			if ( m_szSlots[iSlot].eType == SIR_GRAPHICS_HANDLE_VIEW || m_szSlots[iSlot].eType == SIR_GRAPHICS_HANDLE_SAMPLER ) {
				releaseSlot( iSlot );
			}
		}
		for ( uint32_t iSlot = 0; iSlot < s_nSlotCapacity; ++iSlot ) {
			releaseSlot( iSlot );
		}
		for ( uint32_t iHeap = 0; iHeap < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++iHeap ) {
			m_szDescriptorHeaps[iHeap].Reset();
		}
	} else {
		// Retain heap COM references and allocation owners on uncertain GPU completion.
		for ( uint32_t iHeap = 0; iHeap < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++iHeap ) {
			( void )m_szDescriptorHeaps[iHeap].Detach();
		}
	}
	memset( m_szSlots, 0, sizeof( m_szSlots ) );
	memset( m_szDescriptorsUsed, 0, sizeof( m_szDescriptorsUsed ) );
	memset( m_szDescriptorSizes, 0, sizeof( m_szDescriptorSizes ) );
	m_pNative = nullptr;
	m_pDevice = nullptr;
	m_pMemory = nullptr;
	m_nDeviceGeneration = 0;
	m_nBudgetBytes = 0;
	m_nAllocatedBytes = 0;
	m_nExternalBytes = 0;
	m_nPeakAllocatedBytes = 0;
}

sirGraphicsResult_t SIRD3D12Resources::acquireSlot( sirGraphicsHandleType_t eType, uint32_t *pIndex, sirGraphicsHandle_t *pHandle ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < s_nSlotCapacity; ++iSlot ) {
		sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
		if ( slot.bLive || slot.bRetiring || slot.nGeneration == UINT32_MAX ) {
			continue;
		}
		const uint32_t nNextGeneration = slot.nGeneration + 1;
		slot = {};
		slot.nGeneration = nNextGeneration;
		slot.eType = eType;
		slot.bLive = true;
		*pIndex = iSlot;
		*pHandle = { iSlot, nNextGeneration, m_nDeviceGeneration, eType };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirD3D12ResourceSlot_t *SIRD3D12Resources::findSlot( sirGraphicsHandle_t hHandle, sirGraphicsHandleType_t eType ) noexcept
{
	if ( m_pNative == nullptr || hHandle.nSlot >= s_nSlotCapacity || !sirGraphicsIsHandleValid( hHandle, eType, m_nDeviceGeneration ) ) {
		return nullptr;
	}
	sirD3D12ResourceSlot_t& slot = m_szSlots[hHandle.nSlot];
	if ( !slot.bLive || slot.nGeneration != hHandle.nGeneration || slot.eType != hHandle.eType ) {
		return nullptr;
	}
	return &slot;
}

sirGraphicsResult_t SIRD3D12Resources::reserveDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE eHeap, uint32_t *pIndex,
                                                          D3D12_CPU_DESCRIPTOR_HANDLE *pCpu ) noexcept
{
	if ( eHeap >= D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES || m_szDescriptorHeaps[eHeap] == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	for ( uint32_t iDescriptor = 0; iDescriptor < s_nDescriptorCapacity; ++iDescriptor ) {
		if ( m_szDescriptorsUsed[eHeap][iDescriptor] != 0 ) {
			continue;
		}
		m_szDescriptorsUsed[eHeap][iDescriptor] = 1;
		*pIndex = iDescriptor;
		*pCpu = m_szDescriptorHeaps[eHeap]->GetCPUDescriptorHandleForHeapStart();
		pCpu->ptr += ( SIZE_T )iDescriptor * m_szDescriptorSizes[eHeap];
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

void SIRD3D12Resources::releaseSlot( uint32_t iSlot ) noexcept
{
	sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
	if ( !slot.bLive && !slot.bRetiring ) {
		return;
	}
	if ( slot.eType == SIR_GRAPHICS_HANDLE_VIEW || slot.eType == SIR_GRAPHICS_HANDLE_SAMPLER ) {
		m_szDescriptorsUsed[slot.eDescriptorHeap][slot.iDescriptor] = 0;
		if ( slot.eType == SIR_GRAPHICS_HANDLE_VIEW && slot.hParent.nSlot < s_nSlotCapacity ) {
			sirD3D12ResourceSlot_t& parent = m_szSlots[slot.hParent.nSlot];
			if ( parent.nGeneration == slot.hParent.nGeneration && parent.nDependentViews > 0 ) {
				--parent.nDependentViews;
			}
		}
	} else if ( slot.pAllocation != nullptr ) {
		if ( slot.pMappedBytes != nullptr ) {
			D3D12_RANGE empty = { 0, 0 };
			slot.pAllocation->GetResource()->Unmap( 0, slot.eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK ? &empty : nullptr );
		}
		if ( m_nAllocatedBytes >= slot.nAllocationBytes ) {
			m_nAllocatedBytes -= slot.nAllocationBytes;
		}
		slot.pAllocation->Release();
	} else if ( slot.pExternalResource != nullptr ) {
		if ( m_nAllocatedBytes >= slot.nAllocationBytes ) {
			m_nAllocatedBytes -= slot.nAllocationBytes;
		}
		if ( m_nExternalBytes >= slot.nAllocationBytes ) {
			m_nExternalBytes -= slot.nAllocationBytes;
		}
		m_pMemory->releaseExternalBytes( slot.nAllocationBytes );
		slot.pExternalResource->Release();
	}
	const uint32_t nGeneration = slot.nGeneration;
	slot = {};
	slot.nGeneration = nGeneration;
}

const sirD3D12ResourceSlot_t *SIRD3D12Resources::querySlot( sirGraphicsHandle_t hHandle ) noexcept
{
	return findSlot( hHandle, SIR_GRAPHICS_HANDLE_NONE );
}

bool SIRD3D12Resources::queryDescriptor( sirGraphicsHandle_t hView, D3D12_DESCRIPTOR_HEAP_TYPE eHeap,
                                         D3D12_CPU_DESCRIPTOR_HANDLE *pDescriptor ) noexcept
{
	if ( pDescriptor == nullptr ) {
		return false;
	}
	*pDescriptor = {};
	sirD3D12ResourceSlot_t *pSlot = findSlot( hView, SIR_GRAPHICS_HANDLE_NONE );
	if ( pSlot == nullptr || ( pSlot->eType != SIR_GRAPHICS_HANDLE_VIEW && pSlot->eType != SIR_GRAPHICS_HANDLE_SAMPLER ) ||
	     pSlot->eDescriptorHeap != eHeap || m_szDescriptorHeaps[eHeap] == nullptr ) {
		return false;
	}
	*pDescriptor = m_szDescriptorHeaps[eHeap]->GetCPUDescriptorHandleForHeapStart();
	pDescriptor->ptr += ( SIZE_T )pSlot->iDescriptor * m_szDescriptorSizes[eHeap];
	return true;
}

sirGraphicsResult_t SIRD3D12Resources::reserveDescriptorSpan( D3D12_DESCRIPTOR_HEAP_TYPE eHeap, uint32_t nCount, uint32_t *pIndex,
                                                              D3D12_CPU_DESCRIPTOR_HANDLE *pCpu,
                                                              D3D12_GPU_DESCRIPTOR_HANDLE *pGpu ) noexcept
{
	if ( pIndex == nullptr || pCpu == nullptr || pGpu == nullptr || nCount == 0 || nCount > s_nDescriptorCapacity ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pIndex = 0;
	*pCpu = {};
	*pGpu = {};
	if ( eHeap != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && eHeap != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( m_pDevice == nullptr || m_szDescriptorHeaps[eHeap] == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	for ( uint32_t iStart = 0; iStart + nCount <= s_nDescriptorCapacity; ++iStart ) {
		bool bAvailable = true;
		for ( uint32_t i = 0; i < nCount; ++i ) {
			if ( m_szDescriptorsUsed[eHeap][iStart + i] != 0 ) {
				bAvailable = false;
				break;
			}
		}
		if ( !bAvailable ) {
			continue;
		}
		for ( uint32_t i = 0; i < nCount; ++i ) {
			m_szDescriptorsUsed[eHeap][iStart + i] = 1;
		}
		*pIndex = iStart;
		*pCpu = m_szDescriptorHeaps[eHeap]->GetCPUDescriptorHandleForHeapStart();
		pCpu->ptr += ( SIZE_T )iStart * m_szDescriptorSizes[eHeap];
		*pGpu = m_szDescriptorHeaps[eHeap]->GetGPUDescriptorHandleForHeapStart();
		pGpu->ptr += ( UINT64 )iStart * m_szDescriptorSizes[eHeap];
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

void SIRD3D12Resources::releaseDescriptorSpan( D3D12_DESCRIPTOR_HEAP_TYPE eHeap, uint32_t iStart, uint32_t nCount ) noexcept
{
	if ( ( eHeap != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && eHeap != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) ||
	     iStart >= s_nDescriptorCapacity || nCount > s_nDescriptorCapacity - iStart ) {
		return;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		m_szDescriptorsUsed[eHeap][iStart + i] = 0;
	}
}

ID3D12DescriptorHeap *SIRD3D12Resources::getShaderVisibleHeap( D3D12_DESCRIPTOR_HEAP_TYPE eHeap ) noexcept
{
	return ( eHeap == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || eHeap == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER )
	           ? m_szDescriptorHeaps[eHeap].Get()
			   : nullptr;
}

sirGraphicsResult_t SIRD3D12Resources::retainCommandReference( sirGraphicsHandle_t hHandle ) noexcept
{
	sirD3D12ResourceSlot_t *pSlot = findSlot( hHandle, SIR_GRAPHICS_HANDLE_NONE );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->nCommandReferences == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	++pSlot->nCommandReferences;
	return SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12Resources::releaseCommandReference( sirGraphicsHandle_t hHandle ) noexcept
{
	sirD3D12ResourceSlot_t *pSlot = findSlot( hHandle, SIR_GRAPHICS_HANDLE_NONE );
	if ( pSlot != nullptr && pSlot->nCommandReferences != 0 ) {
		--pSlot->nCommandReferences;
	}
}

bool SIRD3D12Resources::validateRange( const sirD3D12ResourceSlot_t& slot, uint64_t nOffset, uint64_t nBytes ) const noexcept
{
	return nBytes != 0 && nOffset <= slot.nSizeBytes && nBytes <= slot.nSizeBytes - nOffset;
}

bool SIRD3D12Resources::pollRetirement( const sirD3D12CompletionSet_t& completion, bool *pCompleted ) noexcept
{
	return m_pNative != nullptr && m_pNative->pollCompletionSet( completion, pCompleted );
}

void SIRD3D12Resources::collectRetired( void ) noexcept
{
	if ( m_pNative == nullptr ) {
		return;
	}
	for ( uint32_t iSlot = 0; iSlot < s_nSlotCapacity; ++iSlot ) {
		sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
		if ( !slot.bRetiring ) {
			continue;
		}
		bool bComplete = false;
		if ( !pollRetirement( slot.retireCompletion, &bComplete ) ) {
			return;
		}
		if ( bComplete ) {
			releaseSlot( iSlot );
		}
	}
}

uint64_t SIRD3D12Resources::getExternalBytes( void ) const noexcept
{
	return m_nExternalBytes;
}

sirGraphicsResult_t SIRD3D12Resources::importExternalTexture( ID3D12Resource *pResource, sirGraphicsFormat_t eFormat,
                                                              sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pDevice == nullptr || pResource == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const D3D12_RESOURCE_DESC desc = pResource->GetDesc();
	if ( desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize != 1 || desc.MipLevels != 1 ||
	     desc.SampleDesc.Count != 1 || desc.Width > UINT32_MAX || desc.Format != sirD3D12Format( eFormat ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const D3D12_RESOURCE_ALLOCATION_INFO info = m_pDevice->GetResourceAllocationInfo( 0, 1, &desc );
	if ( info.SizeInBytes == UINT64_MAX || info.SizeInBytes == 0 || m_nAllocatedBytes > UINT64_MAX - info.SizeInBytes ||
	     ( m_nBudgetBytes != 0 && ( m_nAllocatedBytes > m_nBudgetBytes || info.SizeInBytes > m_nBudgetBytes - m_nAllocatedBytes ) ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( !m_pMemory->reserveExternalBytes( info.SizeInBytes ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	uint32_t iSlot = 0;
	const sirGraphicsResult_t eSlot = acquireSlot( SIR_GRAPHICS_HANDLE_TEXTURE, &iSlot, pHandle );
	if ( eSlot != SIR_GRAPHICS_SUCCESS ) {
		m_pMemory->releaseExternalBytes( info.SizeInBytes );
		return eSlot;
	}
	sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
	pResource->AddRef();
	slot.pExternalResource = pResource;
	slot.nAllocationBytes = info.SizeInBytes;
	slot.nWidth = ( uint32_t )desc.Width;
	slot.nHeight = desc.Height;
	slot.nDepth = 1;
	slot.nLayers = 1;
	slot.nMips = 1;
	slot.nSamples = 1;
	slot.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	slot.eFormat = eFormat;
	slot.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	slot.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	m_nAllocatedBytes += info.SizeInBytes;
	m_nExternalBytes += info.SizeInBytes;
	if ( m_nAllocatedBytes > m_nPeakAllocatedBytes ) {
		m_nPeakAllocatedBytes = m_nAllocatedBytes;
	}
	return SIR_GRAPHICS_SUCCESS;
}

bool SIRD3D12Resources::canReleaseExternalTexture( sirGraphicsHandle_t hTexture ) noexcept
{
	const sirD3D12ResourceSlot_t *pSlot = findSlot( hTexture, SIR_GRAPHICS_HANDLE_TEXTURE );
	return pSlot != nullptr && pSlot->pExternalResource != nullptr && pSlot->nDependentViews == 0 && pSlot->nCommandReferences == 0;
}

sirGraphicsResult_t SIRD3D12Resources::releaseExternalTexture( sirGraphicsHandle_t hTexture ) noexcept
{
	if ( !canReleaseExternalTexture( hTexture ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	releaseSlot( hTexture.nSlot );
	return SIR_GRAPHICS_SUCCESS;
}

uint64_t SIRD3D12Resources::getAllocatedBytes( void ) const noexcept
{
	return m_nAllocatedBytes;
}

uint64_t SIRD3D12Resources::getPeakAllocatedBytes( void ) const noexcept
{
	return m_nPeakAllocatedBytes;
}

uint64_t SIRD3D12Resources::getMappedUploadBytes( void ) const noexcept
{
	uint64_t nBytes = 0;
	for ( uint32_t iSlot = 0; iSlot < s_nSlotCapacity; ++iSlot ) {
		const sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
		if ( slot.pMappedBytes != nullptr && slot.eMemory != SIR_GRAPHICS_MEMORY_CPU_READBACK ) {
			nBytes += slot.nSizeBytes;
		}
	}
	return nBytes;
}

uint64_t SIRD3D12Resources::getMappedReadbackBytes( void ) const noexcept
{
	uint64_t nBytes = 0;
	for ( uint32_t iSlot = 0; iSlot < s_nSlotCapacity; ++iSlot ) {
		const sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
		if ( slot.pMappedBytes != nullptr && slot.eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK ) {
			nBytes += slot.nSizeBytes;
		}
	}
	return nBytes;
}

sirGraphicsResult_t SIRD3D12Resources::createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	const sirGraphicsResult_t eValid = sirGraphicsValidateBuffer( desc );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( m_pNative == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	collectRetired();
	if ( desc.eMemory == SIR_GRAPHICS_MEMORY_TRANSIENT || desc.nAlignmentBytes > D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( m_nBudgetBytes != 0 && ( m_nAllocatedBytes > m_nBudgetBytes || desc.nSizeBytes > m_nBudgetBytes - m_nAllocatedBytes ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	D3D12_HEAP_TYPE eHeap = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_STATES eState = D3D12_RESOURCE_STATE_COMMON;
	if ( desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD || desc.eMemory == SIR_GRAPHICS_MEMORY_GPU_UPLOAD ) {
		eHeap = desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_GPU_UPLOAD;
		eState = D3D12_RESOURCE_STATE_GENERIC_READ;
	} else if ( desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK ) {
		eHeap = D3D12_HEAP_TYPE_READBACK;
		eState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	if ( eHeap != D3D12_HEAP_TYPE_DEFAULT && ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	D3D12MA::Allocation *pAllocation = nullptr;
	const D3D12_RESOURCE_FLAGS eFlags =
	    ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	const HRESULT nResult = m_pMemory->createBuffer( eHeap, eState, desc.nSizeBytes, &pAllocation, eFlags );
	if ( FAILED( nResult ) || pAllocation == nullptr ) {
		return nResult == E_OUTOFMEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
	}
	const uint64_t nAllocated = pAllocation->GetSize();
	if ( nAllocated > UINT64_MAX - m_nAllocatedBytes || ( m_nBudgetBytes != 0 && nAllocated > m_nBudgetBytes - m_nAllocatedBytes ) ) {
		pAllocation->Release();
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	void *pMapped = nullptr;
	if ( eHeap != D3D12_HEAP_TYPE_DEFAULT ) {
		D3D12_RANGE noRead = { 0, 0 };
		const HRESULT nMap = pAllocation->GetResource()->Map( 0, eHeap == D3D12_HEAP_TYPE_READBACK ? nullptr : &noRead, &pMapped );
		if ( FAILED( nMap ) || pMapped == nullptr ) {
			pAllocation->Release();
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	collectRetired();
	uint32_t iSlot = 0;
	const sirGraphicsResult_t eSlot = acquireSlot( SIR_GRAPHICS_HANDLE_BUFFER, &iSlot, pHandle );
	if ( eSlot != SIR_GRAPHICS_SUCCESS ) {
		if ( pMapped != nullptr ) {
			D3D12_RANGE empty = { 0, 0 };
			pAllocation->GetResource()->Unmap( 0, eHeap == D3D12_HEAP_TYPE_READBACK ? &empty : nullptr );
		}
		pAllocation->Release();
		return eSlot;
	}
	sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
	slot.pAllocation = pAllocation;
	slot.pMappedBytes = pMapped;
	slot.nAllocationBytes = nAllocated;
	slot.nSizeBytes = desc.nSizeBytes;
	slot.nUsage = desc.nUsage;
	slot.eMemory = desc.eMemory;
	m_nAllocatedBytes += nAllocated;
	if ( m_nAllocatedBytes > m_nPeakAllocatedBytes ) {
		m_nPeakAllocatedBytes = m_nAllocatedBytes;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Resources::createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	const sirGraphicsResult_t eValid = sirGraphicsValidateTexture( desc );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( m_pNative == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	collectRetired();
	if ( desc.eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL || desc.nWidth > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
	     desc.nHeight > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || desc.nArrayLayers > UINT16_MAX || desc.nDepth > UINT16_MAX ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( desc.nSampleCount > 1 && ( desc.nMipLevels != 1 || desc.eDimension != SIR_GRAPHICS_TEXTURE_2D ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 &&
	     ( desc.nUsage & ( SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_STORAGE ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( sirD3D12DepthFormat( desc.eFormat ) != ( ( desc.nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( sirD3D12BlockCompressed( desc.eFormat ) &&
	     ( desc.nUsage & ( SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	D3D12_RESOURCE_DESC resource = {};
	resource.Dimension = desc.eDimension == SIR_GRAPHICS_TEXTURE_1D   ? D3D12_RESOURCE_DIMENSION_TEXTURE1D
	                     : desc.eDimension == SIR_GRAPHICS_TEXTURE_3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D
	                                                                  : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource.Width = desc.nWidth;
	resource.Height = desc.nHeight;
	resource.DepthOrArraySize = ( UINT16 )( desc.eDimension == SIR_GRAPHICS_TEXTURE_3D ? desc.nDepth : desc.nArrayLayers );
	resource.MipLevels = ( UINT16 )desc.nMipLevels;
	resource.Format = sirD3D12Format( desc.eFormat );
	resource.SampleDesc.Count = desc.nSampleCount;
	resource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource.Flags = D3D12_RESOURCE_FLAG_NONE;
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0 ) {
		resource.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) != 0 ) {
		resource.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		resource.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if ( resource.Format == DXGI_FORMAT_UNKNOWN ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( desc.nSampleCount > 1 && ( ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ||
	                                !sirD3D12SupportsSampleCount( m_pDevice, resource.Format, desc.nSampleCount ) ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	D3D12MA::Allocation *pAllocation = nullptr;
	const HRESULT nResult = m_pMemory->createTexture( resource, D3D12_RESOURCE_STATE_COMMON, nullptr, &pAllocation );
	if ( FAILED( nResult ) || pAllocation == nullptr ) {
		return nResult == E_OUTOFMEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
	}
	const uint64_t nAllocated = pAllocation->GetSize();
	if ( nAllocated > UINT64_MAX - m_nAllocatedBytes ||
	     ( m_nBudgetBytes != 0 && ( m_nAllocatedBytes > m_nBudgetBytes || nAllocated > m_nBudgetBytes - m_nAllocatedBytes ) ) ) {
		pAllocation->Release();
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	collectRetired();
	uint32_t iSlot = 0;
	const sirGraphicsResult_t eSlot = acquireSlot( SIR_GRAPHICS_HANDLE_TEXTURE, &iSlot, pHandle );
	if ( eSlot != SIR_GRAPHICS_SUCCESS ) {
		pAllocation->Release();
		return eSlot;
	}
	sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
	slot.pAllocation = pAllocation;
	slot.nAllocationBytes = nAllocated;
	slot.nUsage = desc.nUsage;
	slot.nWidth = desc.nWidth;
	slot.nHeight = desc.nHeight;
	slot.nDepth = desc.nDepth;
	slot.nLayers = desc.nArrayLayers;
	slot.nMips = desc.nMipLevels;
	slot.nSamples = desc.nSampleCount;
	slot.eMemory = desc.eMemory;
	slot.eDimension = desc.eDimension;
	slot.eFormat = desc.eFormat;
	m_nAllocatedBytes += nAllocated;
	if ( m_nAllocatedBytes > m_nPeakAllocatedBytes ) {
		m_nPeakAllocatedBytes = m_nAllocatedBytes;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Resources::createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( desc.nUsage == 0 || ( desc.nUsage & ( desc.nUsage - 1 ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12ResourceSlot_t *pResource = findSlot( desc.hResource, SIR_GRAPHICS_HANDLE_NONE );
	if ( pResource == nullptr || ( pResource->eType != SIR_GRAPHICS_HANDLE_TEXTURE && pResource->eType != SIR_GRAPHICS_HANDLE_BUFFER ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( ( pResource->nUsage & desc.nUsage ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pResource->eType == SIR_GRAPHICS_HANDLE_BUFFER && desc.nUsage != SIR_GRAPHICS_USAGE_CONSTANT &&
	     desc.nUsage != SIR_GRAPHICS_USAGE_SAMPLED && desc.nUsage != SIR_GRAPHICS_USAGE_STORAGE ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pResource->eType == SIR_GRAPHICS_HANDLE_TEXTURE && desc.nUsage != SIR_GRAPHICS_USAGE_SAMPLED &&
	     desc.nUsage != SIR_GRAPHICS_USAGE_STORAGE && desc.nUsage != SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT &&
	     desc.nUsage != SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	D3D12_DESCRIPTOR_HEAP_TYPE eHeap = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	if ( desc.nUsage == SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) {
		eHeap = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) {
		eHeap = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	}
	collectRetired();
	DXGI_FORMAT eFormat =
	    desc.eFormat == SIR_GRAPHICS_FORMAT_UNKNOWN ? sirD3D12Format( pResource->eFormat ) : sirD3D12Format( desc.eFormat );
	if ( pResource->eType == SIR_GRAPHICS_HANDLE_TEXTURE ) {
		if ( desc.eFormat != SIR_GRAPHICS_FORMAT_UNKNOWN && desc.eFormat != pResource->eFormat ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( desc.nMipLevels == 0 || desc.nBaseMip >= pResource->nMips || desc.nMipLevels > pResource->nMips - desc.nBaseMip ||
		     desc.nLayers == 0 || desc.nBaseLayer >= pResource->nLayers || desc.nLayers > pResource->nLayers - desc.nBaseLayer ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pResource->nSamples != 1 && desc.nUsage == SIR_GRAPHICS_USAGE_STORAGE ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( sirD3D12DepthFormat( pResource->eFormat ) && desc.nUsage != SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pResource->eDimension == SIR_GRAPHICS_TEXTURE_CUBE && ( desc.nBaseLayer % 6 != 0 || desc.nLayers % 6 != 0 ) &&
		     desc.nUsage == SIR_GRAPHICS_USAGE_SAMPLED ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else {
		if ( desc.nBaseMip != 0 || desc.nMipLevels > 1 || desc.nBaseLayer != 0 || desc.nLayers > 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.nUsage != SIR_GRAPHICS_USAGE_CONSTANT &&
		     ( sirD3D12FormatBytes( desc.eFormat ) == 0 || pResource->nSizeBytes % sirD3D12FormatBytes( desc.eFormat ) != 0 ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( desc.nUsage == SIR_GRAPHICS_USAGE_CONSTANT &&
		     ( pResource->nSizeBytes > D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16 ||
		       pResource->nSizeBytes % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0 ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	if ( eFormat == DXGI_FORMAT_UNKNOWN && desc.nUsage != SIR_GRAPHICS_USAGE_CONSTANT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	uint32_t iDescriptor = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE hCpu = {};
	const sirGraphicsResult_t eDescriptor = reserveDescriptor( eHeap, &iDescriptor, &hCpu );
	if ( eDescriptor != SIR_GRAPHICS_SUCCESS ) {
		return eDescriptor;
	}
	uint32_t iSlot = 0;
	const sirGraphicsResult_t eSlot = acquireSlot( SIR_GRAPHICS_HANDLE_VIEW, &iSlot, pHandle );
	if ( eSlot != SIR_GRAPHICS_SUCCESS ) {
		m_szDescriptorsUsed[eHeap][iDescriptor] = 0;
		return eSlot;
	}
	sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
	slot.iDescriptor = iDescriptor;
	slot.eDescriptorHeap = eHeap;
	slot.hParent = desc.hResource;
	slot.nUsage = desc.nUsage;
	++pResource->nDependentViews;
	ID3D12Resource *pNativeResource = pResource->getNativeResource();

	if ( desc.nUsage == SIR_GRAPHICS_USAGE_CONSTANT ) {
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
		cbv.BufferLocation = pNativeResource->GetGPUVirtualAddress();
		cbv.SizeInBytes = ( UINT )pResource->nSizeBytes;
		m_pDevice->CreateConstantBufferView( &cbv, hCpu );
	} else if ( pResource->eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
		const uint32_t nElementBytes = sirD3D12FormatBytes( desc.eFormat );
		const uint64_t nElements = pResource->nSizeBytes / nElementBytes;
		if ( nElements > UINT32_MAX ) {
			releaseSlot( iSlot );
			*pHandle = {};
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( desc.nUsage == SIR_GRAPHICS_USAGE_STORAGE ) {
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
			uav.Format = eFormat;
			uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav.Buffer.NumElements = ( UINT )nElements;
			m_pDevice->CreateUnorderedAccessView( pNativeResource, nullptr, &uav, hCpu );
		} else {
			D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
			srv.Format = eFormat;
			srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv.Buffer.NumElements = ( UINT )nElements;
			m_pDevice->CreateShaderResourceView( pNativeResource, &srv, hCpu );
		}
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_SAMPLED ) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = eFormat;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if ( pResource->eDimension == SIR_GRAPHICS_TEXTURE_3D ) {
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srv.Texture3D.MostDetailedMip = desc.nBaseMip;
			srv.Texture3D.MipLevels = desc.nMipLevels;
		} else if ( pResource->eDimension == SIR_GRAPHICS_TEXTURE_CUBE ) {
			if ( desc.nLayers == 6 && desc.nBaseLayer == 0 ) {
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srv.TextureCube.MostDetailedMip = desc.nBaseMip;
				srv.TextureCube.MipLevels = desc.nMipLevels;
			} else {
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				srv.TextureCubeArray.MostDetailedMip = desc.nBaseMip;
				srv.TextureCubeArray.MipLevels = desc.nMipLevels;
				srv.TextureCubeArray.First2DArrayFace = desc.nBaseLayer;
				srv.TextureCubeArray.NumCubes = desc.nLayers / 6;
			}
		} else if ( pResource->eDimension == SIR_GRAPHICS_TEXTURE_1D ) {
			if ( pResource->nLayers == 1 ) {
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				srv.Texture1D.MostDetailedMip = desc.nBaseMip;
				srv.Texture1D.MipLevels = desc.nMipLevels;
			} else {
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				srv.Texture1DArray.MostDetailedMip = desc.nBaseMip;
				srv.Texture1DArray.MipLevels = desc.nMipLevels;
				srv.Texture1DArray.FirstArraySlice = desc.nBaseLayer;
				srv.Texture1DArray.ArraySize = desc.nLayers;
			}
		} else if ( pResource->nSamples > 1 ) {
			if ( pResource->nLayers == 1 ) {
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			} else {
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
				srv.Texture2DMSArray.FirstArraySlice = desc.nBaseLayer;
				srv.Texture2DMSArray.ArraySize = desc.nLayers;
			}
		} else if ( pResource->nLayers == 1 ) {
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv.Texture2D.MostDetailedMip = desc.nBaseMip;
			srv.Texture2D.MipLevels = desc.nMipLevels;
		} else {
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv.Texture2DArray.MostDetailedMip = desc.nBaseMip;
			srv.Texture2DArray.MipLevels = desc.nMipLevels;
			srv.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
			srv.Texture2DArray.ArraySize = desc.nLayers;
		}
		m_pDevice->CreateShaderResourceView( pNativeResource, &srv, hCpu );
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) {
		if ( pResource->eDimension != SIR_GRAPHICS_TEXTURE_2D || desc.nMipLevels != 1 ) {
			releaseSlot( iSlot );
			*pHandle = {};
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
		rtv.Format = eFormat;
		if ( pResource->nLayers == 1 && pResource->nSamples > 1 ) {
			rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		} else if ( pResource->nLayers == 1 ) {
			rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtv.Texture2D.MipSlice = desc.nBaseMip;
		} else if ( pResource->nSamples > 1 ) {
			rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			rtv.Texture2DMSArray.FirstArraySlice = desc.nBaseLayer;
			rtv.Texture2DMSArray.ArraySize = desc.nLayers;
		} else {
			rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv.Texture2DArray.MipSlice = desc.nBaseMip;
			rtv.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
			rtv.Texture2DArray.ArraySize = desc.nLayers;
		}
		m_pDevice->CreateRenderTargetView( pNativeResource, &rtv, hCpu );
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) {
		if ( pResource->eDimension != SIR_GRAPHICS_TEXTURE_2D || desc.nMipLevels != 1 ) {
			releaseSlot( iSlot );
			*pHandle = {};
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = eFormat;
		if ( pResource->nLayers == 1 && pResource->nSamples > 1 ) {
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		} else if ( pResource->nLayers == 1 ) {
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Texture2D.MipSlice = desc.nBaseMip;
		} else if ( pResource->nSamples > 1 ) {
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
			dsv.Texture2DMSArray.FirstArraySlice = desc.nBaseLayer;
			dsv.Texture2DMSArray.ArraySize = desc.nLayers;
		} else {
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsv.Texture2DArray.MipSlice = desc.nBaseMip;
			dsv.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
			dsv.Texture2DArray.ArraySize = desc.nLayers;
		}
		m_pDevice->CreateDepthStencilView( pNativeResource, &dsv, hCpu );
	} else if ( desc.nUsage == SIR_GRAPHICS_USAGE_STORAGE ) {
		if ( pResource->eDimension != SIR_GRAPHICS_TEXTURE_2D || desc.nMipLevels != 1 || pResource->nSamples != 1 ) {
			releaseSlot( iSlot );
			*pHandle = {};
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
		uav.Format = eFormat;
		if ( pResource->nLayers == 1 ) {
			uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav.Texture2D.MipSlice = desc.nBaseMip;
		} else {
			uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav.Texture2DArray.MipSlice = desc.nBaseMip;
			uav.Texture2DArray.FirstArraySlice = desc.nBaseLayer;
			uav.Texture2DArray.ArraySize = desc.nLayers;
		}
		m_pDevice->CreateUnorderedAccessView( pNativeResource, nullptr, &uav, hCpu );
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Resources::createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	const sirGraphicsResult_t eValid = sirGraphicsValidateSampler( desc );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( m_pNative == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	collectRetired();
	uint32_t iDescriptor = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE hCpu = {};
	const sirGraphicsResult_t eDescriptor = reserveDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, &iDescriptor, &hCpu );
	if ( eDescriptor != SIR_GRAPHICS_SUCCESS ) {
		return eDescriptor;
	}

	uint32_t iSlot = 0;
	const sirGraphicsResult_t eSlot = acquireSlot( SIR_GRAPHICS_HANDLE_SAMPLER, &iSlot, pHandle );
	if ( eSlot != SIR_GRAPHICS_SUCCESS ) {
		m_szDescriptorsUsed[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER][iDescriptor] = 0;
		return eSlot;
	}
	sirD3D12ResourceSlot_t& slot = m_szSlots[iSlot];
	slot.eDescriptorHeap = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	slot.iDescriptor = iDescriptor;
	D3D12_SAMPLER_DESC sampler = {};
	sampler.Filter = desc.eFilter == SIR_GRAPHICS_FILTER_NEAREST     ? D3D12_FILTER_MIN_MAG_MIP_POINT
	                 : desc.eFilter == SIR_GRAPHICS_FILTER_LINEAR    ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT
	                 : desc.eFilter == SIR_GRAPHICS_FILTER_TRILINEAR ? D3D12_FILTER_MIN_MAG_MIP_LINEAR
	                                                                 : D3D12_FILTER_ANISOTROPIC;
	sampler.AddressU = sirD3D12Address( desc.eAddressU );
	sampler.AddressV = sirD3D12Address( desc.eAddressV );
	sampler.AddressW = sirD3D12Address( desc.eAddressW );
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = ( UINT )desc.fMaximumAnisotropy;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.MinLOD = desc.fMinimumLod;
	sampler.MaxLOD = desc.fMaximumLod;
	m_pDevice->CreateSampler( &sampler, hCpu );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Resources::mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
                                                  sirGraphicsMappedBuffer_t *pMapped ) noexcept
{
	if ( pMapped == nullptr || nBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pMapped = {};
	sirD3D12ResourceSlot_t *pBuffer = findSlot( hBuffer, SIR_GRAPHICS_HANDLE_BUFFER );
	if ( pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pBuffer->pMappedBytes == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !validateRange( *pBuffer, nOffset, nBytes ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pMapped->pData = ( uint8_t * )pBuffer->pMappedBytes + nOffset;
	pMapped->nOffset = nOffset;
	pMapped->nBytes = nBytes;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Resources::unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	// Public map/unmap is a logical borrow. Native host-visible heaps stay
	// persistently mapped until the allocation's fence-safe destruction.
	sirD3D12ResourceSlot_t *pBuffer = findSlot( hBuffer, SIR_GRAPHICS_HANDLE_BUFFER );
	if ( pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pBuffer->pMappedBytes == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return validateRange( *pBuffer, nOffset, nBytes ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_INVALID_ARGUMENT;
}

sirGraphicsResult_t SIRD3D12Resources::flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	// D3D12 host-visible upload/readback heaps are coherent. No explicit
	// flush is required; range validation retains the portable contract.
	return unmapBuffer( hBuffer, nOffset, nBytes );
}

sirGraphicsResult_t SIRD3D12Resources::invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	return unmapBuffer( hBuffer, nOffset, nBytes );
}

sirGraphicsResult_t SIRD3D12Resources::retireResource( sirGraphicsHandle_t hHandle, const sirGraphicsCompletionSet_t& completion ) noexcept
{
	sirD3D12ResourceSlot_t *pSlot = findSlot( hHandle, SIR_GRAPHICS_HANDLE_NONE );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->pExternalResource != nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->nDependentViews != 0 || pSlot->nCommandReferences != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirD3D12CompletionSet_t native = {};
	native.nDeviceGeneration = completion.nDeviceGeneration;
	native.nGraphicsFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
	native.nComputeFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE];
	native.nCopyFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY];
	bool bCompleted = false;
	if ( !pollRetirement( native, &bCompleted ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pSlot->bLive = false;
	pSlot->bRetiring = true;
	pSlot->retireCompletion = native;
	if ( bCompleted ) {
		releaseSlot( hHandle.nSlot );
	}
	return SIR_GRAPHICS_SUCCESS;
}
