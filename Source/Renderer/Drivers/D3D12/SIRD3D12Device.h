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

// Native, Windows-only bootstrap. This is not the renderer-neutral SIRRenderingDevice ABI.
class SIRD3D12Device
{
private:
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_pFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> m_pAdapter;
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pGraphicsQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pGraphicsFence;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCopyAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCopyCommands;
	SIRD3D12Memory m_Memory;
	D3D12MA::Allocation *m_pPendingUpload = nullptr;
	D3D12MA::Allocation *m_pPendingDevice = nullptr;
	D3D12MA::Allocation *m_pPendingReadback = nullptr;
	HANDLE m_hFenceEvent = nullptr;
	uint64_t m_nNextFenceValue = 0;
	uint64_t m_nPendingFenceValue = 0;
	uint64_t m_nCompletedCopyCount = 0;
	HRESULT m_nLastError = S_OK;

	bool waitForPendingCopy( void ) noexcept;
	void releasePendingCopy( void ) noexcept;

public:
	SIRD3D12Device( void ) noexcept;
	~SIRD3D12Device( void ) noexcept;

	SIRD3D12Device( const SIRD3D12Device & ) = delete;
	SIRD3D12Device &operator=( const SIRD3D12Device & ) = delete;

	bool initialize( bool bUseWarp ) noexcept;
	void shutdown( void ) noexcept;
	bool isReady( void ) const noexcept;
	bool queryAdapterInfo( sirD3D12AdapterInfo_t *pInfo ) const noexcept;
	bool queryLocalMemory( sirD3D12MemoryInfo_t *pInfo ) const noexcept;
	HRESULT getLastError( void ) const noexcept;
	uint64_t getCompletedCopyCount( void ) const noexcept;

	// Synchronous diagnostic fixture only; production transfers will use pooled,
	// asynchronous allocations and queue-completion tokens.
	bool copyRoundTripForTest( const void *pInput, void *pOutput, uint64_t nBytes ) noexcept;
};

#endif // __SIRENGINE_SIRD3D12DEVICE_H__
