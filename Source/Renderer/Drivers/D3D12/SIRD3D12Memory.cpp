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

#include <Renderer/Drivers/D3D12/SIRD3D12Memory.h>
#include <Core/Memory/SIRGeneralHeap.h>
#include <limits.h>

static constexpr uint32_t SIR_D3D12_SEALED_DEVICE_BUFFER = 0;
static constexpr uint32_t SIR_D3D12_SEALED_SAMPLED_TEXTURE = 1;
static constexpr uint32_t SIR_D3D12_SEALED_ATTACHMENT_TEXTURE = 2;
static constexpr uint32_t SIR_D3D12_SEALED_UPLOAD_BUFFER = 3;
static constexpr uint32_t SIR_D3D12_SEALED_READBACK_BUFFER = 4;
static constexpr uint32_t SIR_D3D12_SEALED_GPU_UPLOAD_BUFFER = 5;

static void *sirD3D12AllocateHostMetadata( size_t nBytes, size_t nAlignment, void *pUser ) noexcept
{
	SIRGeneralHeap *pHeap = ( SIRGeneralHeap * )pUser;
	return pHeap != nullptr ? pHeap->allocAligned( nBytes, nAlignment ) : nullptr;
}

static void sirD3D12FreeHostMetadata( void *pMemory, void *pUser ) noexcept
{
	if ( pMemory != nullptr && pUser != nullptr ) {
		( ( SIRGeneralHeap * )pUser )->free( pMemory );
	}
}

SIRD3D12Memory::SIRD3D12Memory( void ) noexcept
{
}

SIRD3D12Memory::~SIRD3D12Memory( void ) noexcept
{
	shutdown();
}

