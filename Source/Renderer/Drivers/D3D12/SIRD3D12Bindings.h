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

#ifndef __SIRENGINE_SIRD3D12BINDINGS_H__
#define __SIRENGINE_SIRD3D12BINDINGS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D12/SIRD3D12Pipelines.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Resources.h>

// One root descriptor table per declared binding, allocated in the fixed
// shader-visible heaps. The public nSetIndex maps to HLSL register space.
typedef struct sirD3D12BindingRange_s
{
	sirGraphicsBindingDesc_t desc;
	D3D12_CPU_DESCRIPTOR_HANDLE hCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hGpu;
	D3D12_DESCRIPTOR_HEAP_TYPE eHeap;
	uint32_t nRootIndex;
	uint32_t nFlatStart;
	uint32_t iDescriptor;
	uint32_t nDescriptorCount;
	bool bAllocated;
} sirD3D12BindingRange_t;

typedef struct sirD3D12BindingSetSlot_s
{
	sirD3D12BindingRange_t szRanges[32];
	sirGraphicsHandle_t szResources[128];
	sirGraphicsHandle_t hPipeline;
	sirD3D12CompletionSet_t retireCompletion;
	uint32_t nGeneration;
	uint32_t nSetIndex;
	uint32_t nRangeCount;
	uint32_t nFlatCount;
	uint32_t nCommandReferences;
	bool bLive;
	bool bRetiring;
} sirD3D12BindingSetSlot_t;

// Mutable descriptors are frozen from command recording until the last
// submission fence retires. No per-dispatch allocation and no hidden growth.
class SIRD3D12Bindings
{
private:
	static constexpr uint32_t s_nSetCapacity = 16;
	static constexpr uint32_t s_nDescriptorCapacity = 128;
	sirD3D12BindingSetSlot_t m_szSets[s_nSetCapacity] = {};
	SIRD3D12Device *m_pNative = nullptr;
	SIRD3D12Pipelines *m_pPipelines = nullptr;
	SIRD3D12Resources *m_pResources = nullptr;
	ID3D12Device *m_pDevice = nullptr;
	uint32_t m_nDeviceGeneration = 0;

	sirD3D12BindingSetSlot_t *findSlot( sirGraphicsHandle_t hSet ) noexcept;
	void releaseSlot( uint32_t iSlot ) noexcept;
	bool pollRetirement( const sirD3D12CompletionSet_t& completion, bool *pCompleted ) noexcept;

public:
	SIRD3D12Bindings( void ) noexcept = default;
	~SIRD3D12Bindings( void ) noexcept;
	SIRD3D12Bindings( const SIRD3D12Bindings& ) = delete;
	SIRD3D12Bindings& operator=( const SIRD3D12Bindings& ) = delete;

	bool initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines, uint32_t nGeneration ) noexcept;
	void shutdown( bool bGpuComplete ) noexcept;
	void collectRetired( void ) noexcept;
	sirGraphicsResult_t createSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t updateSet( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites, uint32_t nWriteCount ) noexcept;
	sirGraphicsResult_t retireSet( sirGraphicsHandle_t hSet, const sirGraphicsCompletionSet_t& completion ) noexcept;
	sirGraphicsResult_t retainCommandReference( sirGraphicsHandle_t hSet ) noexcept;
	void releaseCommandReference( sirGraphicsHandle_t hSet ) noexcept;
	const sirD3D12BindingSetSlot_t *querySlot( sirGraphicsHandle_t hSet ) noexcept;
	bool isComplete( const sirD3D12BindingSetSlot_t& slot ) const noexcept;
	ID3D12DescriptorHeap *getHeap( D3D12_DESCRIPTOR_HEAP_TYPE eHeap ) noexcept;
};

#endif // __SIRENGINE_SIRD3D12BINDINGS_H__
