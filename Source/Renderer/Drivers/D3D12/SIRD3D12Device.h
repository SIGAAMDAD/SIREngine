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

#ifndef __SIRENGINE_SIRD3D12DEVICE_H__
#define __SIRENGINE_SIRD3D12DEVICE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Memory.h>
#include <Renderer/Drivers/D3D12/SIRD3D12ComputeKernel.h>
#include <Renderer/Drivers/D3D12/SIRD3D12GraphicsKernel.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Transfers.h>

typedef struct sirD3D12AdapterInfo_s
{
	uint32_t nVendorId;
	uint32_t nDeviceId;
	uint64_t nDedicatedVideoBytes;
	uint64_t nSharedSystemBytes;
	int64_t nAdapterLuid;
	bool bSoftware;
	wchar_t szName[128];
} sirD3D12AdapterInfo_t;

typedef struct sirD3D12MemoryInfo_s
{
	uint64_t nBudgetBytes;
	uint64_t nCurrentUsageBytes;
	uint64_t nAvailableForReservationBytes;
	uint64_t nCurrentReservationBytes;
} sirD3D12MemoryInfo_t;

typedef struct sirD3D12CompletionSet_s
{
	uint64_t nDeviceGeneration;
	uint64_t nGraphicsFence;
	uint64_t nComputeFence;
	uint64_t nCopyFence;
} sirD3D12CompletionSet_t;

typedef struct sirD3D12RetiredAllocation_s
{
	D3D12MA::Allocation *pAllocation;
	sirD3D12CompletionSet_t nCompletion;
} sirD3D12RetiredAllocation_t;

// Native single-caller COMPUTE queue. An empty command-list marker verifies
// submission and fence ownership until compiled compute PSOs become available.
class SIRD3D12ComputeQueue
{
private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommands;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pPendingDependencyFence;
	SIRD3D12ComputeKernel m_Kernel;
	HANDLE m_hEvent = nullptr;
	uint64_t m_nNextFence = 1;
	uint64_t m_nSubmittedFence = 0;
	bool m_bUnsafe = false;
	HRESULT m_nLastError = S_OK;

public:
	SIRD3D12ComputeQueue( void ) noexcept;
	~SIRD3D12ComputeQueue( void ) noexcept;
	SIRD3D12ComputeQueue( const SIRD3D12ComputeQueue & ) = delete;
	SIRD3D12ComputeQueue &operator=( const SIRD3D12ComputeQueue & ) = delete;

	bool initialize( ID3D12Device *pDevice ) noexcept;
	bool shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	bool submitMarker( uint64_t *pFenceValue ) noexcept;
	bool initializeKernel( ID3D12Device *pDevice, SIRD3D12Memory *pMemory, const void *pDxil, size_t nDxilBytes,
	                       uint32_t nMaxElements ) noexcept;
	bool dispatchKernel( uint32_t nElements, uint32_t nSeed, uint64_t *pFenceValue ) noexcept;
	bool readKernel( uint64_t nFence, uint32_t *pOutput, uint32_t nCapacity ) noexcept;
	bool submitMarkerAfterFence( ID3D12Fence *pDependencyFence, uint64_t nDependencyFence, uint64_t *pFenceValue ) noexcept;
	bool pollFence( uint64_t nFence, bool *pCompleted ) const noexcept;
	bool waitForFence( uint64_t nFence, uint32_t nTimeoutMs ) noexcept;
	bool getCompletedFence( uint64_t *pCompletedFence ) const noexcept;
	// Borrowed until the caller takes its own COM reference.
	bool queryDependencyFence( uint64_t nFenceValue, ID3D12Fence **ppFence ) const noexcept;
	uint64_t getLastSubmittedFence( void ) const noexcept;
	HRESULT getLastError( void ) const noexcept;
};

