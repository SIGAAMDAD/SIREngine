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

#ifndef __SIR_VULKANGRAPHICS_H__
#define __SIR_VULKANGRAPHICS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

class SIRVulkanRenderingDevice;

enum class SIRVulkanCommandState : uint8_t {
	SIR_VULKAN_COMMAND_FREE = 0,
	SIR_VULKAN_COMMAND_RECORDING,
	SIR_VULKAN_COMMAND_CLOSED,
	SIR_VULKAN_COMMAND_SUBMITTED
};

typedef struct sirVulkanGraphicsBufferSlot_s
{
	sirVulkanBuffer_t native;
	uint32_t nGeneration;
	uint64_t nRetireValue;
	uint64_t nMappedOffset;
	uint64_t nMappedBytes;
	void *pMapped;
	bool bActive;
	bool bRetiring;
} sirVulkanGraphicsBufferSlot_t;

typedef struct sirVulkanGraphicsImageSlot_s
{
	sirVulkanImage_t native;
	uint32_t nGeneration;
	uint32_t nViewCount;
	uint32_t nWidth;
	uint32_t nHeight;
	uint64_t nRetireValue;
	bool bActive;
	bool bRetiring;
} sirVulkanGraphicsImageSlot_t;

typedef struct sirVulkanGraphicsViewSlot_s
{
	sirVulkanImageView_t native;
	uint32_t nGeneration;
	uint32_t nImageSlot;
	uint32_t nBaseMip;
	uint32_t nBaseLayer;
	uint32_t nMipLevels;
	uint32_t nLayers;
	uint64_t nLastGpuUse;
	uint64_t nRetireValue;
	bool bActive;
	bool bRetiring;
} sirVulkanGraphicsViewSlot_t;

typedef struct sirVulkanGraphicsPipelineSlot_s
{
	sirVulkanComputePipeline_t native;
	bool bGraphics;
	uint32_t nGeneration;
	uint64_t nRetireValue;
	bool bActive;
	bool bRetiring;
} sirVulkanGraphicsPipelineSlot_t;

typedef struct sirVulkanGraphicsBindingSlot_s
{
	sirVulkanDescriptorSet_t native;
	sirGraphicsHandle_t hPipeline;
	uint32_t nGeneration;
	uint32_t nSetIndex;
	uint32_t nReferenceCount;
	sirGraphicsHandle_t szReferences[64];
	uint32_t szReferenceBindings[64];
	uint32_t szReferenceIndices[64];
	uint64_t nRetireValue;
	bool bActive;
	bool bRetiring;
} sirVulkanGraphicsBindingSlot_t;

typedef struct sirVulkanGraphicsSamplerSlot_s
{
	sirVulkanSampler_t native;
	uint32_t nGeneration;
	uint64_t nLastGpuUse;
	uint64_t nRetireValue;
	bool bActive;
	bool bRetiring;
} sirVulkanGraphicsSamplerSlot_t;

// Translates a shared image barrier into one explicitly validated native
// subresource transition; callable without initializing a Vulkan device.
SIRGraphicsResult sirVulkanPlanImageBarrier( const sirGraphicsBarrier_t& barrier, const sirVulkanImage_t *pImage,
                                               sirVulkanImageBarrier_t *pNative ) noexcept;
SIRGraphicsResult sirVulkanPlanBufferBarrier( const sirGraphicsBarrier_t& barrier, const sirVulkanBuffer_t *pBuffer,
                                                sirVulkanBufferBarrier_t *pNative ) noexcept;
SIRGraphicsResult sirVulkanDescribeAdapter( uint32_t nAdapterIndex, const sirVulkanAdapterInfo_t& native,
                                              sirGraphicsAdapterInfo_t *pInfo ) noexcept;
SIRGraphicsResult sirVulkanValidateBatchCounts( const uint32_t *pCounts, uint32_t nListCount, uint32_t *pTotal ) noexcept;
// GPU-independent validation uses the same admission semantics as native creation.
SIRGraphicsResult sirVulkanDescribeCapabilities( const sirVulkanAdapterInfo_t& native, uint32_t nGeneration,
                                                   sirGraphicsCapabilities_t *pCapabilities ) noexcept;
SIRGraphicsResult sirVulkanValidateDeviceAdmission( const sirGraphicsDeviceCreateDesc_t& request,
                                                      const sirGraphicsCapabilities_t& available ) noexcept;

