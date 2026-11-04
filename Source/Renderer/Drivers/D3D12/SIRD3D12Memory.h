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

#ifndef __SIRENGINE_SIRD3D12MEMORY_H__
#define __SIRENGINE_SIRD3D12MEMORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <D3D12MemAlloc.h>

class SIRGeneralHeap;

typedef enum sirD3D12TransferPath_e : uint8_t
{
	SIR_D3D12_TRANSFER_DIRECT_UMA = 0,
	SIR_D3D12_TRANSFER_DIRECT_GPU_UPLOAD = 1,
	SIR_D3D12_TRANSFER_STAGED = 2
} sirD3D12TransferPath_t;

typedef struct sirD3D12TransferPoolConfig_s
{
	uint64_t nSlotBytes;
	bool bPreferDirectMapped;
} sirD3D12TransferPoolConfig_t;

typedef struct sirD3D12ResidentTransferView_s
{
	ID3D12Resource *pWriteResource;
	ID3D12Resource *pGpuResource;
	ID3D12Resource *pReadbackResource;
	uint8_t *pWriteBytes;
	const uint8_t *pReadbackBytes;
	uint64_t nSlotBytes;
	uint64_t nReservedBufferBytes;
	uint64_t nPersistentMappedBytes;
	sirD3D12TransferPath_t ePath;
} sirD3D12ResidentTransferView_t;

// Allocation bytes come from D3D12MA on both memory segments. Swapchain
// backing is external to D3D12MA and charged using the D3D12 allocation
// footprint estimate rather than falsely claiming DXGI ownership.
typedef struct sirD3D12AllocationLedger_s
{
	uint64_t nCommittedBytes;
	uint64_t nPeakCommittedBytes;
	uint64_t nBlockBytes;
	uint64_t nExternalBytes;
	uint64_t nResidentLimitBytes;
} sirD3D12AllocationLedger_t;

// Native D3D12MA owner. The allocation owns its ID3D12Resource; callers
// borrow Allocation::GetResource() without obtaining an extra COM reference.
class SIRD3D12Memory
{
private:
	D3D12MA::Allocator *m_pAllocator = nullptr;
	D3D12MA::ALLOCATION_CALLBACKS m_Callbacks = {};
	SIRGeneralHeap *m_pHostHeap = nullptr;
	uint64_t m_nResidentLimitBytes = 0;
	uint64_t m_nExternalBytes = 0;
	uint64_t m_nPeakTrackedBytes = 0;
	bool enforceResidentLimit( D3D12MA::Allocation **ppAllocation ) noexcept;
	D3D12MA::Allocation *m_pTransferWrite = nullptr;
	D3D12MA::Allocation *m_pTransferGpu = nullptr;
	D3D12MA::Allocation *m_pTransferReadback = nullptr;
	uint8_t *m_pTransferWriteBytes = nullptr;
	const uint8_t *m_pTransferReadbackBytes = nullptr;
	uint64_t m_nTransferSlotBytes = 0;
	uint64_t m_nTransferReservedBytes = 0;
	sirD3D12TransferPath_t m_eTransferPath = SIR_D3D12_TRANSFER_STAGED;
	HRESULT m_nLastError = S_OK;

public:
	SIRD3D12Memory( void ) noexcept;
	~SIRD3D12Memory( void ) noexcept;
	SIRD3D12Memory( const SIRD3D12Memory & ) = delete;
	SIRD3D12Memory &operator=( const SIRD3D12Memory & ) = delete;

	bool initialize( IDXGIAdapter *pAdapter, ID3D12Device *pDevice, SIRGeneralHeap *pCpuHeap = nullptr ) noexcept;
	bool configureResidentBudget( uint64_t nBytes ) noexcept;
	bool queryAllocationLedger( sirD3D12AllocationLedger_t *pLedger ) const noexcept;
	bool reserveExternalBytes( uint64_t nBytes ) noexcept;
	void releaseExternalBytes( uint64_t nBytes ) noexcept;
	void shutdown( void ) noexcept;
	bool initializeTransferArena( const sirD3D12TransferPoolConfig_t &config, uint32_t nSlots ) noexcept;
	void releaseTransferArena( bool bGpuComplete ) noexcept;
	bool queryTransferArena( sirD3D12ResidentTransferView_t *pView ) const noexcept;
	// Called only when a GPU completion cannot be proved: intentionally retains
	// native allocations rather than freeing memory still referenced by a queue.
	void abandon( void ) noexcept;
	bool isReady( void ) const noexcept;
	HRESULT createBuffer( D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState, uint64_t nBytes,
	                      D3D12MA::Allocation **ppAllocation, D3D12_RESOURCE_FLAGS eResourceFlags = D3D12_RESOURCE_FLAG_NONE ) noexcept;
	HRESULT createTexture2D( uint32_t nWidth, uint32_t nHeight, DXGI_FORMAT eFormat, D3D12_RESOURCE_FLAGS eFlags,
	                         D3D12_RESOURCE_STATES eInitialState, const D3D12_CLEAR_VALUE *pClearValue,
	                         D3D12MA::Allocation **ppAllocation ) noexcept;
	HRESULT createTexture( const D3D12_RESOURCE_DESC &resource, D3D12_RESOURCE_STATES eInitialState, const D3D12_CLEAR_VALUE *pClearValue,
	                       D3D12MA::Allocation **ppAllocation ) noexcept;
	HRESULT getLastError( void ) const noexcept;
};

#endif // __SIRENGINE_SIRD3D12MEMORY_H__
