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

#ifndef __SIRENGINE_SIRD3D12PIPELINES_H__
#define __SIRENGINE_SIRD3D12PIPELINES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>

typedef struct sirD3D12PipelineSlot_s
{
	Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState;
	sirD3D12CompletionSet_t retireCompletion;
	sirGraphicsBindingDesc_t szBindings[32];
	uint32_t nGeneration;
	uint32_t nBindingCount;
	uint32_t nCommandReferences;
	sirGraphicsPipelineType_t eType;
	sirGraphicsTopology_t eTopology;
	bool bLive;
	bool bRetiring;
} sirD3D12PipelineSlot_t;

typedef struct sirD3D12PipelineCacheStats_s
{
	uint64_t nCacheHits;
	uint64_t nCacheMisses;
	uint64_t nCacheStores;
	uint64_t nRootReuses;
	bool bAvailable;
} sirD3D12PipelineCacheStats_t;

// Backend-owned, bounded PSO and root-signature table. Shader bytes are
// supplied by the offline cooker and consumed synchronously by D3D12;
// no HLSL compilation, runtime shader allocations or GRDG dependency.
class SIRD3D12Pipelines
{
public:
	SIRD3D12Pipelines( void ) noexcept = default;

	~SIRD3D12Pipelines( void ) noexcept;
	SIRD3D12Pipelines( const SIRD3D12Pipelines& ) = delete;
	SIRD3D12Pipelines& operator=( const SIRD3D12Pipelines& ) = delete;

	bool initialize( SIRD3D12Device *pNative, uint32_t nGeneration ) noexcept;
	sirGraphicsResult_t loadPipelineCache( const void *pData, uint64_t nBytes ) noexcept;
	sirGraphicsResult_t serializePipelineCache( void *pData, uint64_t *pBytes ) const noexcept;
	sirGraphicsResult_t queryPipelineCacheStats( sirD3D12PipelineCacheStats_t *pStats ) const noexcept;
	void shutdown( bool bGpuComplete ) noexcept;
	void collectRetired( void ) noexcept;
	const sirD3D12PipelineSlot_t *querySlot( sirGraphicsHandle_t hPipeline ) noexcept;
	sirGraphicsResult_t retainCommandReference( sirGraphicsHandle_t hPipeline ) noexcept;
	void releaseCommandReference( sirGraphicsHandle_t hPipeline ) noexcept;
	sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept;
	sirGraphicsResult_t retirePipeline( sirGraphicsHandle_t hPipeline, const sirGraphicsCompletionSet_t& completion ) noexcept;
	bool queryNative( sirGraphicsHandle_t hPipeline, ID3D12PipelineState **ppPipeline, ID3D12RootSignature **ppRoot ) noexcept;

protected:
private:
	static constexpr uint32_t s_nPipelineCapacity = 64;
	sirD3D12PipelineSlot_t m_szSlots[s_nPipelineCapacity] = {};
	SIRD3D12Device *m_pNative = nullptr;
	ID3D12Device *m_pDevice = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary> m_pPipelineLibrary;
	void *m_pLibraryBlob = nullptr;
	sirD3D12PipelineCacheStats_t m_CacheStats = {};
	bool m_bPipelineCreated = false;
	uint32_t m_nDeviceGeneration = 0;

	sirD3D12PipelineSlot_t *findSlot( sirGraphicsHandle_t hPipeline ) noexcept;
	void releaseSlot( uint32_t iSlot ) noexcept;
	HRESULT createRootSignature( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature **ppRoot ) noexcept;
	HRESULT createGraphicsState( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature *pRoot,
	                             ID3D12PipelineState **ppPipeline, const wchar_t *pszCacheKey ) noexcept;
	HRESULT createComputeState( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature *pRoot,
	                            ID3D12PipelineState **ppPipeline, const wchar_t *pszCacheKey ) noexcept;

};

#endif // __SIRENGINE_SIRD3D12PIPELINES_H__
