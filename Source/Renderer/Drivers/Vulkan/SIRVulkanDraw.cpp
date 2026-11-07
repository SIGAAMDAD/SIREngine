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
#include <Renderer/Drivers/Vulkan/SIRVulkanGraphics.h>
#include <Renderer/SIRGraphicsValidation.h>

sirGraphicsResult_t SIRVulkanCommandList::beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr || m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pass.nColorCount > 8 || ( pass.nColorCount != 0 && pass.pColors == nullptr ) ||
	     ( pass.nColorCount == 0 && pass.hDepthView.nGeneration == 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nWidth = 0;
	uint32_t nHeight = 0;
	for ( uint32_t i = 0; i <= pass.nColorCount; ++i ) {
		const bool bDepth = i == pass.nColorCount;
		const sirGraphicsHandle_t hView = bDepth ? pass.hDepthView : pass.pColors[i].hView;
		if ( bDepth && hView.nGeneration == 0 ) {
			continue;
		}
		if ( !sirGraphicsIsHandleValid( hView, SIR_GRAPHICS_HANDLE_VIEW, m_pOwner->m_nDeviceGeneration ) ||
		     hView.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_VIEW_CAPACITY ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const sirVulkanGraphicsViewSlot_t& view = m_pOwner->m_szViews[hView.nSlot];
		if ( !view.bActive || view.nGeneration != hView.nGeneration || view.nMipLevels != 1 || view.nLayers != 1 ||
		     view.nImageSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_TEXTURE_CAPACITY ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const sirVulkanGraphicsImageSlot_t& image = m_pOwner->m_szImages[view.nImageSlot];
		if ( !image.bActive || image.nWidth == 0 || image.nHeight == 0 ||
		     ( nWidth != 0 && ( nWidth != image.nWidth || nHeight != image.nHeight ) ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const VkImageAspectFlags nAspects = sirVulkanImageAspectMask( image.native.eFormat );
		if ( bDepth ? ( ( nAspects & VK_IMAGE_ASPECT_DEPTH_BIT ) == 0 ||
		                ( image.native.nUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) == 0 )
		            : ( nAspects != VK_IMAGE_ASPECT_COLOR_BIT || ( image.native.nUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) == 0 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nWidth = image.nWidth;
		nHeight = image.nHeight;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_RENDER_BEGIN;
	op.nColorCount = pass.nColorCount;
	for ( uint32_t i = 0; i < pass.nColorCount; ++i ) {
		op.szColors[i] = pass.pColors[i];
	}
	op.hDepthView = pass.hDepthView;
	op.fClearDepth = pass.fClearDepth;
	op.nClearStencil = pass.nClearStencil;
	op.bClearDepth = pass.bClearDepth;
	op.bStoreDepth = pass.bStoreDepth;
	m_bInRenderPass = true;
	m_hBoundPipeline = {};
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::endRenderPass( void ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || !m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_RENDER_END;
	m_bInRenderPass = false;
	m_hBoundPipeline = {};
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffset,
                                                            uint32_t nStride ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr || !m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const sirVulkanGraphicsBufferSlot_t *pBuffer = m_pOwner->resolveBuffer( hBuffer );
	if ( pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nSlot >= 32 || nStride == 0 || nOffset >= pBuffer->native.nSize ||
	     ( pBuffer->native.nUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ) == 0 || pBuffer->pMapped != nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_VERTEX_BUFFER;
	op.hSource = hBuffer;
	op.nSourceOffset = nOffset;
	op.nBufferSlot = nSlot;
	op.nBufferStride = nStride;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, bool bIndex32 ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr || !m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const sirVulkanGraphicsBufferSlot_t *pBuffer = m_pOwner->resolveBuffer( hBuffer );
	if ( pBuffer == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint32_t nIndexBytes = bIndex32 ? 4 : 2;
	if ( nOffset >= pBuffer->native.nSize || nOffset % nIndexBytes != 0 ||
	     ( pBuffer->native.nUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) == 0 || pBuffer->pMapped != nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_INDEX_BUFFER;
	op.hSource = hBuffer;
	op.nSourceOffset = nOffset;
	op.bIndex32 = bIndex32;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || !m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( !( viewport.fWidth > 0.0f && viewport.fHeight > 0.0f && viewport.fMinimumDepth >= 0.0f && viewport.fMaximumDepth <= 1.0f &&
	        viewport.fMinimumDepth <= viewport.fMaximumDepth ) ||
	     scissor.nLeft < 0 || scissor.nTop < 0 || scissor.nRight <= scissor.nLeft || scissor.nBottom <= scissor.nTop ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_VIEWPORT;
	op.viewport = viewport;
	op.scissor = scissor;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex,
                                                uint32_t nFirstInstance ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr || !m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( nVertices == 0 || nInstances == 0 || m_hBoundPipeline.nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_DRAW;
	op.nDrawCount = nVertices;
	op.nInstanceCount = nInstances;
	op.nFirstElement = nFirstVertex;
	op.nFirstInstance = nFirstInstance;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
                                                       uint32_t nFirstInstance ) noexcept
{
	const sirGraphicsResult_t eValidation = draw( nIndices, nInstances, nFirstIndex, nFirstInstance );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount - 1];
	op.eKind = SIR_VULKAN_GRAPHICS_DRAW_INDEXED;
	op.nVertexOffset = nVertexOffset;
	return SIR_GRAPHICS_SUCCESS;
}
