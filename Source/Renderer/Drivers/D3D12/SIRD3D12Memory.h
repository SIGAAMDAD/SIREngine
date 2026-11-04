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

// Native D3D12MA owner. The allocation owns its ID3D12Resource; callers
// borrow Allocation::GetResource() without obtaining an extra COM reference.
class SIRD3D12Memory
{
private:
	D3D12MA::Allocator *m_pAllocator = nullptr;
	HRESULT m_nLastError = S_OK;

public:
	SIRD3D12Memory( void ) noexcept;
	~SIRD3D12Memory( void ) noexcept;
	SIRD3D12Memory( const SIRD3D12Memory & ) = delete;
	SIRD3D12Memory &operator=( const SIRD3D12Memory & ) = delete;

	bool initialize( IDXGIAdapter *pAdapter, ID3D12Device *pDevice ) noexcept;
	void shutdown( void ) noexcept;
	// Called only when a GPU completion cannot be proved: intentionally retains
	// native allocations rather than freeing memory still referenced by a queue.
	void abandon( void ) noexcept;
	bool isReady( void ) const noexcept;
	HRESULT createBuffer( D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState, uint64_t nBytes,
	                      D3D12MA::Allocation **ppAllocation, D3D12_RESOURCE_FLAGS eResourceFlags = D3D12_RESOURCE_FLAG_NONE ) noexcept;
	HRESULT getLastError( void ) const noexcept;
};

#endif // __SIRENGINE_SIRD3D12MEMORY_H__
