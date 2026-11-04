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

#ifndef __SIRENGINE_SIRVULKANGRAPHICS_H__
#define __SIRENGINE_SIRVULKANGRAPHICS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

class SIRVulkanRenderingDevice;

typedef enum sirVulkanCommandState_e : uint8_t
{
	SIR_VULKAN_COMMAND_FREE = 0,
	SIR_VULKAN_COMMAND_RECORDING,
	SIR_VULKAN_COMMAND_CLOSED,
	SIR_VULKAN_COMMAND_SUBMITTED
} sirVulkanCommandState_t;

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

// Commands are borrowed from a fixed device-owned pool. The first native
// integration slice supports one recorded buffer copy per submission.
class SIRVulkanCommandList final : public SIRGraphicsCommandList
{
private:
	friend class SIRVulkanRenderingDevice;

	SIRVulkanRenderingDevice *m_pOwner = nullptr;
	sirVulkanCommandState_t m_eState = SIR_VULKAN_COMMAND_FREE;
	sirGraphicsHandle_t m_hSource = {};
	sirGraphicsHandle_t m_hDestination = {};
	uint64_t m_nSourceOffset = 0;
	uint64_t m_nDestinationOffset = 0;
	uint64_t m_nBytes = 0;
	uint64_t m_nLastCompletion = 0;
	bool m_bHasCopy = false;

	void prepare( SIRVulkanRenderingDevice *pOwner ) noexcept;

public:
	sirGraphicsQueue_t getQueue( void ) const noexcept override
	{
		return SIR_GRAPHICS_QUEUE_DIRECT;
	}
	sirGraphicsResult_t reset( void ) noexcept override;
	sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource, uint64_t nSrcOffset,
	                                uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t close( void ) noexcept override;

	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *, uint32_t nCount ) noexcept override
	{
		return nCount == 0 ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t & ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t & ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t endRenderPass( void ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindBindingSet( uint32_t, sirGraphicsHandle_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindVertexBuffer( uint32_t, sirGraphicsHandle_t, uint64_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t, uint64_t, bool ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t &, const sirGraphicsRect_t & ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t draw( uint32_t, uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t drawIndexed( uint32_t, uint32_t, uint32_t, int32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t dispatch( uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
};

class SIRVulkanRenderingDevice final : public SIRRenderingDevice
{
private:
	friend class SIRVulkanCommandList;

	static constexpr uint32_t SIR_VULKAN_BUFFER_CAPACITY = 64;
	SIRVulkanDevice m_Native;
	sirVulkanGraphicsBufferSlot_t m_szBuffers[SIR_VULKAN_BUFFER_CAPACITY] = {};
	SIRVulkanCommandList m_szCommands[SIR_VULKAN_TRANSFER_SLOT_COUNT];
	uint64_t m_nNativeSerial = 0;
	uint64_t m_nLastCompletion = 0;
	uint32_t m_nDeviceGeneration = 0;
	bool m_bReady = false;

	sirVulkanGraphicsBufferSlot_t *resolveBuffer( sirGraphicsHandle_t handle ) noexcept;
	const sirVulkanGraphicsBufferSlot_t *resolveBuffer( sirGraphicsHandle_t handle ) const noexcept;
	void collectRetired( void ) noexcept;
	sirGraphicsResult_t initialize( void ) noexcept;

	friend class SIRVulkanDriver;

public:
	SIRVulkanRenderingDevice( void ) noexcept;
	~SIRVulkanRenderingDevice( void ) noexcept override;

	SIRVulkanRenderingDevice( const SIRVulkanRenderingDevice & ) = delete;
	SIRVulkanRenderingDevice &operator=( const SIRVulkanRenderingDevice & ) = delete;

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_VULKAN;
	}
	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return m_nDeviceGeneration;
	}
	sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept override;
	sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept override;
	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept override;
	sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept override;
	sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept override;
	sirGraphicsResult_t acquireCommands( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept override;
	sirGraphicsResult_t submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t &submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override;
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t &completion, bool *pCompleted ) noexcept override;
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t &completion, uint64_t nTimeoutNs ) noexcept override;
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t &completion ) noexcept override;
	sirGraphicsResult_t shutdown( void ) noexcept override;

	// Unimplemented features fail explicitly rather than returning fake handles.
	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t &, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createView( const sirGraphicsViewDesc_t &, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t &, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t &, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createBindingSet( sirGraphicsHandle_t, uint32_t, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t updateBindings( sirGraphicsHandle_t, const sirGraphicsDescriptorWrite_t *, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createSurface( const sirGraphicsNativeSurface_t &, const sirGraphicsPresentDesc_t &,
	                                   sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( pHandle != nullptr ) {
			*pHandle = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t resizeSurface( sirGraphicsHandle_t, const sirGraphicsNativeSurface_t &,
	                                   const sirGraphicsPresentDesc_t & ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t acquireSurfaceImage( sirGraphicsHandle_t, sirGraphicsSurfaceImage_t *pImage ) noexcept override
	{
		if ( pImage != nullptr ) {
			*pImage = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t present( const sirGraphicsSurfaceImage_t &, const sirGraphicsCompletion_t & ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
};

class SIRVulkanDriver final : public SIRGraphicsDriver
{
private:
	SIRVulkanRenderingDevice m_Device;

public:
	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_VULKAN;
	}
	sirGraphicsResult_t getAdapterCount( uint32_t *pCount ) const noexcept override;
	sirGraphicsResult_t queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override;
	sirGraphicsResult_t createDevice( const sirGraphicsDeviceCreateDesc_t &desc, SIRRenderingDevice **ppDevice ) noexcept override;
	sirGraphicsResult_t destroyDevice( SIRRenderingDevice *pDevice ) noexcept override;
};

#endif // __SIRENGINE_SIRVULKANGRAPHICS_H__