// Native, Windows-only bootstrap. This is not the renderer-neutral SIRRenderingDevice ABI.
class SIRD3D12Device
{
private:
	static constexpr uint32_t s_nRetirementCapacity = 32;
	sirD3D12RetiredAllocation_t m_szRetiredAllocations[s_nRetirementCapacity] = {};
	uint32_t m_nRetiredAllocationCount = 0;
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_pFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> m_pAdapter;
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pGraphicsQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pGraphicsFence;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pDirectComputeDependency;
	SIRD3D12Memory m_Memory;
	SIRD3D12Transfers m_Transfers;
	SIRD3D12ComputeQueue m_ComputeQueue;
	SIRD3D12GraphicsKernel m_GraphicsKernel;
	HANDLE m_hFenceEvent = nullptr;
	uint64_t m_nNextFenceValue = 0;
	uint64_t m_nDeviceGeneration = 0;
	uint64_t m_nDirectDependencyFenceValue = 0;
	bool m_bGraphicsUnsafe = false;
	HRESULT m_nLastError = S_OK;

	bool retireDirectComputeDependency( void ) noexcept;
	void releaseRetiredAllocations( bool bCompletionProven ) noexcept;

public:
	SIRD3D12Device( void ) noexcept;
	~SIRD3D12Device( void ) noexcept;

	SIRD3D12Device( const SIRD3D12Device & ) = delete;
	SIRD3D12Device &operator=( const SIRD3D12Device & ) = delete;

	bool initialize( bool bUseWarp, const sirD3D12TransferPoolConfig_t &transferConfig ) noexcept;
	void shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	bool queryAdapterInfo( sirD3D12AdapterInfo_t *pInfo ) const noexcept;
	bool queryLocalMemory( sirD3D12MemoryInfo_t *pInfo ) const noexcept;
	bool queryUnifiedMemory( bool *pUnified ) const noexcept;
	HRESULT getLastError( void ) const noexcept;
	ID3D12Device *getNativeDevice( void ) const noexcept;
	SIRD3D12Memory *getMemory( void ) noexcept;
	SIRD3D12Transfers &getTransfers( void ) noexcept;
	const SIRD3D12Transfers &getTransfers( void ) const noexcept;
	SIRD3D12ComputeQueue &getComputeQueue( void ) noexcept;
	bool snapshotCompletionSet( sirD3D12CompletionSet_t *pSet ) const noexcept;
	bool pollCompletionSet( const sirD3D12CompletionSet_t &set, bool *pCompleted ) noexcept;
	bool submitComputeMarker( sirD3D12CompletionSet_t *pSet ) noexcept;
	bool initializeComputeKernel( const void *pDxil, size_t nDxilBytes, uint32_t nMaxElements ) noexcept;
	bool submitComputeKernel( uint32_t nElements, uint32_t nSeed, sirD3D12CompletionSet_t *pSet ) noexcept;
	bool submitComputeAfterCopy( sirD3D12TransferTicket_t ticket, sirD3D12CompletionSet_t *pSet ) noexcept;
	bool submitGraphicsAfterCompute( const sirD3D12CompletionSet_t &computeSet, sirD3D12CompletionSet_t *pSet ) noexcept;
	bool waitForGraphicsFence( uint64_t nFenceValue, uint32_t nTimeoutMs ) noexcept;
	bool initializeGraphicsKernel( const void *pVertexDxil, size_t nVertexBytes, const void *pPixelDxil, size_t nPixelBytes,
	                               uint32_t nMaxWidth, uint32_t nMaxHeight ) noexcept;
	bool submitGraphicsTriangle( uint32_t nWidth, uint32_t nHeight, sirD3D12CompletionSet_t *pSet ) noexcept;
	bool readGraphicsTriangle( const sirD3D12CompletionSet_t &set, uint8_t *pOutput, uint32_t nRowPitch, size_t nOutputBytes ) noexcept;
	bool getGraphicsKernelCapacityForTest( uint32_t *pWidth, uint32_t *pHeight ) const noexcept;

	// Native-only resource ownership; never pass D3D12MA pointers into GRDG.
	bool createNativeBuffer( D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState, uint64_t nBytes,
	                         D3D12MA::Allocation **ppAllocation ) noexcept;
	// Transfers the allocation's sole owner on success; otherwise the caller retains it.
	bool retireNativeAllocation( D3D12MA::Allocation **ppAllocation, const sirD3D12CompletionSet_t &completion ) noexcept;
	uint32_t collectRetiredAllocations( void ) noexcept;
	uint32_t getRetiredAllocationCount( void ) const noexcept;
	uint32_t getRetirementCapacity( void ) const noexcept;
};

#endif // __SIRENGINE_SIRD3D12DEVICE_H__
