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

#ifndef __SIRENGINE_SIRD3D12RESOURCES_H__
#define __SIRENGINE_SIRD3D12RESOURCES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>

// A slot owns either one native D3D12MA allocation or one descriptor.
// Retirement invalidates the public handle immediately, then retains the
// backing and dependent views until all named GPU queue fences complete.
typedef struct sirD3D12ResourceSlot_s
{
	D3D12MA::Allocation *pAllocation;
	ID3D12Resource *pExternalResource;
	void *pMappedBytes;
	sirD3D12CompletionSet_t retireCompletion;
	uint64_t nAllocationBytes;
	uint64_t nSizeBytes;
	uint32_t nGeneration;
	uint32_t nUsage;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
	uint32_t nLayers;
	uint32_t nMips;
	uint32_t nSamples;
	uint32_t nDependentViews;
	uint32_t nCommandReferences;
	uint32_t iDescriptor;
	sirGraphicsHandle_t hParent;
	sirGraphicsHandleType_t eType;
	sirGraphicsMemoryClass_t eMemory;
	sirGraphicsTextureDimension_t eDimension;
	sirGraphicsFormat_t eFormat;
	D3D12_DESCRIPTOR_HEAP_TYPE eDescriptorHeap;
	bool bLive;
	bool bRetiring;
	ID3D12Resource *getNativeResource( void ) const noexcept
	{
		return pExternalResource != nullptr ? pExternalResource : ( pAllocation != nullptr ? pAllocation->GetResource() : nullptr );
	}
} sirD3D12ResourceSlot_t;

// Fixed capacity is deliberate. Resource and descriptor creation are cold
// paths; submit/map/retire/collect require no CPU heap allocations.
class SIRD3D12Resources
{
private:
	static constexpr uint32_t s_nSlotCapacity = 256;
	static constexpr uint32_t s_nDescriptorCapacity = 128;
	sirD3D12ResourceSlot_t m_szSlots[s_nSlotCapacity] = {};
	uint8_t m_szDescriptorsUsed[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES][s_nDescriptorCapacity] = {};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_szDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	uint32_t m_szDescriptorSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};
	SIRD3D12Device *m_pNative = nullptr;
	ID3D12Device *m_pDevice = nullptr;
	SIRD3D12Memory *m_pMemory = nullptr;
	uint32_t m_nDeviceGeneration = 0;
	uint64_t m_nBudgetBytes = 0;
	uint64_t m_nAllocatedBytes = 0;
	uint64_t m_nExternalBytes = 0;
	uint64_t m_nPeakAllocatedBytes = 0;

	sirGraphicsResult_t acquireSlot( sirGraphicsHandleType_t eType, uint32_t *pIndex, sirGraphicsHandle_t *pHandle ) noexcept;
	sirD3D12ResourceSlot_t *findSlot( sirGraphicsHandle_t hHandle, sirGraphicsHandleType_t eType ) noexcept;
	sirGraphicsResult_t reserveDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE eHeap, uint32_t *pIndex, D3D12_CPU_DESCRIPTOR_HANDLE *pCpu ) noexcept;
	void releaseSlot( uint32_t iSlot ) noexcept;
	bool validateRange( const sirD3D12ResourceSlot_t& slot, uint64_t nOffset, uint64_t nBytes ) const noexcept;
	bool pollRetirement( const sirD3D12CompletionSet_t& completion, bool *pCompleted ) noexcept;

public:
	SIRD3D12Resources( void ) noexcept = default;
	~SIRD3D12Resources( void ) noexcept;
	SIRD3D12Resources( const SIRD3D12Resources& ) = delete;
	SIRD3D12Resources& operator=( const SIRD3D12Resources& ) = delete;

	bool initialize( SIRD3D12Device *pNative, uint32_t nGeneration, uint64_t nBudgetBytes ) noexcept;
	void shutdown( bool bGpuComplete ) noexcept;
	void collectRetired( void ) noexcept;
	const sirD3D12ResourceSlot_t *querySlot( sirGraphicsHandle_t hHandle ) noexcept;
	bool queryDescriptor( sirGraphicsHandle_t hView, D3D12_DESCRIPTOR_HEAP_TYPE eHeap, D3D12_CPU_DESCRIPTOR_HANDLE *pDescriptor ) noexcept;
	sirGraphicsResult_t reserveDescriptorSpan( D3D12_DESCRIPTOR_HEAP_TYPE eHeap, uint32_t nCount, uint32_t *pIndex,
	                                           D3D12_CPU_DESCRIPTOR_HANDLE *pCpu, D3D12_GPU_DESCRIPTOR_HANDLE *pGpu ) noexcept;
	void releaseDescriptorSpan( D3D12_DESCRIPTOR_HEAP_TYPE eHeap, uint32_t iStart, uint32_t nCount ) noexcept;
	ID3D12DescriptorHeap *getShaderVisibleHeap( D3D12_DESCRIPTOR_HEAP_TYPE eHeap ) noexcept;
	sirGraphicsResult_t retainCommandReference( sirGraphicsHandle_t hHandle ) noexcept;
	void releaseCommandReference( sirGraphicsHandle_t hHandle ) noexcept;
	uint64_t getAllocatedBytes( void ) const noexcept;
	uint64_t getExternalBytes( void ) const noexcept;
	sirGraphicsResult_t importExternalTexture( ID3D12Resource *pResource, sirGraphicsFormat_t eFormat,
	                                           sirGraphicsHandle_t *pHandle ) noexcept;
	bool canReleaseExternalTexture( sirGraphicsHandle_t hTexture ) noexcept;
	sirGraphicsResult_t releaseExternalTexture( sirGraphicsHandle_t hTexture ) noexcept;
	uint64_t getPeakAllocatedBytes( void ) const noexcept;
	uint64_t getMappedUploadBytes( void ) const noexcept;
	uint64_t getMappedReadbackBytes( void ) const noexcept;

	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept;
	sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept;
	sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept;
	sirGraphicsResult_t invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept;
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t hHandle, const sirGraphicsCompletionSet_t& completion ) noexcept;
};

#endif // __SIRENGINE_SIRD3D12RESOURCES_H__
