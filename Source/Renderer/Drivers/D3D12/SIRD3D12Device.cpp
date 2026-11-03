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

#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>

#include <string.h>

static HRESULT createBootstrapBuffer( ID3D12Device *pDevice, D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState,
                                      uint64_t nBytes, ID3D12Resource **ppResource ) noexcept
{
	if ( pDevice == nullptr || ppResource == nullptr || nBytes == 0 ) {
		return E_INVALIDARG;
	}

	*ppResource = nullptr;
	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = eHeapType;
	heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = nBytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	return pDevice->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &desc, eInitialState, nullptr, IID_PPV_ARGS( ppResource ) );
}

SIRD3D12Device::SIRD3D12Device( void ) noexcept
{
}

SIRD3D12Device::~SIRD3D12Device( void ) noexcept
{
	shutdown();
}

bool SIRD3D12Device::initialize( bool bUseWarp ) noexcept
{
	if ( isReady() || m_pFactory != nullptr ) {
		m_nLastError = E_FAIL;
		return false;
	}

	m_nLastError = CreateDXGIFactory2( 0, IID_PPV_ARGS( m_pFactory.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	if ( bUseWarp ) {
		m_nLastError = m_pFactory->EnumWarpAdapter( IID_PPV_ARGS( m_pAdapter.GetAddressOf() ) );
		if ( SUCCEEDED( m_nLastError ) ) {
			m_nLastError = D3D12CreateDevice( m_pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( m_pDevice.GetAddressOf() ) );
		}
	} else {
		for ( UINT iAdapter = 0;; ++iAdapter ) {
			Microsoft::WRL::ComPtr<IDXGIAdapter4> pCandidate;
			const HRESULT nEnumResult = m_pFactory->EnumAdapterByGpuPreference(
			    iAdapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( pCandidate.GetAddressOf() ) );
			if ( nEnumResult == DXGI_ERROR_NOT_FOUND ) {
				break;
			}
			if ( FAILED( nEnumResult ) ) {
				m_nLastError = nEnumResult;
				break;
			}

			DXGI_ADAPTER_DESC3 description = {};
			if ( FAILED( pCandidate->GetDesc3( &description ) ) || ( description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE ) != 0 ) {
				continue;
			}

			Microsoft::WRL::ComPtr<ID3D12Device> pCandidateDevice;
			const HRESULT nDeviceResult = D3D12CreateDevice( pCandidate.Get(), D3D_FEATURE_LEVEL_12_0,
			                                                 IID_PPV_ARGS( pCandidateDevice.GetAddressOf() ) );
			if ( SUCCEEDED( nDeviceResult ) ) {
				m_pAdapter = pCandidate;
				m_pDevice = pCandidateDevice;
				break;
			}
			m_nLastError = nDeviceResult;
		}
	}

	if ( m_pDevice == nullptr ) {
		if ( SUCCEEDED( m_nLastError ) ) {
			m_nLastError = DXGI_ERROR_NOT_FOUND;
		}
		shutdown();
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queue = {};
	queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue.NodeMask = 0;
	m_nLastError = m_pDevice->CreateCommandQueue( &queue, IID_PPV_ARGS( m_pGraphicsQueue.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	m_nLastError = m_pDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( m_pGraphicsFence.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	m_hFenceEvent = CreateEventW( nullptr, FALSE, FALSE, nullptr );
	if ( m_hFenceEvent == nullptr ) {
		m_nLastError = HRESULT_FROM_WIN32( GetLastError() );
		shutdown();
		return false;
	}

	m_nNextFenceValue = 1;
	m_nLastError = S_OK;
	return true;
}

void SIRD3D12Device::shutdown( void ) noexcept
{
	if ( m_pGraphicsQueue != nullptr && m_pGraphicsFence != nullptr && m_hFenceEvent != nullptr ) {
		const uint64_t nFence = m_nNextFenceValue;
		if ( SUCCEEDED( m_pGraphicsQueue->Signal( m_pGraphicsFence.Get(), nFence ) ) ) {
			if ( m_pGraphicsFence->GetCompletedValue() < nFence &&
			     SUCCEEDED( m_pGraphicsFence->SetEventOnCompletion( nFence, m_hFenceEvent ) ) ) {
				WaitForSingleObject( m_hFenceEvent, 15000 );
			}
		}
	}
	if ( m_hFenceEvent != nullptr ) {
		CloseHandle( m_hFenceEvent );
		m_hFenceEvent = nullptr;
	}
	m_pGraphicsFence.Reset();
	m_pGraphicsQueue.Reset();
	m_pDevice.Reset();
	m_pAdapter.Reset();
	m_pFactory.Reset();
	m_nNextFenceValue = 0;
}

bool SIRD3D12Device::isReady( void ) const noexcept
{
	return m_pDevice != nullptr && m_pGraphicsQueue != nullptr && m_pGraphicsFence != nullptr && m_hFenceEvent != nullptr;
}

bool SIRD3D12Device::queryAdapterInfo( sirD3D12AdapterInfo_t *pInfo ) const noexcept
{
	if ( !isReady() || pInfo == nullptr ) {
		return false;
	}

	DXGI_ADAPTER_DESC3 desc = {};
	if ( FAILED( m_pAdapter->GetDesc3( &desc ) ) ) {
		return false;
	}

	*pInfo = {};
	pInfo->nVendorId = desc.VendorId;
	pInfo->nDeviceId = desc.DeviceId;
	pInfo->nDedicatedVideoBytes = desc.DedicatedVideoMemory;
	pInfo->nSharedSystemBytes = desc.SharedSystemMemory;
	pInfo->nAdapterLuid = ( ( int64_t )desc.AdapterLuid.HighPart << 32 ) | desc.AdapterLuid.LowPart;
	pInfo->bSoftware = ( desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE ) != 0;
	memcpy( pInfo->szName, desc.Description, sizeof( pInfo->szName ) );
	return true;
}

bool SIRD3D12Device::queryLocalMemory( sirD3D12MemoryInfo_t *pInfo ) const noexcept
{
	if ( !isReady() || pInfo == nullptr ) {
		return false;
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
	if ( FAILED( m_pAdapter->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info ) ) ) {
		return false;
	}

	pInfo->nBudgetBytes = info.Budget;
	pInfo->nCurrentUsageBytes = info.CurrentUsage;
	pInfo->nAvailableForReservationBytes = info.AvailableForReservation;
	pInfo->nCurrentReservationBytes = info.CurrentReservation;
	return true;
}

HRESULT SIRD3D12Device::getLastError( void ) const noexcept
{
	return m_nLastError;
}

bool SIRD3D12Device::copyRoundTripForTest( const void *pInput, void *pOutput, uint64_t nBytes ) noexcept
{
	constexpr uint64_t MAX_TEST_COPY_BYTES = 16ULL * 1024ULL * 1024ULL;
	if ( !isReady() || pInput == nullptr || pOutput == nullptr || nBytes == 0 || nBytes > MAX_TEST_COPY_BYTES ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> pUpload;
	Microsoft::WRL::ComPtr<ID3D12Resource> pDeviceBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> pReadback;
	m_nLastError = createBootstrapBuffer( m_pDevice.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nBytes,
	                                      pUpload.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = createBootstrapBuffer( m_pDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, nBytes,
	                                      pDeviceBuffer.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = createBootstrapBuffer( m_pDevice.Get(), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nBytes,
	                                      pReadback.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	D3D12_RANGE emptyRead = { 0, 0 };
	void *pUploadBytes = nullptr;
	m_nLastError = pUpload->Map( 0, &emptyRead, &pUploadBytes );
	if ( FAILED( m_nLastError ) || pUploadBytes == nullptr ) {
		return false;
	}
	memcpy( pUploadBytes, pInput, ( size_t )nBytes );
	pUpload->Unmap( 0, nullptr );

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommands;
	m_nLastError = m_pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( pAllocator.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator.Get(), nullptr,
	                                            IID_PPV_ARGS( pCommands.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	pCommands->CopyBufferRegion( pDeviceBuffer.Get(), 0, pUpload.Get(), 0, nBytes );
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pDeviceBuffer.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	pCommands->ResourceBarrier( 1, &barrier );
	pCommands->CopyBufferRegion( pReadback.Get(), 0, pDeviceBuffer.Get(), 0, nBytes );

	m_nLastError = pCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	ID3D12CommandList *pLists[] = { pCommands.Get() };
	m_pGraphicsQueue->ExecuteCommandLists( 1, pLists );

	const uint64_t nFenceValue = m_nNextFenceValue++;
	m_nLastError = m_pGraphicsQueue->Signal( m_pGraphicsFence.Get(), nFenceValue );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	if ( m_pGraphicsFence->GetCompletedValue() < nFenceValue ) {
		m_nLastError = m_pGraphicsFence->SetEventOnCompletion( nFenceValue, m_hFenceEvent );
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
		const DWORD nWait = WaitForSingleObject( m_hFenceEvent, 15000 );
		if ( nWait != WAIT_OBJECT_0 ) {
			m_nLastError = nWait == WAIT_TIMEOUT ? HRESULT_FROM_WIN32( ERROR_TIMEOUT ) : HRESULT_FROM_WIN32( GetLastError() );
			return false;
		}
	}

	D3D12_RANGE readRange = { 0, ( SIZE_T )nBytes };
	void *pReadBytes = nullptr;
	m_nLastError = pReadback->Map( 0, &readRange, &pReadBytes );
	if ( FAILED( m_nLastError ) || pReadBytes == nullptr ) {
		return false;
	}
	memcpy( pOutput, pReadBytes, ( size_t )nBytes );
	pReadback->Unmap( 0, &emptyRead );
	m_nLastError = S_OK;
	return true;
}
