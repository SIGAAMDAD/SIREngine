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

#ifndef __SIRENGINE_SIRD3D12TRANSFERS_H__
#define __SIRENGINE_SIRD3D12TRANSFERS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D12/SIRD3D12Memory.h>
#include <wrl/client.h>

// Local native ticket. The eventual shared renderer ABI will supply device and
// queue generation identifiers; the serial prevents slot reuse from reviving a
// stale ticket within a single SIRD3D12Transfers lifetime.
typedef struct sirD3D12TransferTicket_s
{
	uint64_t nSerial;
	uint32_t iSlot;
} sirD3D12TransferTicket_t;

typedef struct sirD3D12TransferSlot_s
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommands;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pReadbackAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pReadbackCommands;
	D3D12MA::Allocation *pUpload = nullptr;
	D3D12MA::Allocation *pDevice = nullptr;
	D3D12MA::Allocation *pReadback = nullptr;
	uint64_t nCapacityBytes = 0;
	uint64_t nBytes = 0;
	uint64_t nSerial = 0;
	uint64_t nFence = 0;
	bool bOccupied = false;
} sirD3D12TransferSlot_t;

// Single-caller native copy queue with a fixed number of in-flight slots.
// No hidden waits in submit(). Each slot owns its command allocator/list and
// D3D12MA buffers until its native copy-queue fence has completed.
class SIRD3D12Transfers
{
private:
	static constexpr uint32_t s_nSlotCount = 3;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
	sirD3D12TransferSlot_t m_szSlots[s_nSlotCount];
	SIRD3D12Memory *m_pMemory = nullptr;
	HANDLE m_hEvent = nullptr;
	uint64_t m_nNextSerial = 1;
	uint64_t m_nNextFence = 1;
	uint64_t m_nLastSubmittedFence = 0;
	uint64_t m_nCompletedCopies = 0;
	bool m_bSubmissionUnsafe = false;
	HRESULT m_nLastError = S_OK;

	sirD3D12TransferSlot_t *findSlot( sirD3D12TransferTicket_t ticket ) noexcept;
	bool growSlot( sirD3D12TransferSlot_t *pSlot, uint64_t nBytes ) noexcept;
	bool waitForFence( uint64_t nFence, uint32_t nTimeoutMs ) noexcept;
	void releaseSlotResources( sirD3D12TransferSlot_t *pSlot ) noexcept;

public:
	SIRD3D12Transfers( void ) noexcept;
	~SIRD3D12Transfers( void ) noexcept;
	SIRD3D12Transfers( const SIRD3D12Transfers & ) = delete;
	SIRD3D12Transfers &operator=( const SIRD3D12Transfers & ) = delete;

	bool initialize( ID3D12Device *pDevice, SIRD3D12Memory *pMemory ) noexcept;
	// Returns false if native completion could not be proved and the caller
	// must preserve the allocator and device backing until the GPU is quiescent.
	bool shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	uint32_t getSlotCount( void ) const noexcept;
	uint64_t getCompletedCopyCount( void ) const noexcept;
	HRESULT getLastError( void ) const noexcept;

	bool submit( const void *pSource, uint64_t nBytes, sirD3D12TransferTicket_t *pTicket ) noexcept;
	bool poll( sirD3D12TransferTicket_t ticket, bool *pCompleted ) noexcept;
	bool wait( sirD3D12TransferTicket_t ticket, uint32_t nTimeoutMs ) noexcept;
	bool receive( sirD3D12TransferTicket_t ticket, void *pDestination, uint64_t nCapacityBytes ) noexcept;
	bool discard( sirD3D12TransferTicket_t ticket ) noexcept;
};

#endif // __SIRENGINE_SIRD3D12TRANSFERS_H__
