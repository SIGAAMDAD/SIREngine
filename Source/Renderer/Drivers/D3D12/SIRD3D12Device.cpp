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
#include <atomic>

static std::atomic<uint64_t> s_nNextD3D12DeviceGeneration{ 1 };

SIRD3D12ComputeQueue::SIRD3D12ComputeQueue( void ) noexcept
{
}

SIRD3D12ComputeQueue::~SIRD3D12ComputeQueue( void ) noexcept
{
	shutdown();
}

bool SIRD3D12ComputeQueue::initialize( ID3D12Device *pDevice ) noexcept
{
	if ( pDevice == nullptr || m_pQueue != nullptr || m_bUnsafe ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	D3D12_COMMAND_QUEUE_DESC queue = {};
	queue.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	queue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	m_nLastError = pDevice->CreateCommandQueue( &queue, IID_PPV_ARGS( m_pQueue.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_nLastError = pDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( m_pFence.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_hEvent = CreateEventW( nullptr, FALSE, FALSE, nullptr );
	if ( m_hEvent == nullptr ) {
		m_nLastError = HRESULT_FROM_WIN32( GetLastError() );
		shutdown();
		return false;
	}
	m_nLastError = pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS( m_pAllocator.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_nLastError = pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_pAllocator.Get(), nullptr,
	                                           IID_PPV_ARGS( m_pCommands.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_nLastError = m_pCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12ComputeQueue::shutdown( void ) noexcept
{
	bool bSafe = !m_bUnsafe;
	if ( bSafe && m_nSubmittedFence != 0 && m_pFence != nullptr ) {
		bSafe = waitForFence( m_nSubmittedFence, 15000 );
	}
	if ( bSafe ) {
		m_pPendingDependencyFence.Reset();
		m_pCommands.Reset();
		m_pAllocator.Reset();
		m_pFence.Reset();
		m_pQueue.Reset();
		if ( m_hEvent != nullptr ) {
			CloseHandle( m_hEvent );
		}
	} else {
		// Native references are retained when GPU completion cannot be proved.
		( void )m_pPendingDependencyFence.Detach();
		( void )m_pCommands.Detach();
		( void )m_pAllocator.Detach();
		( void )m_pFence.Detach();
		( void )m_pQueue.Detach();
	}
	m_hEvent = nullptr;
	m_nNextFence = 1;
	m_nSubmittedFence = 0;
	m_bUnsafe = !bSafe;
	return bSafe;
}

bool SIRD3D12ComputeQueue::isReady( void ) const noexcept
{
	return m_pQueue != nullptr && m_pFence != nullptr && m_pAllocator != nullptr && m_pCommands != nullptr && m_hEvent != nullptr &&
	       !m_bUnsafe;
}

bool SIRD3D12ComputeQueue::getCompletedFence( uint64_t *pCompletedFence ) const noexcept
{
	if ( pCompletedFence == nullptr || !isReady() ) {
		return false;
	}
	*pCompletedFence = m_pFence->GetCompletedValue();
	return *pCompletedFence != UINT64_MAX;
}

uint64_t SIRD3D12ComputeQueue::getLastSubmittedFence( void ) const noexcept
{
	return m_nSubmittedFence;
}

HRESULT SIRD3D12ComputeQueue::getLastError( void ) const noexcept
{
	return m_nLastError;
}

bool SIRD3D12ComputeQueue::pollFence( uint64_t nFence, bool *pCompleted ) const noexcept
{
	if ( pCompleted == nullptr ) {
		return false;
	}
	*pCompleted = false;
	if ( nFence > m_nSubmittedFence ) {
		return false;
	}
	uint64_t nCompleted = 0;
	if ( !getCompletedFence( &nCompleted ) ) {
		return false;
	}
	*pCompleted = nCompleted >= nFence;
	return true;
}

bool SIRD3D12ComputeQueue::waitForFence( uint64_t nFence, uint32_t nTimeoutMs ) noexcept
{
	bool bCompleted = false;
	if ( !pollFence( nFence, &bCompleted ) ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	if ( bCompleted ) {
		m_nLastError = S_OK;
		return true;
	}
	m_nLastError = m_pFence->SetEventOnCompletion( nFence, m_hEvent );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	const DWORD nWait = WaitForSingleObject( m_hEvent, nTimeoutMs );
	if ( nWait != WAIT_OBJECT_0 ) {
		m_nLastError = nWait == WAIT_TIMEOUT ? HRESULT_FROM_WIN32( ERROR_TIMEOUT ) : HRESULT_FROM_WIN32( GetLastError() );
		return false;
	}
	if ( !pollFence( nFence, &bCompleted ) || !bCompleted ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12ComputeQueue::submitMarker( uint64_t *pFenceValue ) noexcept
{
	return submitMarkerAfterFence( nullptr, 0, pFenceValue );
}

bool SIRD3D12ComputeQueue::submitMarkerAfterFence( ID3D12Fence *pDependencyFence, uint64_t nDependencyFence,
                                                   uint64_t *pFenceValue ) noexcept
{
	if ( pFenceValue == nullptr ) {
		m_nLastError = E_POINTER;
		return false;
	}
	*pFenceValue = 0;

	if ( ( pDependencyFence == nullptr ) != ( nDependencyFence == 0 ) ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	if ( !isReady() || m_nNextFence == UINT64_MAX ) {
		m_nLastError = E_FAIL;
		return false;
	}

	bool bCompleted = false;
	if ( !pollFence( m_nSubmittedFence, &bCompleted ) ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	if ( !bCompleted ) {
		m_nLastError = HRESULT_FROM_WIN32( ERROR_BUSY );
		return false;
	}

	// The previous queue marker is retired; its external fence can be freed.
	m_pPendingDependencyFence.Reset();
	m_nLastError = m_pAllocator->Reset();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_pCommands->Reset( m_pAllocator.Get(), nullptr );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_pCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	if ( pDependencyFence != nullptr ) {
		// Retain the external fence until this compute marker has retired,
		// including when its COPY queue owner shuts down independently.
		m_pPendingDependencyFence = pDependencyFence;
		m_nLastError = m_pQueue->Wait( m_pPendingDependencyFence.Get(), nDependencyFence );
		if ( FAILED( m_nLastError ) ) {
			m_pPendingDependencyFence.Reset();
			return false;
		}
	}

	ID3D12CommandList *pLists[] = { m_pCommands.Get() };
	m_pQueue->ExecuteCommandLists( 1, pLists );
	m_nLastError = m_pQueue->Signal( m_pFence.Get(), m_nNextFence );
	if ( FAILED( m_nLastError ) ) {
		m_bUnsafe = true;
		return false;
	}
	m_nSubmittedFence = m_nNextFence;
	*pFenceValue = m_nNextFence;
	++m_nNextFence;
	m_nLastError = S_OK;
	return true;
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
			const HRESULT nEnumResult = m_pFactory->EnumAdapterByGpuPreference( iAdapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			                                                                    IID_PPV_ARGS( pCandidate.GetAddressOf() ) );
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
			const HRESULT nDeviceResult =
			    D3D12CreateDevice( pCandidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( pCandidateDevice.GetAddressOf() ) );
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

	if ( !m_Memory.initialize( m_pAdapter.Get(), m_pDevice.Get() ) ) {
		m_nLastError = m_Memory.getLastError();
		shutdown();
		return false;
	}

	m_nLastError = m_pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( m_pCopyAllocator.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_nLastError = m_pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCopyAllocator.Get(), nullptr,
	                                             IID_PPV_ARGS( m_pCopyCommands.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}
	m_nLastError = m_pCopyCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	if ( !m_Transfers.initialize( m_pDevice.Get(), &m_Memory ) ) {
		m_nLastError = m_Transfers.getLastError();
		shutdown();
		return false;
	}
	if ( !m_ComputeQueue.initialize( m_pDevice.Get() ) ) {
		m_nLastError = m_ComputeQueue.getLastError();
		shutdown();
		return false;
	}
	m_nDeviceGeneration = s_nNextD3D12DeviceGeneration.fetch_add( 1, std::memory_order_relaxed );
	m_nNextFenceValue = 1;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Device::waitForPendingCopy( void ) noexcept
{
	if ( m_pPendingUpload == nullptr ) {
		return true;
	}
	if ( m_pGraphicsQueue == nullptr || m_pGraphicsFence == nullptr || m_hFenceEvent == nullptr ) {
		m_nLastError = E_FAIL;
		return false;
	}

	if ( m_nPendingFenceValue == 0 ) {
		const uint64_t nFence = m_nNextFenceValue;
		m_nLastError = m_pGraphicsQueue->Signal( m_pGraphicsFence.Get(), nFence );
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
		++m_nNextFenceValue;
		m_nPendingFenceValue = nFence;
	}
	const uint64_t nCompleted = m_pGraphicsFence->GetCompletedValue();
	if ( nCompleted == UINT64_MAX ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	if ( nCompleted < m_nPendingFenceValue ) {
		m_nLastError = m_pGraphicsFence->SetEventOnCompletion( m_nPendingFenceValue, m_hFenceEvent );
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
		const DWORD nWait = WaitForSingleObject( m_hFenceEvent, 15000 );
		if ( nWait != WAIT_OBJECT_0 ) {
			m_nLastError = nWait == WAIT_TIMEOUT ? HRESULT_FROM_WIN32( ERROR_TIMEOUT ) : HRESULT_FROM_WIN32( GetLastError() );
			return false;
		}
	}
	if ( m_pGraphicsFence->GetCompletedValue() < m_nPendingFenceValue ) {
		m_nLastError = E_FAIL;
		return false;
	}
	return true;
}

void SIRD3D12Device::releasePendingCopy( void ) noexcept
{
	if ( m_pPendingReadback != nullptr ) {
		m_pPendingReadback->Release();
		m_pPendingReadback = nullptr;
	}
	if ( m_pPendingDevice != nullptr ) {
		m_pPendingDevice->Release();
		m_pPendingDevice = nullptr;
	}
	if ( m_pPendingUpload != nullptr ) {
		m_pPendingUpload->Release();
		m_pPendingUpload = nullptr;
	}
	m_nPendingFenceValue = 0;
}

void SIRD3D12Device::shutdown( void ) noexcept
{
	const bool bCopySafe = waitForPendingCopy();
	// Compute markers may wait on COPY fences: retire COMPUTE before COPY.
	const bool bComputeSafe = m_ComputeQueue.shutdown();
	const bool bTransferSafe = m_Transfers.shutdown();
	if ( bCopySafe && bTransferSafe && bComputeSafe ) {
		releasePendingCopy();
		m_pCopyCommands.Reset();
		m_pCopyAllocator.Reset();
		m_Memory.shutdown();
		m_pGraphicsQueue.Reset();
		m_pGraphicsFence.Reset();
		m_pDevice.Reset();
	} else {
		// The command list may still reference every pending allocation. Leak
		// those native objects deliberately rather than create a GPU use-after-free.
		( void )m_pCopyCommands.Detach();
		( void )m_pCopyAllocator.Detach();
		( void )m_pGraphicsQueue.Detach();
		( void )m_pGraphicsFence.Detach();
		( void )m_pDevice.Detach();
		m_Memory.abandon();
		m_pPendingUpload = nullptr;
		m_pPendingDevice = nullptr;
		m_pPendingReadback = nullptr;
	}
	if ( bCopySafe && bTransferSafe && bComputeSafe && m_hFenceEvent != nullptr ) {
		CloseHandle( m_hFenceEvent );
	}
	m_hFenceEvent = nullptr;
	m_pAdapter.Reset();
	m_pFactory.Reset();
	m_nNextFenceValue = 0;
	m_nPendingFenceValue = 0;
	m_nDeviceGeneration = 0;
}

bool SIRD3D12Device::isReady( void ) const noexcept
{
	return m_pDevice != nullptr && m_pGraphicsQueue != nullptr && m_pGraphicsFence != nullptr && m_hFenceEvent != nullptr &&
	       m_pCopyAllocator != nullptr && m_pCopyCommands != nullptr && m_Memory.isReady() && m_Transfers.isReady() &&
	       m_ComputeQueue.isReady() && m_nDeviceGeneration != 0;
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
	pInfo->nAdapterLuid = ( int64_t )( ( ( uint64_t )( uint32_t )desc.AdapterLuid.HighPart << 32 ) | desc.AdapterLuid.LowPart );
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

uint64_t SIRD3D12Device::getCompletedCopyCount( void ) const noexcept
{
	return m_nCompletedCopyCount;
}

SIRD3D12Transfers &SIRD3D12Device::getTransfers( void ) noexcept
{
	return m_Transfers;
}

SIRD3D12ComputeQueue &SIRD3D12Device::getComputeQueue( void ) noexcept
{
	return m_ComputeQueue;
}

bool SIRD3D12Device::snapshotCompletionSet( sirD3D12CompletionSet_t *pSet ) const noexcept
{
	if ( pSet == nullptr || !isReady() ) {
		return false;
	}
	pSet->nDeviceGeneration = m_nDeviceGeneration;
	pSet->nGraphicsFence = m_nNextFenceValue - 1;
	pSet->nComputeFence = m_ComputeQueue.getLastSubmittedFence();
	pSet->nCopyFence = m_Transfers.getLastSubmittedFence();
	return true;
}

bool SIRD3D12Device::pollCompletionSet( const sirD3D12CompletionSet_t &set, bool *pCompleted ) noexcept
{
	if ( pCompleted == nullptr ) {
		m_nLastError = E_POINTER;
		return false;
	}
	*pCompleted = false;
	if ( !isReady() || set.nDeviceGeneration != m_nDeviceGeneration || set.nGraphicsFence >= m_nNextFenceValue ||
	     set.nComputeFence > m_ComputeQueue.getLastSubmittedFence() || set.nCopyFence > m_Transfers.getLastSubmittedFence() ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	const uint64_t nGraphicsDone = m_pGraphicsFence->GetCompletedValue();
	uint64_t nComputeDone = 0;
	uint64_t nCopyDone = 0;
	if ( nGraphicsDone == UINT64_MAX || !m_ComputeQueue.getCompletedFence( &nComputeDone ) ||
	     !m_Transfers.getCompletedFence( &nCopyDone ) ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	*pCompleted = nGraphicsDone >= set.nGraphicsFence && nComputeDone >= set.nComputeFence && nCopyDone >= set.nCopyFence;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Device::submitComputeMarker( sirD3D12CompletionSet_t *pSet ) noexcept
{
	if ( pSet == nullptr || !isReady() ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	uint64_t nSubmittedFence = 0;
	if ( !m_ComputeQueue.submitMarker( &nSubmittedFence ) ) {
		m_nLastError = m_ComputeQueue.getLastError();
		return false;
	}
	return snapshotCompletionSet( pSet ) && pSet->nComputeFence == nSubmittedFence;
}

bool SIRD3D12Device::submitComputeAfterCopy( sirD3D12TransferTicket_t ticket, sirD3D12CompletionSet_t *pSet ) noexcept
{
	if ( pSet == nullptr || !isReady() ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	*pSet = {};

	ID3D12Fence *pCopyFence = nullptr;
	uint64_t nCopyFence = 0;
	if ( !m_Transfers.queryDependency( ticket, &pCopyFence, &nCopyFence ) ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	uint64_t nComputeFence = 0;
	if ( !m_ComputeQueue.submitMarkerAfterFence( pCopyFence, nCopyFence, &nComputeFence ) ) {
		m_nLastError = m_ComputeQueue.getLastError();
		return false;
	}

	if ( !snapshotCompletionSet( pSet ) || pSet->nCopyFence < nCopyFence || pSet->nComputeFence != nComputeFence ) {
		m_nLastError = E_FAIL;
		return false;
	}
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Device::copyRoundTripForTest( const void *pInput, void *pOutput, uint64_t nBytes ) noexcept
{
	constexpr uint64_t MAX_TEST_COPY_BYTES = 16ULL * 1024ULL * 1024ULL;
	if ( !isReady() || pInput == nullptr || pOutput == nullptr || nBytes == 0 || nBytes > MAX_TEST_COPY_BYTES ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	if ( m_pPendingUpload != nullptr ) {
		if ( !waitForPendingCopy() ) {
			return false;
		}
		releasePendingCopy();
	}

	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pUpload;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pDeviceBuffer;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pReadback;
	m_nLastError = m_Memory.createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nBytes, pUpload.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_Memory.createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, nBytes, pDeviceBuffer.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_Memory.createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nBytes, pReadback.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	D3D12_RANGE emptyRead = { 0, 0 };
	void *pUploadBytes = nullptr;
	m_nLastError = pUpload->GetResource()->Map( 0, &emptyRead, &pUploadBytes );
	if ( FAILED( m_nLastError ) || pUploadBytes == nullptr ) {
		return false;
	}
	memcpy( pUploadBytes, pInput, ( size_t )nBytes );
	pUpload->GetResource()->Unmap( 0, nullptr );

	m_nLastError = m_pCopyAllocator->Reset();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_pCopyCommands->Reset( m_pCopyAllocator.Get(), nullptr );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_pCopyCommands->CopyBufferRegion( pDeviceBuffer->GetResource(), 0, pUpload->GetResource(), 0, nBytes );
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pDeviceBuffer->GetResource();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	m_pCopyCommands->ResourceBarrier( 1, &barrier );
	m_pCopyCommands->CopyBufferRegion( pReadback->GetResource(), 0, pDeviceBuffer->GetResource(), 0, nBytes );
	m_nLastError = m_pCopyCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	m_pPendingUpload = pUpload.Detach();
	m_pPendingDevice = pDeviceBuffer.Detach();
	m_pPendingReadback = pReadback.Detach();
	m_nPendingFenceValue = 0;
	ID3D12CommandList *pLists[] = { m_pCopyCommands.Get() };
	m_pGraphicsQueue->ExecuteCommandLists( 1, pLists );
	if ( !waitForPendingCopy() ) {
		return false;
	}

	D3D12_RANGE readRange = { 0, ( SIZE_T )nBytes };
	void *pReadBytes = nullptr;
	m_nLastError = m_pPendingReadback->GetResource()->Map( 0, &readRange, &pReadBytes );
	if ( FAILED( m_nLastError ) || pReadBytes == nullptr ) {
		releasePendingCopy();
		return false;
	}
	memcpy( pOutput, pReadBytes, ( size_t )nBytes );
	m_pPendingReadback->GetResource()->Unmap( 0, &emptyRead );
	releasePendingCopy();
	++m_nCompletedCopyCount;
	m_nLastError = S_OK;
	return true;
}
