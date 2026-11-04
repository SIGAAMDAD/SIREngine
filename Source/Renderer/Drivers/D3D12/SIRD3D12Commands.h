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

#ifndef __SIRENGINE_SIRD3D12COMMANDS_H__
#define __SIRENGINE_SIRD3D12COMMANDS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D12/SIRD3D12Pipelines.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Resources.h>

// Each command context owns a fixed native allocator/list pair and holds
// references to recorded resources until the GPU submission fence completes.
// Single-caller recording and submission; no implicit CPU fence wait.
class SIRD3D12CommandList final : public SIRGraphicsCommandList
{
	friend class SIRD3D12Commands;

private:
	static constexpr uint32_t s_nReferenceCapacity = 128;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommands;
	SIRD3D12Device *m_pNative = nullptr;
	SIRD3D12Resources *m_pResources = nullptr;
	SIRD3D12Pipelines *m_pPipelines = nullptr;
	sirGraphicsHandle_t m_szReferences[s_nReferenceCapacity] = {};
	sirGraphicsQueue_t m_eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	sirGraphicsPipelineType_t m_ePipelineType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	sirGraphicsTopology_t m_eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	uint32_t m_nReferenceCount = 0;
	uint32_t m_nBindingCount = 0;
	uint64_t m_nSubmittedFence = 0;
	bool m_bRecording = false;
	bool m_bClosed = false;
	bool m_bSubmitted = false;
	bool m_bRenderPass = false;
	bool m_bPipelineBound = false;

	sirGraphicsResult_t pin( sirGraphicsHandle_t hHandle ) noexcept;
	void releaseReferences( void ) noexcept;
	bool completeSubmission( void ) noexcept;
	bool validRecording( void ) const noexcept;
	ID3D12CommandList *getNativeList( void ) const noexcept;
	void setSubmitted( uint64_t nFence ) noexcept;

public:
	SIRD3D12CommandList( void ) noexcept = default;
	~SIRD3D12CommandList( void ) noexcept override = default;
	SIRD3D12CommandList( const SIRD3D12CommandList & ) = delete;
	SIRD3D12CommandList &operator=( const SIRD3D12CommandList & ) = delete;

	bool initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines,
	                 sirGraphicsQueue_t eQueue ) noexcept;
	void shutdown( bool bGpuComplete ) noexcept;
	sirGraphicsQueue_t getQueue( void ) const noexcept override;
	sirGraphicsResult_t reset( void ) noexcept override;
	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept override;
	sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource, uint64_t nSrcOffset,
	                                uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t &copy ) noexcept override;
	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t &pass ) noexcept override;
	sirGraphicsResult_t endRenderPass( void ) noexcept override;
	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept override;
	sirGraphicsResult_t bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept override;
	sirGraphicsResult_t bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes,
	                                      uint32_t nStrideBytes ) noexcept override;
	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, bool bIndex32 ) noexcept override;
	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t &viewport, const sirGraphicsRect_t &scissor ) noexcept override;
	sirGraphicsResult_t draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept override;
	sirGraphicsResult_t drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
	                                 uint32_t nFirstInstance ) noexcept override;
	sirGraphicsResult_t dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept override;
	sirGraphicsResult_t close( void ) noexcept override;
};

class SIRD3D12Commands
{
private:
	static constexpr uint32_t s_nListsPerQueue = 4;
	SIRD3D12CommandList m_szLists[SIR_GRAPHICS_QUEUE_COUNT][s_nListsPerQueue];
	SIRD3D12Device *m_pNative = nullptr;

public:
	SIRD3D12Commands( void ) noexcept = default;
	~SIRD3D12Commands( void ) noexcept;
	SIRD3D12Commands( const SIRD3D12Commands & ) = delete;
	SIRD3D12Commands &operator=( const SIRD3D12Commands & ) = delete;
	bool initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines ) noexcept;
	void shutdown( bool bGpuComplete ) noexcept;
	void collectCompleted( void ) noexcept;
	sirGraphicsResult_t acquire( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept;
	sirGraphicsResult_t submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t &submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept;
};

#endif // __SIRENGINE_SIRD3D12COMMANDS_H__