// Each command list records up to eight ordered operations without CPU
// allocation. Native submission validates the whole stream atomically.
// Closing an empty command list releases its slot.
enum class SIRVulkanGraphicsOpKind : uint8_t {
	SIR_VULKAN_GRAPHICS_BUFFER_COPY = 0,
	SIR_VULKAN_GRAPHICS_TEXTURE_COPY,
	SIR_VULKAN_GRAPHICS_IMAGE_BARRIER,
	SIR_VULKAN_GRAPHICS_BUFFER_BARRIER,
	SIR_VULKAN_GRAPHICS_COMPUTE_DISPATCH,
	SIR_VULKAN_GRAPHICS_RENDER_BEGIN,
	SIR_VULKAN_GRAPHICS_RENDER_END,
	SIR_VULKAN_GRAPHICS_BIND_PIPELINE,
	SIR_VULKAN_GRAPHICS_VERTEX_BUFFER,
	SIR_VULKAN_GRAPHICS_INDEX_BUFFER,
	SIR_VULKAN_GRAPHICS_VIEWPORT,
	SIR_VULKAN_GRAPHICS_DRAW,
	SIR_VULKAN_GRAPHICS_DRAW_INDEXED,
	SIR_VULKAN_GRAPHICS_BIND_DESCRIPTOR_SET
};

typedef struct sirVulkanGraphicsOperation_s
{
	SIRVulkanGraphicsOpKind eKind;
	sirGraphicsHandle_t hSource;
	sirGraphicsHandle_t hDestination;
	uint64_t nSourceOffset;
	uint64_t nDestinationOffset;
	uint64_t nBytes;
	sirGraphicsBufferTextureCopy_t textureCopy;
	sirGraphicsBarrier_t barrier;
	sirGraphicsHandle_t hPipeline;
	sirGraphicsHandle_t hBindingSet;
	uint32_t nSetIndex;
	uint32_t nGroupCountX;
	uint32_t nGroupCountY;
	uint32_t nGroupCountZ;
	uint32_t nColorCount;
	sirGraphicsColorAttachment_t szColors[8];
	sirGraphicsHandle_t hDepthView;
	float fClearDepth;
	uint32_t nClearStencil;
	bool bClearDepth;
	bool bStoreDepth;
	sirGraphicsViewport_t viewport;
	sirGraphicsRect_t scissor;
	uint32_t nBufferSlot;
	uint32_t nBufferStride;
	bool bIndex32;
	uint32_t nDrawCount;
	uint32_t nInstanceCount;
	uint32_t nFirstElement;
	int32_t nVertexOffset;
	uint32_t nFirstInstance;
} sirVulkanGraphicsOperation_t;

class SIRVulkanCommandList final : public SIRGraphicsCommandList
{
private:
	friend class SIRVulkanRenderingDevice;

	SIRVulkanRenderingDevice *m_pOwner = nullptr;
	SIRVulkanCommandState m_eState = SIRVulkanCommandState::SIR_VULKAN_COMMAND_FREE;
	sirVulkanGraphicsOperation_t m_szOperations[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	uint32_t m_nOperationCount = 0;
	uint64_t m_nLastCompletion = 0;
	sirGraphicsHandle_t m_hBoundPipeline {};
	bool m_bInRenderPass = false;

	void prepare( SIRVulkanRenderingDevice *pOwner ) noexcept;

public:
	SIRGraphicsQueue getQueue( void ) const noexcept override
	{
		return SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT;
	}
	SIRGraphicsResult reset( void ) noexcept override;
	SIRGraphicsResult copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource, uint64_t nSrcOffset,
	                                uint64_t nBytes ) noexcept override;
	SIRGraphicsResult close( void ) noexcept override;

	SIRGraphicsResult barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept override;
	SIRGraphicsResult copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept override;
	SIRGraphicsResult beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept override;
	SIRGraphicsResult endRenderPass( void ) noexcept override;
	SIRGraphicsResult bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept override;
	SIRGraphicsResult bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept override;
	SIRGraphicsResult bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffset,
	                                      uint32_t nStride ) noexcept override;
	SIRGraphicsResult bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, bool bIndex32 ) noexcept override;
	SIRGraphicsResult setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept override;
	SIRGraphicsResult draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept override;
	SIRGraphicsResult drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
	                                 uint32_t nFirstInstance ) noexcept override;
	SIRGraphicsResult dispatch( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept override;
};

class SIRVulkanRenderingDevice final : public SIRRenderingDevice
{
private:
	friend class SIRVulkanCommandList;

