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

SIRD3D12Memory::SIRD3D12Memory( void ) noexcept
{
}

SIRD3D12Memory::~SIRD3D12Memory( void ) noexcept
{
	shutdown();
}

bool SIRD3D12Memory::initialize( IDXGIAdapter *pAdapter, ID3D12Device *pDevice ) noexcept
{
	if ( m_pAllocator != nullptr || pAdapter == nullptr || pDevice == nullptr ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.pAdapter = pAdapter;
	desc.pDevice = pDevice;
	m_nLastError = D3D12MA::CreateAllocator( &desc, &m_pAllocator );
	return SUCCEEDED( m_nLastError ) && m_pAllocator != nullptr;
}

void SIRD3D12Memory::shutdown( void ) noexcept
{
	releaseTransferArena( true );
	if ( m_pAllocator != nullptr ) {
		m_pAllocator->Release();
		m_pAllocator = nullptr;
	}
}

void SIRD3D12Memory::abandon( void ) noexcept
{
	// The GPU may still use this arena. Preserve the native resources,
	// including their persistent mappings, and the allocator that owns them.
	releaseTransferArena( false );
	m_pAllocator = nullptr;
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

	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, nullptr, ppAllocation, IID_NULL, nullptr );
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
	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, pClearValue, ppAllocation, IID_NULL, nullptr );
	return m_nLastError;
}

bool SIRD3D12Memory::initializeTransferArena( const sirD3D12TransferPoolConfig_t &config, uint32_t nSlots ) noexcept
{
	constexpr uint64_t MAX_SLOT_BYTES = 16ULL * 1024ULL * 1024ULL;
	if ( !isReady() || m_pTransferWrite != nullptr || m_pTransferReadback != nullptr ||
	     config.nSlotBytes == 0 || config.nSlotBytes > MAX_SLOT_BYTES || nSlots == 0 || nSlots > 16 ||
	     config.nSlotBytes > SIZE_MAX / nSlots ) {
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
		m_nLastError = createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, nBytes,
		                             &m_pTransferGpu );
		if ( FAILED( m_nLastError ) ) {
			releaseTransferArena( true );
			return false;
		}
	}
	m_nLastError = createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nBytes,
	                             &m_pTransferReadback );
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
