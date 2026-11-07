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

static VkDescriptorType sirVulkanBindingDescriptorType( sirGraphicsBindingType_t eType ) noexcept
{
	switch ( eType ) {
	case SIR_GRAPHICS_BINDING_CONSTANT_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case SIR_GRAPHICS_BINDING_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case SIR_GRAPHICS_BINDING_STORAGE_TEXTURE:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case SIR_GRAPHICS_BINDING_SAMPLER:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	default:
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

sirGraphicsResult_t SIRVulkanRenderingDevice::createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex,
                                                                sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	*pHandle = {};
	if ( !m_bReady )
		return SIR_GRAPHICS_BUSY;
	if ( !sirGraphicsIsHandleValid( hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, m_nDeviceGeneration ) ||
	     hPipeline.nSlot >= SIR_VULKAN_PIPELINE_CAPACITY )
		return SIR_GRAPHICS_INVALID_HANDLE;
	const sirVulkanGraphicsPipelineSlot_t& pipeline = m_szPipelines[hPipeline.nSlot];
	if ( !pipeline.bActive || !pipeline.bGraphics || pipeline.nGeneration != hPipeline.nGeneration ||
	     nSetIndex >= pipeline.native.nSetLayoutCount )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	collectRetired();
	for ( uint32_t i = 0; i < SIR_VULKAN_BINDING_SET_CAPACITY; ++i ) {
		sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[i];
		if ( set.bActive || set.bRetiring || set.nGeneration == 0 )
			continue;
		const sirVulkanResult_t eCreate = m_Native.createDescriptorSet( &pipeline.native, nSetIndex, &set.native );
		if ( eCreate != SIR_VULKAN_SUCCESS )
			return eCreate == SIR_VULKAN_ALLOCATION_FAILED ? SIR_GRAPHICS_OUT_OF_MEMORY
			       : eCreate == SIR_VULKAN_API_UNSUPPORTED ? SIR_GRAPHICS_UNSUPPORTED
			       : eCreate == SIR_VULKAN_BUSY            ? SIR_GRAPHICS_BUSY
			                                               : SIR_GRAPHICS_DRIVER_ERROR;
		set.hPipeline = hPipeline;
		set.nSetIndex = nSetIndex;
		set.nReferenceCount = 0;
		set.nRetireValue = 0;
		set.bActive = true;
		*pHandle = { i, set.nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::updateBindings( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
                                                              uint32_t nCount ) noexcept
{
	if ( !m_bReady )
		return SIR_GRAPHICS_BUSY;
	if ( !sirGraphicsIsHandleValid( hSet, SIR_GRAPHICS_HANDLE_BINDING_SET, m_nDeviceGeneration ) ||
	     hSet.nSlot >= SIR_VULKAN_BINDING_SET_CAPACITY )
		return SIR_GRAPHICS_INVALID_HANDLE;
	sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[hSet.nSlot];
	if ( !set.bActive || set.nGeneration != hSet.nGeneration || pWrites == nullptr || nCount == 0 || nCount > 32 )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	if ( !sirGraphicsIsHandleValid( set.hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, m_nDeviceGeneration ) ||
	     set.hPipeline.nSlot >= SIR_VULKAN_PIPELINE_CAPACITY )
		return SIR_GRAPHICS_INVALID_HANDLE;
	const sirVulkanGraphicsPipelineSlot_t& pipeline = m_szPipelines[set.hPipeline.nSlot];
	if ( !pipeline.bActive || pipeline.nGeneration != set.hPipeline.nGeneration )
		return SIR_GRAPHICS_INVALID_HANDLE;

	VkWriteDescriptorSet nativeWrites[32] = {};
	VkDescriptorBufferInfo buffers[32] = {};
	VkDescriptorImageInfo images[32] = {};
	uint32_t szReferenceSlots[32] = {};
	uint32_t nNewReferences = set.nReferenceCount;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const sirGraphicsDescriptorWrite_t& write = pWrites[i];
		const sirGraphicsBindingDesc_t *pBinding = nullptr;
		for ( uint32_t j = 0; j < pipeline.native.nBindingCount; ++j ) {
			const sirGraphicsBindingDesc_t& binding = pipeline.native.szBindings[j];
			if ( binding.nSpace == set.nSetIndex && binding.nBinding == write.nBinding ) {
				pBinding = &binding;
				break;
			}
		}
		if ( pBinding == nullptr || write.nArrayIndex >= pBinding->nArrayCount )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pWrites[j].nBinding == write.nBinding && pWrites[j].nArrayIndex == write.nArrayIndex )
				return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		uint32_t iReference = 0;
		while ( iReference < set.nReferenceCount &&
		        ( set.szReferenceBindings[iReference] != write.nBinding || set.szReferenceIndices[iReference] != write.nArrayIndex ) ) {
			++iReference;
		}
		if ( iReference == set.nReferenceCount )
			iReference = nNewReferences++;
		if ( iReference >= 64 )
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		szReferenceSlots[i] = iReference;
		VkWriteDescriptorSet& native = nativeWrites[i];
		native.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		native.dstSet = set.native.hSet;
		native.dstBinding = write.nBinding;
		native.dstArrayElement = write.nArrayIndex;
		native.descriptorCount = 1;
		native.descriptorType = sirVulkanBindingDescriptorType( pBinding->eType );
		if ( pBinding->eType == SIR_GRAPHICS_BINDING_CONSTANT_BUFFER || pBinding->eType == SIR_GRAPHICS_BINDING_STORAGE_BUFFER ) {
			const sirVulkanGraphicsBufferSlot_t *pBuffer = resolveBuffer( write.hResource );
			if ( pBuffer == nullptr || pBuffer->pMapped != nullptr )
				return SIR_GRAPHICS_INVALID_HANDLE;
			const VkBufferUsageFlags nRequired = pBinding->eType == SIR_GRAPHICS_BINDING_CONSTANT_BUFFER
			                                         ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			                                         : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			if ( ( pBuffer->native.nUsage & nRequired ) == 0 || write.nOffsetBytes >= pBuffer->native.nSize ||
			     ( write.nRangeBytes != 0 && write.nRangeBytes > pBuffer->native.nSize - write.nOffsetBytes ) )
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			buffers[i].buffer = pBuffer->native.hBuffer;
			buffers[i].offset = write.nOffsetBytes;
			buffers[i].range = write.nRangeBytes == 0 ? VK_WHOLE_SIZE : write.nRangeBytes;
			native.pBufferInfo = &buffers[i];
		} else if ( pBinding->eType == SIR_GRAPHICS_BINDING_SAMPLER ) {
			const sirGraphicsHandle_t hSampler = write.hResource;
			if ( !sirGraphicsIsHandleValid( hSampler, SIR_GRAPHICS_HANDLE_SAMPLER, m_nDeviceGeneration ) ||
			     hSampler.nSlot >= SIR_VULKAN_SAMPLER_CAPACITY )
				return SIR_GRAPHICS_INVALID_HANDLE;
			const sirVulkanGraphicsSamplerSlot_t& sampler = m_szSamplers[hSampler.nSlot];
			if ( !sampler.bActive || sampler.nGeneration != hSampler.nGeneration || write.nOffsetBytes != 0 || write.nRangeBytes != 0 )
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			images[i].sampler = sampler.native.hSampler;
			native.pImageInfo = &images[i];
		} else {
			const sirGraphicsHandle_t hView = write.hResource;
			if ( !sirGraphicsIsHandleValid( hView, SIR_GRAPHICS_HANDLE_VIEW, m_nDeviceGeneration ) ||
			     hView.nSlot >= SIR_VULKAN_VIEW_CAPACITY )
				return SIR_GRAPHICS_INVALID_HANDLE;
			const sirVulkanGraphicsViewSlot_t& view = m_szViews[hView.nSlot];
			if ( !view.bActive || view.nGeneration != hView.nGeneration || write.nOffsetBytes != 0 || write.nRangeBytes != 0 )
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			const sirVulkanGraphicsImageSlot_t& image = m_szImages[view.nImageSlot];
			const VkImageUsageFlags nRequired =
			    pBinding->eType == SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE ? VK_IMAGE_USAGE_SAMPLED_BIT : VK_IMAGE_USAGE_STORAGE_BIT;
			if ( !image.bActive || ( image.native.nUsage & nRequired ) == 0 )
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			images[i].imageView = view.native.hView;
			images[i].imageLayout = pBinding->eType == SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			                                                                                : VK_IMAGE_LAYOUT_GENERAL;
			native.pImageInfo = &images[i];
		}
	}
	const sirVulkanResult_t eUpdate = m_Native.updateDescriptorSet( &set.native, nativeWrites, nCount );
	if ( eUpdate != SIR_VULKAN_SUCCESS )
		return eUpdate == SIR_VULKAN_BUSY              ? SIR_GRAPHICS_BUSY
		       : eUpdate == SIR_VULKAN_API_UNSUPPORTED ? SIR_GRAPHICS_UNSUPPORTED
		                                               : SIR_GRAPHICS_DRIVER_ERROR;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const uint32_t iReference = szReferenceSlots[i];
		set.szReferences[iReference] = pWrites[i].hResource;
		set.szReferenceBindings[iReference] = pWrites[i].nBinding;
		set.szReferenceIndices[iReference] = pWrites[i].nArrayIndex;
	}
	set.nReferenceCount = nNewReferences;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr || !m_bInRenderPass )
		return SIR_GRAPHICS_BUSY;
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY )
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	if ( m_hBoundPipeline.nGeneration == 0 ||
	     !sirGraphicsIsHandleValid( hBindingSet, SIR_GRAPHICS_HANDLE_BINDING_SET, m_pOwner->m_nDeviceGeneration ) ||
	     hBindingSet.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_BINDING_SET_CAPACITY )
		return SIR_GRAPHICS_INVALID_HANDLE;
	const sirVulkanGraphicsBindingSlot_t& set = m_pOwner->m_szBindingSets[hBindingSet.nSlot];
	if ( !set.bActive || set.nGeneration != hBindingSet.nGeneration || set.nSetIndex != nSetIndex ||
	     set.hPipeline.nSlot != m_hBoundPipeline.nSlot || set.hPipeline.nGeneration != m_hBoundPipeline.nGeneration )
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	sirVulkanGraphicsOperation_t& op = m_szOperations[m_nOperationCount++];
	op = {};
	op.eKind = SIR_VULKAN_GRAPHICS_BIND_DESCRIPTOR_SET;
	op.hBindingSet = hBindingSet;
	op.hPipeline = m_hBoundPipeline;
	op.nSetIndex = nSetIndex;
	return SIR_GRAPHICS_SUCCESS;
}