bool SIRD3D12Memory::initialize( IDXGIAdapter *pAdapter, ID3D12Device *pDevice, SIRGeneralHeap *pCpuHeap ) noexcept
{
	if ( m_pAllocator != nullptr || pAdapter == nullptr || pDevice == nullptr ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	if ( pCpuHeap != nullptr && !pCpuHeap->isConfiguredForDomain( SIR_MEMORY_DOMAIN_RENDERING_CPU ) ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	m_pHostHeap = pCpuHeap;
	m_Callbacks = {};
	if ( m_pHostHeap != nullptr ) {
		m_Callbacks.pAllocate = &sirD3D12AllocateHostMetadata;
		m_Callbacks.pFree = &sirD3D12FreeHostMetadata;
		m_Callbacks.pPrivateData = m_pHostHeap;
	}
	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.pAdapter = pAdapter;
	desc.pDevice = pDevice;
	desc.pAllocationCallbacks = m_pHostHeap != nullptr ? &m_Callbacks : nullptr;
	m_nLastError = D3D12MA::CreateAllocator( &desc, &m_pAllocator );
	if ( FAILED( m_nLastError ) || m_pAllocator == nullptr ) {
		m_pHostHeap = nullptr;
		return false;
	}
	return true;
}

bool SIRD3D12Memory::queryAllocationLedger( sirD3D12AllocationLedger_t *pLedger ) const noexcept
{
	if ( pLedger == nullptr || m_pAllocator == nullptr ) {
		return false;
	}
	D3D12MA::Budget local = {};
	D3D12MA::Budget nonLocal = {};
	m_pAllocator->GetBudget( &local, &nonLocal );
	if ( local.Stats.AllocationBytes > UINT64_MAX - nonLocal.Stats.AllocationBytes ||
	     local.Stats.BlockBytes > UINT64_MAX - nonLocal.Stats.BlockBytes ) {
		return false;
	}
	const uint64_t nPhysicalBlocks = local.Stats.BlockBytes + nonLocal.Stats.BlockBytes;
	if ( nPhysicalBlocks > UINT64_MAX - m_nExternalBytes ) {
		return false;
	}
	*pLedger = {};
	pLedger->nCommittedBytes = nPhysicalBlocks + m_nExternalBytes;
	pLedger->nBlockBytes = nPhysicalBlocks;
	pLedger->nExternalBytes = m_nExternalBytes;
	pLedger->nPeakCommittedBytes = m_nPeakTrackedBytes > pLedger->nCommittedBytes ? m_nPeakTrackedBytes : pLedger->nCommittedBytes;
	pLedger->nResidentLimitBytes = m_nResidentLimitBytes;
	return true;
}

bool SIRD3D12Memory::configureResidentBudget( uint64_t nBytes ) noexcept
{
	sirD3D12AllocationLedger_t ledger = {};
	if ( !queryAllocationLedger( &ledger ) || ( nBytes != 0 && ledger.nCommittedBytes > nBytes ) ) {
		return false;
	}
	m_nResidentLimitBytes = nBytes;
	if ( ledger.nCommittedBytes > m_nPeakTrackedBytes ) {
		m_nPeakTrackedBytes = ledger.nCommittedBytes;
	}
	return true;
}

bool SIRD3D12Memory::configureSealedPools( const sirD3D12SealedPoolConfig_t &config ) noexcept
{
	if ( m_pAllocator == nullptr || m_bSealed ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	const uint64_t szPoolBytes[s_nSealedPoolCount] = { config.nDeviceBufferBytes,      config.nSampledTextureBytes,
	                                                   config.nAttachmentTextureBytes, config.nUploadBufferBytes,
	                                                   config.nReadbackBufferBytes,    config.nGpuUploadBufferBytes };
	const D3D12_HEAP_TYPE szHeapTypes[s_nSealedPoolCount] = { D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_TYPE_DEFAULT,
	                                                          D3D12_HEAP_TYPE_DEFAULT,  D3D12_HEAP_TYPE_UPLOAD,
	                                                          D3D12_HEAP_TYPE_READBACK, D3D12_HEAP_TYPE_GPU_UPLOAD };
	const D3D12_HEAP_FLAGS szHeapFlags[s_nSealedPoolCount] = {
	    D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,        D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
	    D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
	    D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,        D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS };
	uint64_t nReserveBytes = 0;
	constexpr uint64_t POOL_ALIGNMENT = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	for ( uint32_t iPool = 0; iPool < s_nSealedPoolCount; ++iPool ) {
		const uint64_t nBytes = szPoolBytes[iPool];
		if ( nBytes != 0 && ( nBytes < POOL_ALIGNMENT || nBytes % POOL_ALIGNMENT != 0 || nReserveBytes > UINT64_MAX - nBytes ) ) {
			m_nLastError = E_INVALIDARG;
			return false;
		}
		nReserveBytes += nBytes;
	}
	sirD3D12AllocationLedger_t before = {};
	if ( nReserveBytes == 0 || !queryAllocationLedger( &before ) ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	if ( m_nResidentLimitBytes != 0 &&
	     ( before.nCommittedBytes > m_nResidentLimitBytes || nReserveBytes > m_nResidentLimitBytes - before.nCommittedBytes ) ) {
		m_nLastError = E_OUTOFMEMORY;
		return false;
	}
	D3D12MA::Pool *szPools[s_nSealedPoolCount] = {};
	m_nLastError = S_OK;
	for ( uint32_t iPool = 0; iPool < s_nSealedPoolCount; ++iPool ) {
		if ( szPoolBytes[iPool] == 0 ) {
			continue;
		}
		if ( iPool == SIR_D3D12_SEALED_GPU_UPLOAD_BUFFER && m_pAllocator->IsGPUUploadHeapSupported() == FALSE ) {
			m_nLastError = E_NOTIMPL;
			break;
		}
		D3D12MA::POOL_DESC desc = {};
		desc.HeapProperties.Type = szHeapTypes[iPool];
		desc.HeapFlags = szHeapFlags[iPool];
		desc.BlockSize = szPoolBytes[iPool];
		desc.MinBlockCount = 1;
		desc.MaxBlockCount = 1;
		m_nLastError = m_pAllocator->CreatePool( &desc, &szPools[iPool] );
		if ( FAILED( m_nLastError ) ) {
			break;
		}
	}
	sirD3D12AllocationLedger_t after = {};
	if ( SUCCEEDED( m_nLastError ) &&
	     ( !queryAllocationLedger( &after ) || ( m_nResidentLimitBytes != 0 && after.nCommittedBytes > m_nResidentLimitBytes ) ) ) {
		m_nLastError = E_OUTOFMEMORY;
	}
	if ( FAILED( m_nLastError ) ) {
		for ( uint32_t iPool = 0; iPool < s_nSealedPoolCount; ++iPool ) {
			if ( szPools[iPool] != nullptr ) {
				szPools[iPool]->Release();
			}
		}
		return false;
	}
	for ( uint32_t iPool = 0; iPool < s_nSealedPoolCount; ++iPool ) {
		m_szSealedPools[iPool] = szPools[iPool];
	}
	m_bSealed = true;
	if ( after.nCommittedBytes > m_nPeakTrackedBytes ) {
		m_nPeakTrackedBytes = after.nCommittedBytes;
	}
	return true;
}

bool SIRD3D12Memory::reserveExternalBytes( uint64_t nBytes ) noexcept
{
	// External backing is not covered by the fixed D3D12MA pools.
	if ( m_bSealed ) {
		return false;
	}
	sirD3D12AllocationLedger_t ledger = {};
	if ( nBytes == 0 || !queryAllocationLedger( &ledger ) || ledger.nCommittedBytes > UINT64_MAX - nBytes ||
	     ( m_nResidentLimitBytes != 0 &&
	       ( ledger.nCommittedBytes > m_nResidentLimitBytes || nBytes > m_nResidentLimitBytes - ledger.nCommittedBytes ) ) ) {
		return false;
	}
	m_nExternalBytes += nBytes;
	const uint64_t nTotal = ledger.nCommittedBytes + nBytes;
	if ( nTotal > m_nPeakTrackedBytes ) {
		m_nPeakTrackedBytes = nTotal;
	}
	return true;
}

void SIRD3D12Memory::releaseExternalBytes( uint64_t nBytes ) noexcept
{
	if ( nBytes <= m_nExternalBytes ) {
		m_nExternalBytes -= nBytes;
	}
}

bool SIRD3D12Memory::enforceResidentLimit( D3D12MA::Allocation **ppAllocation ) noexcept
{
	if ( ppAllocation == nullptr || *ppAllocation == nullptr ) {
		return false;
	}
	sirD3D12AllocationLedger_t ledger = {};
	if ( !queryAllocationLedger( &ledger ) || ( m_nResidentLimitBytes != 0 && ledger.nCommittedBytes > m_nResidentLimitBytes ) ) {
		( *ppAllocation )->Release();
		*ppAllocation = nullptr;
		m_nLastError = E_OUTOFMEMORY;
		return false;
	}
	if ( ledger.nCommittedBytes > m_nPeakTrackedBytes ) {
		m_nPeakTrackedBytes = ledger.nCommittedBytes;
	}
	return true;
}

void SIRD3D12Memory::shutdown( void ) noexcept
{
	releaseTransferArena( true );
	for ( uint32_t iPool = 0; iPool < s_nSealedPoolCount; ++iPool ) {
		if ( m_szSealedPools[iPool] != nullptr ) {
			m_szSealedPools[iPool]->Release();
			m_szSealedPools[iPool] = nullptr;
		}
	}
	m_bSealed = false;
	if ( m_pAllocator != nullptr ) {
		m_pAllocator->Release();
		m_pAllocator = nullptr;
	}
	m_pHostHeap = nullptr;
	m_Callbacks = {};
	m_nResidentLimitBytes = 0;
	m_nExternalBytes = 0;
	m_nPeakTrackedBytes = 0;
}

void SIRD3D12Memory::abandon( void ) noexcept
{
	// The GPU may still use this arena. Preserve the native resources,
	// including their persistent mappings, and the allocator that owns them.
	releaseTransferArena( false );
	for ( uint32_t iPool = 0; iPool < s_nSealedPoolCount; ++iPool ) {
		m_szSealedPools[iPool] = nullptr;
	}
	m_bSealed = false;
	m_pAllocator = nullptr;
	m_pHostHeap = nullptr;
	m_Callbacks = {};
}

bool SIRD3D12Memory::isReady( void ) const noexcept
{
	return m_pAllocator != nullptr;
}

HRESULT SIRD3D12Memory::createBuffer( D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState, uint64_t nBytes,
                                      D3D12MA::Allocation **ppAllocation, D3D12_RESOURCE_FLAGS eResourceFlags ) noexcept
{
	if ( ppAllocation == nullptr ) {
		m_nLastError = E_POINTER;
		return m_nLastError;
	}
	*ppAllocation = nullptr;
	if ( m_pAllocator == nullptr || nBytes == 0 || nBytes > SIZE_MAX ) {
		m_nLastError = E_INVALIDARG;
		return m_nLastError;
	}

	D3D12_RESOURCE_DESC resource = {};
	resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource.Width = nBytes;
	resource.Height = 1;
	resource.DepthOrArraySize = 1;
	resource.MipLevels = 1;
	resource.Format = DXGI_FORMAT_UNKNOWN;
	resource.SampleDesc.Count = 1;
	resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource.Flags = eResourceFlags;
	D3D12MA::ALLOCATION_DESC allocation = {};
	allocation.HeapType = eHeapType;
	allocation.Flags = D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET;
	if ( m_bSealed ) {
		uint32_t iPool = s_nSealedPoolCount;
		if ( eHeapType == D3D12_HEAP_TYPE_DEFAULT ) {
			iPool = SIR_D3D12_SEALED_DEVICE_BUFFER;
		} else if ( eHeapType == D3D12_HEAP_TYPE_UPLOAD ) {
			iPool = SIR_D3D12_SEALED_UPLOAD_BUFFER;
		} else if ( eHeapType == D3D12_HEAP_TYPE_READBACK ) {
			iPool = SIR_D3D12_SEALED_READBACK_BUFFER;
		} else if ( eHeapType == D3D12_HEAP_TYPE_GPU_UPLOAD ) {
			iPool = SIR_D3D12_SEALED_GPU_UPLOAD_BUFFER;
		}
		if ( iPool == s_nSealedPoolCount || m_szSealedPools[iPool] == nullptr ) {
			m_nLastError = E_OUTOFMEMORY;
			return m_nLastError;
		}
		allocation.CustomPool = m_szSealedPools[iPool];
	}

	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, nullptr, ppAllocation, IID_NULL, nullptr );
	if ( SUCCEEDED( m_nLastError ) && !enforceResidentLimit( ppAllocation ) ) {
		return E_OUTOFMEMORY;
	}
	return m_nLastError;
}

HRESULT SIRD3D12Memory::createTexture2D( uint32_t nWidth, uint32_t nHeight, DXGI_FORMAT eFormat, D3D12_RESOURCE_FLAGS eFlags,
                                         D3D12_RESOURCE_STATES eInitialState, const D3D12_CLEAR_VALUE *pClearValue,
                                         D3D12MA::Allocation **ppAllocation ) noexcept
{
	if ( ppAllocation == nullptr ) {
		m_nLastError = E_POINTER;
		return m_nLastError;
	}
	*ppAllocation = nullptr;
	if ( m_pAllocator == nullptr || nWidth == 0 || nHeight == 0 || eFormat == DXGI_FORMAT_UNKNOWN ) {
		m_nLastError = E_INVALIDARG;
		return m_nLastError;
	}

	D3D12_RESOURCE_DESC resource = {};
	resource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource.Width = nWidth;
	resource.Height = nHeight;
	resource.DepthOrArraySize = 1;
	resource.MipLevels = 1;
	resource.Format = eFormat;
	resource.SampleDesc.Count = 1;
	resource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource.Flags = eFlags;

	D3D12MA::ALLOCATION_DESC allocation = {};
	allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	allocation.Flags = D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET;
	if ( m_bSealed ) {
		const uint32_t iPool =
		    ( resource.Flags & ( D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ) ) != 0
		        ? SIR_D3D12_SEALED_ATTACHMENT_TEXTURE
		        : SIR_D3D12_SEALED_SAMPLED_TEXTURE;
		if ( m_szSealedPools[iPool] == nullptr ) {
			m_nLastError = E_OUTOFMEMORY;
			return m_nLastError;
		}
		allocation.CustomPool = m_szSealedPools[iPool];
	}
	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, pClearValue, ppAllocation, IID_NULL, nullptr );
	if ( SUCCEEDED( m_nLastError ) && !enforceResidentLimit( ppAllocation ) ) {
		return E_OUTOFMEMORY;
	}
	return m_nLastError;
}

HRESULT SIRD3D12Memory::createTexture( const D3D12_RESOURCE_DESC &resource, D3D12_RESOURCE_STATES eInitialState,
                                       const D3D12_CLEAR_VALUE *pClearValue, D3D12MA::Allocation **ppAllocation ) noexcept
{
	if ( ppAllocation == nullptr ) {
		m_nLastError = E_POINTER;
		return m_nLastError;
	}
	*ppAllocation = nullptr;
	if ( m_pAllocator == nullptr || resource.Width == 0 || resource.Height == 0 || resource.Format == DXGI_FORMAT_UNKNOWN ) {
		m_nLastError = E_INVALIDARG;
		return m_nLastError;
	}
	D3D12MA::ALLOCATION_DESC allocation = {};
	allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	allocation.Flags = D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET;
	if ( m_bSealed ) {
		const uint32_t iPool =
		    ( resource.Flags & ( D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ) ) != 0
		        ? SIR_D3D12_SEALED_ATTACHMENT_TEXTURE
		        : SIR_D3D12_SEALED_SAMPLED_TEXTURE;
		if ( m_szSealedPools[iPool] == nullptr ) {
			m_nLastError = E_OUTOFMEMORY;
			return m_nLastError;
		}
		allocation.CustomPool = m_szSealedPools[iPool];
	}
	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, pClearValue, ppAllocation, IID_NULL, nullptr );
	if ( SUCCEEDED( m_nLastError ) && !enforceResidentLimit( ppAllocation ) ) {
		return E_OUTOFMEMORY;
	}
	return m_nLastError;
}

bool SIRD3D12Memory::initializeTransferArena( const sirD3D12TransferPoolConfig_t &config, uint32_t nSlots ) noexcept
{
	constexpr uint64_t MAX_SLOT_BYTES = 16ULL * 1024ULL * 1024ULL;
	if ( !isReady() || m_pTransferWrite != nullptr || m_pTransferReadback != nullptr || config.nSlotBytes == 0 ||
	     config.nSlotBytes > MAX_SLOT_BYTES || nSlots == 0 || nSlots > 16 || config.nSlotBytes > SIZE_MAX / nSlots ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	const uint64_t nBytes = config.nSlotBytes * nSlots;
	const bool bUMA = m_pAllocator->IsUMA() != FALSE;
	const bool bGPUUpload = m_pAllocator->IsGPUUploadHeapSupported() != FALSE;
	D3D12_HEAP_TYPE eWriteHeap = D3D12_HEAP_TYPE_UPLOAD;
	if ( config.bPreferDirectMapped && bGPUUpload ) {
		eWriteHeap = D3D12_HEAP_TYPE_GPU_UPLOAD;
		m_eTransferPath = SIR_D3D12_TRANSFER_DIRECT_GPU_UPLOAD;
	} else if ( config.bPreferDirectMapped && bUMA ) {
		m_eTransferPath = SIR_D3D12_TRANSFER_DIRECT_UMA;
	} else {
		m_eTransferPath = SIR_D3D12_TRANSFER_STAGED;
	}

	m_nLastError = createBuffer( eWriteHeap, D3D12_RESOURCE_STATE_GENERIC_READ, nBytes, &m_pTransferWrite );
	if ( FAILED( m_nLastError ) ) {
		releaseTransferArena( true );
		return false;
	}
	if ( m_eTransferPath == SIR_D3D12_TRANSFER_STAGED ) {
		m_nLastError = createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, nBytes, &m_pTransferGpu );
		if ( FAILED( m_nLastError ) ) {
			releaseTransferArena( true );
			return false;
		}
	}
	m_nLastError = createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nBytes, &m_pTransferReadback );
	if ( FAILED( m_nLastError ) ) {
		releaseTransferArena( true );
		return false;
	}

	D3D12_RANGE noRead = { 0, 0 };
	void *pWrite = nullptr;
	m_nLastError = m_pTransferWrite->GetResource()->Map( 0, &noRead, &pWrite );
	if ( FAILED( m_nLastError ) || pWrite == nullptr ) {
		releaseTransferArena( true );
		return false;
	}
	m_pTransferWriteBytes = ( uint8_t * )pWrite;

	void *pReadback = nullptr;
	m_nLastError = m_pTransferReadback->GetResource()->Map( 0, nullptr, &pReadback );
	if ( FAILED( m_nLastError ) || pReadback == nullptr ) {
		releaseTransferArena( true );
		return false;
	}
	m_pTransferReadbackBytes = ( const uint8_t * )pReadback;
	m_nTransferSlotBytes = config.nSlotBytes;
	m_nTransferReservedBytes = nBytes * ( m_pTransferGpu != nullptr ? 3 : 2 );
	m_nLastError = S_OK;
	return true;
}

