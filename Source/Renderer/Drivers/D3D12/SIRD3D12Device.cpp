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

HRESULT sirD3D12EnumerateAdapters( IDXGIFactory6 *pFactory, uint32_t nRequestedIndex, IDXGIAdapter4 **ppSelected,
                                   uint32_t *pCount ) noexcept
{
	if ( pFactory == nullptr || ( ppSelected == nullptr && pCount == nullptr ) ) {
		return E_INVALIDARG;
	}
	if ( ppSelected != nullptr ) {
		*ppSelected = nullptr;
	}
	if ( pCount != nullptr ) {
		*pCount = 0;
	}
	uint32_t nFound = 0;
	for ( UINT iDxgi = 0; iDxgi < 256; ++iDxgi ) {
		Microsoft::WRL::ComPtr<IDXGIAdapter4> pCandidate;
		HRESULT nResult =
		    pFactory->EnumAdapterByGpuPreference( iDxgi, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( pCandidate.GetAddressOf() ) );
		if ( nResult == DXGI_ERROR_NOT_FOUND ) {
			break;
		}
		if ( FAILED( nResult ) ) {
			return nResult;
		}
		DXGI_ADAPTER_DESC3 description = {};
		nResult = pCandidate->GetDesc3( &description );
		if ( FAILED( nResult ) ) {
			return nResult;
		}
		if ( ( description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE ) != 0 ) {
			continue;
		}
		Microsoft::WRL::ComPtr<ID3D12Device> pProbe;
		if ( FAILED( D3D12CreateDevice( pCandidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( pProbe.GetAddressOf() ) ) ) ) {
			continue;
		}
		if ( ppSelected != nullptr && nFound == nRequestedIndex ) {
			*ppSelected = pCandidate.Detach();
			return S_OK;
		}
		++nFound;
	}
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pWarp;
	if ( SUCCEEDED( pFactory->EnumWarpAdapter( IID_PPV_ARGS( pWarp.GetAddressOf() ) ) ) ) {
		Microsoft::WRL::ComPtr<ID3D12Device> pProbe;
		if ( SUCCEEDED( D3D12CreateDevice( pWarp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( pProbe.GetAddressOf() ) ) ) ) {
			if ( ppSelected != nullptr && nFound == nRequestedIndex ) {
				*ppSelected = pWarp.Detach();
				return S_OK;
			}
			++nFound;
		}
	}
	if ( pCount != nullptr ) {
		*pCount = nFound;
	}
	return ppSelected != nullptr || nFound == 0 ? DXGI_ERROR_NOT_FOUND : S_OK;
}

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

bool SIRD3D12ComputeQueue::queryDependencyFence( uint64_t nFenceValue, ID3D12Fence **ppFence ) const noexcept
{
	if ( ppFence == nullptr ) {
		return false;
	}
	*ppFence = nullptr;
	if ( !isReady() || nFenceValue == 0 || nFenceValue > m_nSubmittedFence ) {
		return false;
	}

	*ppFence = m_pFence.Get();
	return true;
}

HRESULT SIRD3D12ComputeQueue::getLastError( void ) const noexcept
{
	return m_nLastError;
}

ID3D12CommandQueue *SIRD3D12ComputeQueue::getSubmissionQueue( void ) const noexcept
{
	return isReady() ? m_pQueue.Get() : nullptr;
}

ID3D12Fence *SIRD3D12ComputeQueue::getSubmissionFence( void ) const noexcept
{
	return isReady() ? m_pFence.Get() : nullptr;
}

