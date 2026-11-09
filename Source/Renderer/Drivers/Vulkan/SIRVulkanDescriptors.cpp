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
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

// Graphics descriptor sets are allocated outside the frame submission path.
// Each bounded allocation owns a Vulkan pool so its retirement has no effect
// on unrelated live sets. Device timeline completion fences all mutations.
sirVulkanResult_t SIRVulkanDevice::createDescriptorSet( const sirVulkanComputePipeline_t *pPipeline, uint32_t nSetIndex,
                                                        sirVulkanDescriptorSet_t *pSet ) noexcept
{
	if ( !m_bReady || m_bDeviceLost )
		return SIR_VULKAN_INVALID_STATE;
	if ( pSet == nullptr || pSet->hSet != VK_NULL_HANDLE || pSet->hPool != VK_NULL_HANDLE || pPipeline == nullptr ||
	     !pPipeline->bGraphics || pPipeline->hLayout == VK_NULL_HANDLE || pPipeline->nDeviceSerial != m_nDeviceSerial ||
	     nSetIndex >= pPipeline->nSetLayoutCount ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const PFN_vkCreateDescriptorPool pfnCreatePool =
	    ( PFN_vkCreateDescriptorPool )m_pfnGetDeviceProcAddr( m_Device, "vkCreateDescriptorPool" );
	const PFN_vkDestroyDescriptorPool pfnDestroyPool =
	    ( PFN_vkDestroyDescriptorPool )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyDescriptorPool" );
	const PFN_vkAllocateDescriptorSets pfnAllocateSets =
	    ( PFN_vkAllocateDescriptorSets )m_pfnGetDeviceProcAddr( m_Device, "vkAllocateDescriptorSets" );
	if ( pfnCreatePool == nullptr || pfnDestroyPool == nullptr || pfnAllocateSets == nullptr )
		return SIR_VULKAN_API_UNSUPPORTED;

	VkDescriptorPoolSize sizes[5] = {};
	uint32_t nSizes = 0;
	for ( uint32_t i = 0; i < pPipeline->nBindingCount; ++i ) {
		const sirGraphicsBindingDesc_t& binding = pPipeline->szBindings[i];
		if ( binding.nSpace != nSetIndex )
			continue;
		VkDescriptorType eType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		switch ( binding.eType ) {
		case SIR_GRAPHICS_BINDING_CONSTANT_BUFFER:
			eType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			break;
		case SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE:
			eType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			break;
		case SIR_GRAPHICS_BINDING_STORAGE_BUFFER:
			eType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			break;
		case SIR_GRAPHICS_BINDING_STORAGE_TEXTURE:
			eType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			break;
		case SIR_GRAPHICS_BINDING_SAMPLER:
			eType = VK_DESCRIPTOR_TYPE_SAMPLER;
			break;
		default:
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
		uint32_t iType = 0;
		while ( iType < nSizes && sizes[iType].type != eType )
			++iType;
		if ( iType == nSizes ) {
			if ( nSizes >= 5 )
				return SIR_VULKAN_INVALID_ARGUMENT;
			sizes[nSizes].type = eType;
			sizes[nSizes++].descriptorCount = 0;
		}
		sizes[iType].descriptorCount += binding.nArrayCount;
	}
	// Vulkan requires a positive descriptorPoolSizeCount for some drivers.
	// Empty sets are legal; reserve one unused uniform descriptor for them.
	if ( nSizes == 0 ) {
		sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
		nSizes = 1;
	}
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = nSizes;
	poolInfo.pPoolSizes = sizes;
	VkDescriptorPool hPool = VK_NULL_HANDLE;
	const VkResult ePool = pfnCreatePool( m_Device, &poolInfo, nullptr, &hPool );
	if ( ePool != VK_SUCCESS || hPool == VK_NULL_HANDLE ) {
		m_bDeviceLost = ePool == VK_ERROR_DEVICE_LOST;
		return ePool == VK_ERROR_OUT_OF_HOST_MEMORY || ePool == VK_ERROR_OUT_OF_DEVICE_MEMORY ? SIR_VULKAN_ALLOCATION_FAILED
		                                                                                      : SIR_VULKAN_NATIVE_FAILURE;
	}
	VkDescriptorSetAllocateInfo allocation = {};
	allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocation.descriptorPool = hPool;
	allocation.descriptorSetCount = 1;
	allocation.pSetLayouts = &pPipeline->szSetLayouts[nSetIndex];
	VkDescriptorSet hSet = VK_NULL_HANDLE;
	const VkResult eSet = pfnAllocateSets( m_Device, &allocation, &hSet );
	if ( eSet != VK_SUCCESS || hSet == VK_NULL_HANDLE ) {
		pfnDestroyPool( m_Device, hPool, nullptr );
		m_bDeviceLost = eSet == VK_ERROR_DEVICE_LOST;
		return eSet == VK_ERROR_OUT_OF_HOST_MEMORY || eSet == VK_ERROR_OUT_OF_DEVICE_MEMORY || eSet == VK_ERROR_OUT_OF_POOL_MEMORY
		           ? SIR_VULKAN_ALLOCATION_FAILED
				   : SIR_VULKAN_NATIVE_FAILURE;
	}
	pSet->hPool = hPool;
	pSet->hSet = hSet;
	pSet->hPipelineLayout = pPipeline->hLayout;
	pSet->nSetIndex = nSetIndex;
	pSet->nDeviceSerial = m_nDeviceSerial;
	pSet->nLastGpuUse = 0;
	++m_nLiveDescriptorSets;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::updateDescriptorSet( const sirVulkanDescriptorSet_t *pSet, const VkWriteDescriptorSet *pWrites,
                                                        uint32_t nCount ) noexcept
{
	if ( !m_bReady || m_bDeviceLost )
		return SIR_VULKAN_INVALID_STATE;
	if ( pSet == nullptr || pSet->hSet == VK_NULL_HANDLE || pSet->hPool == VK_NULL_HANDLE || pSet->nDeviceSerial != m_nDeviceSerial ||
	     pWrites == nullptr || nCount == 0 || nCount > 32 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pSet->nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion = { m_nDeviceSerial, pSet->nLastGpuUse };
		bool bCompleted = false;
		const sirVulkanResult_t ePoll = queryCompletion( &completion, &bCompleted );
		if ( ePoll != SIR_VULKAN_SUCCESS )
			return ePoll;
		if ( !bCompleted )
			return SIR_VULKAN_BUSY;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( pWrites[i].dstSet != pSet->hSet || pWrites[i].descriptorCount != 1 )
			return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const PFN_vkUpdateDescriptorSets pfnUpdate = ( PFN_vkUpdateDescriptorSets )m_pfnGetDeviceProcAddr( m_Device, "vkUpdateDescriptorSets" );
	if ( pfnUpdate == nullptr )
		return SIR_VULKAN_API_UNSUPPORTED;
	pfnUpdate( m_Device, nCount, pWrites, 0, nullptr );
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::destroyDescriptorSet( sirVulkanDescriptorSet_t *pSet ) noexcept
{
	if ( !m_bReady || m_bDeviceLost )
		return SIR_VULKAN_INVALID_STATE;
	if ( pSet == nullptr || pSet->hPool == VK_NULL_HANDLE || pSet->hSet == VK_NULL_HANDLE || pSet->nDeviceSerial != m_nDeviceSerial ||
	     pSet->nLastGpuUse > m_nLastTransferValue )
		return SIR_VULKAN_INVALID_ARGUMENT;
	if ( pSet->nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion = { m_nDeviceSerial, pSet->nLastGpuUse };
		bool bCompleted = false;
		const sirVulkanResult_t ePoll = queryCompletion( &completion, &bCompleted );
		if ( ePoll != SIR_VULKAN_SUCCESS )
			return ePoll;
		if ( !bCompleted )
			return SIR_VULKAN_BUSY;
	}
	const PFN_vkDestroyDescriptorPool pfnDestroyPool =
	    ( PFN_vkDestroyDescriptorPool )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyDescriptorPool" );
	if ( pfnDestroyPool == nullptr )
		return SIR_VULKAN_API_UNSUPPORTED;
	pfnDestroyPool( m_Device, pSet->hPool, nullptr );
	*pSet = {};
	--m_nLiveDescriptorSets;
	return SIR_VULKAN_SUCCESS;
}