void SIRD3D12Memory::releaseTransferArena( bool bGpuComplete ) noexcept
{
	if ( bGpuComplete ) {
		D3D12_RANGE noWrite = { 0, 0 };
		if ( m_pTransferReadbackBytes != nullptr && m_pTransferReadback != nullptr ) {
			m_pTransferReadback->GetResource()->Unmap( 0, &noWrite );
		}
		if ( m_pTransferWriteBytes != nullptr && m_pTransferWrite != nullptr ) {
			m_pTransferWrite->GetResource()->Unmap( 0, nullptr );
		}
		if ( m_pTransferReadback != nullptr ) {
			m_pTransferReadback->Release();
		}
		if ( m_pTransferGpu != nullptr ) {
			m_pTransferGpu->Release();
		}
		if ( m_pTransferWrite != nullptr ) {
			m_pTransferWrite->Release();
		}
	}
	m_pTransferWrite = nullptr;
	m_pTransferGpu = nullptr;
	m_pTransferReadback = nullptr;
	m_pTransferWriteBytes = nullptr;
	m_pTransferReadbackBytes = nullptr;
	m_nTransferSlotBytes = 0;
	m_nTransferReservedBytes = 0;
	m_eTransferPath = SIR_D3D12_TRANSFER_STAGED;
}