bool SIRD3D12ComputeQueue::submitRecorded( ID3D12CommandList *const *ppLists, uint32_t nCount, uint64_t *pFence ) noexcept
{
	if ( pFence == nullptr ) {
		return false;
	}
	*pFence = 0;
	if ( !isReady() || ppLists == nullptr || nCount == 0 || m_nNextFence == UINT64_MAX ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	m_pQueue->ExecuteCommandLists( nCount, ppLists );
	m_nLastError = m_pQueue->Signal( m_pFence.Get(), m_nNextFence );
	if ( FAILED( m_nLastError ) ) {
		m_bUnsafe = true;
		return false;
	}
	m_nSubmittedFence = m_nNextFence;
	*pFence = m_nNextFence;
	++m_nNextFence;
	return true;
}

void SIRD3D12ComputeQueue::markSubmissionUnsafe( void ) noexcept
{
	m_bUnsafe = true;
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

bool SIRD3D12Device::initialize( uint32_t nAdapterIndex, const sirD3D12TransferPoolConfig_t& transferConfig, SIRGeneralHeap *pCpuHeap,
                                 const sirD3D12DiagnosticConfig_t& diagnostics ) noexcept
{
	if ( isReady() || m_pFactory != nullptr || m_bGraphicsUnsafe ) {
		m_nLastError = E_FAIL;
		return false;
	}

	const sirGraphicsResult_t eConfigure = m_Diagnostics.configureBeforeDevice( diagnostics );
	if ( eConfigure != SIR_GRAPHICS_SUCCESS ) {
		m_nLastError = eConfigure == SIR_GRAPHICS_INVALID_ARGUMENT ? E_INVALIDARG : E_NOTIMPL;
		return false;
	}
	m_nLastError = CreateDXGIFactory2( 0, IID_PPV_ARGS( m_pFactory.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	m_nLastError = sirD3D12EnumerateAdapters( m_pFactory.Get(), nAdapterIndex, m_pAdapter.ReleaseAndGetAddressOf(), nullptr );
	if ( SUCCEEDED( m_nLastError ) ) {
		m_nLastError = D3D12CreateDevice( m_pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( m_pDevice.GetAddressOf() ) );
	}

	if ( m_pDevice == nullptr ) {
		if ( SUCCEEDED( m_nLastError ) ) {
			m_nLastError = DXGI_ERROR_NOT_FOUND;
		}
		shutdown();
		return false;
	}

	m_Diagnostics.attachDevice( m_pDevice.Get() );

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

	if ( !m_Memory.initialize( m_pAdapter.Get(), m_pDevice.Get(), pCpuHeap ) ) {
		m_nLastError = m_Memory.getLastError();
		shutdown();
		return false;
	}

	if ( !m_Transfers.initialize( m_pDevice.Get(), &m_Memory, transferConfig ) ) {
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

bool SIRD3D12Device::waitForGraphicsFence( uint64_t nFenceValue, uint32_t nTimeoutMs ) noexcept
{
	if ( m_pGraphicsFence == nullptr || m_hFenceEvent == nullptr || m_bGraphicsUnsafe || nFenceValue == 0 ||
	     nFenceValue >= m_nNextFenceValue ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	uint64_t nCompleted = m_pGraphicsFence->GetCompletedValue();
	if ( nCompleted == UINT64_MAX ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		m_Diagnostics.captureStatus( m_nLastError );
		return false;
	}
	if ( nCompleted < nFenceValue ) {
		m_nLastError = m_pGraphicsFence->SetEventOnCompletion( nFenceValue, m_hFenceEvent );
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
		const DWORD nWait = WaitForSingleObject( m_hFenceEvent, nTimeoutMs );
		if ( nWait != WAIT_OBJECT_0 ) {
			m_nLastError = nWait == WAIT_TIMEOUT ? HRESULT_FROM_WIN32( ERROR_TIMEOUT ) : HRESULT_FROM_WIN32( GetLastError() );
			return false;
		}
		nCompleted = m_pGraphicsFence->GetCompletedValue();
	}
	if ( nCompleted == UINT64_MAX || nCompleted < nFenceValue ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		m_Diagnostics.captureStatus( m_nLastError );
		return false;
	}
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Device::retireDirectComputeDependency( void ) noexcept
{
	if ( m_bGraphicsUnsafe ) {
		return false;
	}
	if ( m_pDirectComputeDependency == nullptr ) {
		return true;
	}
	if ( m_nDirectDependencyFenceValue == 0 || !waitForGraphicsFence( m_nDirectDependencyFenceValue, 15000 ) ) {
		return false;
	}
	m_pDirectComputeDependency.Reset();
	m_nDirectDependencyFenceValue = 0;
	return true;
}

void SIRD3D12Device::releaseRetiredAllocations( bool bCompletionProven ) noexcept
{
	for ( uint32_t iEntry = 0; iEntry < m_nRetiredAllocationCount; ++iEntry ) {
		D3D12MA::Allocation *pAllocation = m_szRetiredAllocations[iEntry].pAllocation;
		if ( bCompletionProven && pAllocation != nullptr ) {
			pAllocation->Release();
		}
		// On an uncertain queue completion, intentionally abandon the
		// allocation with its underlying D3D12MA allocator and GPU device.
		m_szRetiredAllocations[iEntry] = {};
	}
	m_nRetiredAllocationCount = 0;
}

void SIRD3D12Device::shutdown( void ) noexcept
{
	// DIRECT can wait on COMPUTE, and COMPUTE can wait on COPY.
	// Drain dependent queues from the consumer back to the producer.
	const bool bGraphicsSafe = !m_bGraphicsUnsafe && ( m_nNextFenceValue <= 1 || waitForGraphicsFence( m_nNextFenceValue - 1, 15000 ) );
	const bool bDirectSafe = bGraphicsSafe && retireDirectComputeDependency();
	const bool bComputeSafe = m_ComputeQueue.shutdown();
	const bool bTransferSafe = m_Transfers.shutdown();
	const bool bAllQueuesSafe = bDirectSafe && bComputeSafe && bTransferSafe;
	releaseRetiredAllocations( bAllQueuesSafe );
	if ( m_pDevice != nullptr ) {
		( void )m_Diagnostics.checkHealth();
	}
	m_Diagnostics.detachDevice();
	if ( bAllQueuesSafe ) {
		m_Memory.shutdown();
		m_pGraphicsQueue.Reset();
		m_pGraphicsFence.Reset();
		m_pDevice.Reset();
	} else {
		// The command list may still reference every pending allocation. Leak
		// those native objects deliberately rather than create a GPU use-after-free.
		( void )m_pDirectComputeDependency.Detach();
		( void )m_pGraphicsQueue.Detach();
		( void )m_pGraphicsFence.Detach();
		( void )m_pDevice.Detach();
		m_Memory.abandon();
	}
	if ( bDirectSafe && bComputeSafe && bTransferSafe && m_hFenceEvent != nullptr ) {
		CloseHandle( m_hFenceEvent );
	}
	m_hFenceEvent = nullptr;
	m_pAdapter.Reset();
	m_pFactory.Reset();
	m_nNextFenceValue = 0;
	m_nDirectDependencyFenceValue = 0;
	m_bGraphicsUnsafe = !bDirectSafe;
	m_nDeviceGeneration = 0;
}

bool SIRD3D12Device::isReady( void ) const noexcept
{
	return m_pDevice != nullptr && m_pGraphicsQueue != nullptr && m_pGraphicsFence != nullptr && m_hFenceEvent != nullptr &&
	       m_Memory.isReady() && m_Transfers.isReady() && m_ComputeQueue.isReady() && m_nDeviceGeneration != 0 && !m_bGraphicsUnsafe;
}

sirGraphicsResult_t SIRD3D12Device::submitRecorded( sirGraphicsQueue_t eQueue, ID3D12CommandList *const *ppLists, uint32_t nCount,
                                                    const sirGraphicsCompletion_t *pWaits, uint32_t nWaitCount,
                                                    sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( !isReady() ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( eQueue >= SIR_GRAPHICS_QUEUE_COUNT || ppLists == nullptr || nCount == 0 || nCount > 4 ||
	     ( nWaitCount != 0 && pWaits == nullptr ) || nWaitCount > 64 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12CompletionSet_t submitted = {};
	if ( !snapshotCompletionSet( &submitted ) ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	ID3D12CommandQueue *pQueue = eQueue == SIR_GRAPHICS_QUEUE_DIRECT    ? m_pGraphicsQueue.Get()
	                             : eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ? m_ComputeQueue.getSubmissionQueue()
	                                                                    : m_Transfers.getSubmissionQueue();
	if ( pQueue == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	ID3D12Fence *szDependencies[64] = {};
	for ( uint32_t iWait = 0; iWait < nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t& wait = pWaits[iWait];
		if ( wait.nDeviceGeneration != submitted.nDeviceGeneration || wait.eQueue >= SIR_GRAPHICS_QUEUE_COUNT || wait.nValue == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint64_t nLast = wait.eQueue == SIR_GRAPHICS_QUEUE_DIRECT    ? submitted.nGraphicsFence
		                       : wait.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ? submitted.nComputeFence
		                                                                   : submitted.nCopyFence;
		if ( wait.nValue > nLast ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szDependencies[iWait] = wait.eQueue == SIR_GRAPHICS_QUEUE_DIRECT    ? m_pGraphicsFence.Get()
		                        : wait.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ? m_ComputeQueue.getSubmissionFence()
		                                                                    : m_Transfers.getSubmissionFence();
		if ( szDependencies[iWait] == nullptr ) {
			return SIR_GRAPHICS_DEVICE_LOST;
		}
	}
	for ( uint32_t iWait = 0; iWait < nWaitCount; ++iWait ) {
		m_nLastError = pQueue->Wait( szDependencies[iWait], pWaits[iWait].nValue );
		if ( FAILED( m_nLastError ) ) {
			if ( eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
				m_bGraphicsUnsafe = true;
			} else if ( eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
				m_ComputeQueue.markSubmissionUnsafe();
			} else {
				m_Transfers.markSubmissionUnsafe();
			}
			m_Diagnostics.captureStatus( m_nLastError );
			return sirD3D12ClassifyStatus( m_nLastError );
		}
	}
	uint64_t nFence = 0;
	if ( eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
		if ( m_nNextFenceValue == UINT64_MAX ) {
			m_bGraphicsUnsafe = nWaitCount != 0;
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		m_pGraphicsQueue->ExecuteCommandLists( nCount, ppLists );
		m_nLastError = m_pGraphicsQueue->Signal( m_pGraphicsFence.Get(), m_nNextFenceValue );
		if ( FAILED( m_nLastError ) ) {
			m_bGraphicsUnsafe = true;
			m_Diagnostics.captureStatus( m_nLastError );
			return sirD3D12ClassifyStatus( m_nLastError );
		}
		nFence = m_nNextFenceValue;
		++m_nNextFenceValue;
	} else if ( eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
		if ( !m_ComputeQueue.submitRecorded( ppLists, nCount, &nFence ) ) {
			m_ComputeQueue.markSubmissionUnsafe();
			m_nLastError = m_ComputeQueue.getLastError();
			m_Diagnostics.captureStatus( m_nLastError );
			return sirD3D12ClassifyStatus( m_nLastError );
		}
	} else if ( !m_Transfers.submitRecorded( ppLists, nCount, &nFence ) ) {
		m_Transfers.markSubmissionUnsafe();
		m_nLastError = m_Transfers.getLastError();
		m_Diagnostics.captureStatus( m_nLastError );
		return sirD3D12ClassifyStatus( m_nLastError );
	}
	*pCompletion = { ( uint32_t )submitted.nDeviceGeneration, eQueue, nFence };
	return SIR_GRAPHICS_SUCCESS;
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

bool SIRD3D12Device::queryDiagnostics( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept
{
	return m_Diagnostics.querySnapshot( pSnapshot );
}

bool SIRD3D12Device::checkDeviceHealth( void ) noexcept
{
	return m_Diagnostics.checkHealth();
}

void SIRD3D12Device::captureDeviceFailure( HRESULT nResult ) noexcept
{
	m_Diagnostics.captureStatus( nResult );
}

bool SIRD3D12Device::queryAllocationLedger( sirD3D12AllocationLedger_t *pLedger ) const noexcept
{
	return isReady() && m_Memory.queryAllocationLedger( pLedger );
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

bool SIRD3D12Device::queryUnifiedMemory( bool *pUnified ) const noexcept
{
	if ( pUnified == nullptr || !isReady() ) {
		return false;
	}
	*pUnified = false;
	D3D12_FEATURE_DATA_ARCHITECTURE1 architecture = {};
	architecture.NodeIndex = 0;
	if ( FAILED( m_pDevice->CheckFeatureSupport( D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof( architecture ) ) ) ) {
		return false;
	}
	*pUnified = architecture.UMA != FALSE;
	return true;
}

IDXGIFactory6 *SIRD3D12Device::getNativeFactory( void ) const noexcept
{
	return isReady() ? m_pFactory.Get() : nullptr;
}

ID3D12CommandQueue *SIRD3D12Device::getDirectQueue( void ) const noexcept
{
	return isReady() ? m_pGraphicsQueue.Get() : nullptr;
}

ID3D12Device *SIRD3D12Device::getNativeDevice( void ) const noexcept
{
	return isReady() ? m_pDevice.Get() : nullptr;
}

SIRD3D12Memory *SIRD3D12Device::getMemory( void ) noexcept
{
	return isReady() ? &m_Memory : nullptr;
}

SIRD3D12Transfers& SIRD3D12Device::getTransfers( void ) noexcept
{
	return m_Transfers;
}

const SIRD3D12Transfers& SIRD3D12Device::getTransfers( void ) const noexcept
{
	return m_Transfers;
}

SIRD3D12ComputeQueue& SIRD3D12Device::getComputeQueue( void ) noexcept
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

bool SIRD3D12Device::pollCompletionSet( const sirD3D12CompletionSet_t& set, bool *pCompleted ) noexcept
{
	if ( pCompleted == nullptr ) {
		m_nLastError = E_POINTER;
		return false;
	}
	*pCompleted = false;
	if ( isReady() && !m_Diagnostics.checkHealth() ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
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
		m_Diagnostics.captureStatus( m_nLastError );
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

bool SIRD3D12Device::submitGraphicsAfterCompute( const sirD3D12CompletionSet_t& computeSet, sirD3D12CompletionSet_t *pSet ) noexcept
{
	if ( pSet == nullptr || !isReady() ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	*pSet = {};

	bool bPriorCompleted = false;

	if ( !pollCompletionSet( computeSet, &bPriorCompleted ) || computeSet.nComputeFence == 0 ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	ID3D12Fence *pComputeFence = nullptr;
	if ( !m_ComputeQueue.queryDependencyFence( computeSet.nComputeFence, &pComputeFence ) ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	if ( m_nNextFenceValue == UINT64_MAX ) {
		m_nLastError = E_FAIL;
		return false;
	}

	// The caller never waits: report queue saturation while a prior DIRECT
	// dependency remains outstanding, and release it only after completion.
	if ( m_pDirectComputeDependency != nullptr ) {
		const uint64_t nCompleted = m_pGraphicsFence->GetCompletedValue();
		if ( nCompleted == UINT64_MAX ) {
			m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
			return false;
		}
		if ( nCompleted < m_nDirectDependencyFenceValue ) {
			m_nLastError = HRESULT_FROM_WIN32( ERROR_BUSY );
			return false;
		}
		m_pDirectComputeDependency.Reset();
		m_nDirectDependencyFenceValue = 0;
	}

	Microsoft::WRL::ComPtr<ID3D12Fence> pOwnedDependency = pComputeFence;
	m_nLastError = m_pGraphicsQueue->Wait( pOwnedDependency.Get(), computeSet.nComputeFence );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const uint64_t nGraphicsFence = m_nNextFenceValue;
	m_nLastError = m_pGraphicsQueue->Signal( m_pGraphicsFence.Get(), nGraphicsFence );
	if ( FAILED( m_nLastError ) ) {
		// Wait was already queued. Keep the fence and queue alive even
		// though the matching signal could not be submitted.
		m_pDirectComputeDependency = pOwnedDependency;
		m_bGraphicsUnsafe = true;
		return false;
	}

	m_pDirectComputeDependency = pOwnedDependency;
	m_nDirectDependencyFenceValue = nGraphicsFence;
	++m_nNextFenceValue;
	m_nLastError = S_OK;
	return snapshotCompletionSet( pSet ) && pSet->nGraphicsFence == nGraphicsFence;
}

bool SIRD3D12Device::createNativeBuffer( D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState, uint64_t nBytes,
                                         D3D12MA::Allocation **ppAllocation ) noexcept
{
	if ( ppAllocation == nullptr ) {
		m_nLastError = E_POINTER;
		return false;
	}
	*ppAllocation = nullptr;
	if ( !isReady() ) {
		m_nLastError = E_FAIL;
		return false;
	}

	m_nLastError = m_Memory.createBuffer( eHeapType, eInitialState, nBytes, ppAllocation );
	return SUCCEEDED( m_nLastError ) && *ppAllocation != nullptr;
}

bool SIRD3D12Device::retireNativeAllocation( D3D12MA::Allocation **ppAllocation, const sirD3D12CompletionSet_t& completion ) noexcept
{
	if ( ppAllocation == nullptr || *ppAllocation == nullptr || !isReady() ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	if ( m_nRetiredAllocationCount >= s_nRetirementCapacity ) {
		m_nLastError = HRESULT_FROM_WIN32( ERROR_BUSY );
		return false;
	}

	bool bCompleted = false;
	if ( !pollCompletionSet( completion, &bCompleted ) ) {
		return false;
	}

	const uint32_t iEntry = m_nRetiredAllocationCount;
	m_szRetiredAllocations[iEntry].pAllocation = *ppAllocation;
	m_szRetiredAllocations[iEntry].nCompletion = completion;
	++m_nRetiredAllocationCount;
	*ppAllocation = nullptr;
	m_nLastError = S_OK;
	return true;
}

uint32_t SIRD3D12Device::collectRetiredAllocations( void ) noexcept
{
	if ( !isReady() ) {
		m_nLastError = E_FAIL;
		return 0;
	}

	uint32_t nCollected = 0;
	uint32_t iEntry = 0;
	while ( iEntry < m_nRetiredAllocationCount ) {
		bool bCompleted = false;
		if ( !pollCompletionSet( m_szRetiredAllocations[iEntry].nCompletion, &bCompleted ) ) {
			return nCollected;
		}
		if ( !bCompleted ) {
			++iEntry;
			continue;
		}

		m_szRetiredAllocations[iEntry].pAllocation->Release();
		--m_nRetiredAllocationCount;
		m_szRetiredAllocations[iEntry] = m_szRetiredAllocations[m_nRetiredAllocationCount];
		m_szRetiredAllocations[m_nRetiredAllocationCount] = {};
		++nCollected;
	}
	m_nLastError = S_OK;
	return nCollected;
}

uint32_t SIRD3D12Device::getRetiredAllocationCount( void ) const noexcept
{
	return m_nRetiredAllocationCount;
}

uint32_t SIRD3D12Device::getRetirementCapacity( void ) const noexcept
{
	return s_nRetirementCapacity;
}