	static constexpr uint32_t SIR_VULKAN_BUFFER_CAPACITY = 64;
	static constexpr uint32_t SIR_VULKAN_TEXTURE_CAPACITY = 64;
	static constexpr uint32_t SIR_VULKAN_VIEW_CAPACITY = 128;
	static constexpr uint32_t SIR_VULKAN_SAMPLER_CAPACITY = 64;
	static constexpr uint32_t SIR_VULKAN_PIPELINE_CAPACITY = 16;
	static constexpr uint32_t SIR_VULKAN_BINDING_SET_CAPACITY = 64;
	SIRVulkanDevice m_Native;
	sirVulkanGraphicsBufferSlot_t m_szBuffers[SIR_VULKAN_BUFFER_CAPACITY] = {};
	sirVulkanGraphicsImageSlot_t m_szImages[SIR_VULKAN_TEXTURE_CAPACITY] = {};
	sirVulkanGraphicsViewSlot_t m_szViews[SIR_VULKAN_VIEW_CAPACITY] = {};
	sirVulkanGraphicsSamplerSlot_t m_szSamplers[SIR_VULKAN_SAMPLER_CAPACITY] = {};
	sirVulkanGraphicsPipelineSlot_t m_szPipelines[SIR_VULKAN_PIPELINE_CAPACITY] = {};
	sirVulkanGraphicsBindingSlot_t m_szBindingSets[SIR_VULKAN_BINDING_SET_CAPACITY] = {};
	SIRVulkanCommandList m_szCommands[SIR_VULKAN_TRANSFER_SLOT_COUNT];
	uint64_t m_nNativeSerial = 0;
	uint64_t m_nLastCompletion = 0;
	uint32_t m_nDeviceGeneration = 0;
	bool m_bReady = false;

	sirVulkanGraphicsBufferSlot_t *resolveBuffer( sirGraphicsHandle_t handle ) noexcept;
	const sirVulkanGraphicsBufferSlot_t *resolveBuffer( sirGraphicsHandle_t handle ) const noexcept;
	void collectRetired( void ) noexcept;
	void collectImageResources( void ) noexcept;
	SIRGraphicsResult retireImageResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept;
	SIRGraphicsResult initialize( const sirVulkanPoolBudget_t *pBudget, uint32_t nAdapterIndex ) noexcept;

	friend class SIRVulkanDriver;

public:
	SIRVulkanRenderingDevice( void ) noexcept;
	~SIRVulkanRenderingDevice( void ) noexcept override;

	SIRVulkanRenderingDevice( const SIRVulkanRenderingDevice& ) = delete;
	SIRVulkanRenderingDevice& operator=( const SIRVulkanRenderingDevice& ) = delete;

	SIRGraphicsBackend getBackend( void ) const noexcept override
	{
		return SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_VULKAN;
	}
	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return m_nDeviceGeneration;
	}
	SIRGraphicsResult queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept override;
	SIRGraphicsResult queryPipelineModelCaps( SIRGraphicsPipelineModel eModel,
	                                            sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept override;
	SIRGraphicsResult queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept override;
	SIRGraphicsResult createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept override;
	SIRGraphicsResult unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept override;
	SIRGraphicsResult flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept override;
	SIRGraphicsResult invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept override;
	SIRGraphicsResult acquireCommands( SIRGraphicsQueue eQueue, SIRGraphicsCommandList **ppCommands ) noexcept override;
	SIRGraphicsResult submit( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override;
	SIRGraphicsResult poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept override;
	SIRGraphicsResult wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNs ) noexcept override;
	SIRGraphicsResult retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept override;
	SIRGraphicsResult shutdown( void ) noexcept override;

	// Unimplemented features fail explicitly rather than returning fake handles.
	SIRGraphicsResult createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createSampler( const sirGraphicsSamplerDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
	                                      sirGraphicsHandle_t *pHandle ) noexcept override;
	SIRGraphicsResult updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
	                                    uint32_t nCount ) noexcept override;
	SIRGraphicsResult createSurface( const sirGraphicsNativeSurface_t&, const sirGraphicsPresentDesc_t&,
	                                   sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	SIRGraphicsResult resizeSurface( sirGraphicsHandle_t, const sirGraphicsNativeSurface_t&,
	                                   const sirGraphicsPresentDesc_t& ) noexcept override
	{
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	SIRGraphicsResult acquireSurfaceImage( sirGraphicsHandle_t, sirGraphicsSurfaceImage_t *pImage ) noexcept override
	{
		if ( pImage != nullptr ) {
			*pImage = {};
		}
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	SIRGraphicsResult present( const sirGraphicsSurfaceImage_t&, const sirGraphicsCompletion_t& ) noexcept override
	{
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
};

class SIRVulkanDriver final : public SIRGraphicsDriver
{
private:
	SIRVulkanRenderingDevice m_Device;

public:
	SIRGraphicsBackend getBackend( void ) const noexcept override
	{
		return SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_VULKAN;
	}
	SIRGraphicsResult getAdapterCount( uint32_t *pCount ) const noexcept override;
	SIRGraphicsResult queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override;
	SIRGraphicsResult createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept override;
	SIRGraphicsResult destroyDevice( SIRRenderingDevice *pDevice ) noexcept override;
};

#endif // __SIR_VULKANGRAPHICS_H__