bool SIRD3D12Memory::queryTransferArena( sirD3D12ResidentTransferView_t *pView ) const noexcept
{
	if ( pView == nullptr || !isReady() || m_pTransferWrite == nullptr || m_pTransferReadback == nullptr ||
	     m_pTransferWriteBytes == nullptr || m_pTransferReadbackBytes == nullptr || m_nTransferSlotBytes == 0 ) {
		return false;
	}
	*pView = {};
	pView->pWriteResource = m_pTransferWrite->GetResource();
	pView->pGpuResource = m_pTransferGpu == nullptr ? pView->pWriteResource : m_pTransferGpu->GetResource();
	pView->pReadbackResource = m_pTransferReadback->GetResource();
	pView->pWriteBytes = m_pTransferWriteBytes;
	pView->pReadbackBytes = m_pTransferReadbackBytes;
	pView->nSlotBytes = m_nTransferSlotBytes;
	pView->nReservedBufferBytes = m_nTransferReservedBytes;
	pView->nPersistentMappedBytes = ( m_nTransferReservedBytes / ( m_pTransferGpu != nullptr ? 3 : 2 ) ) * 2;
	pView->ePath = m_eTransferPath;
	return true;
}

HRESULT SIRD3D12Memory::getLastError( void ) const noexcept
{
	return m_nLastError;
}
