/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>
#include <Renderer/RHI/SIRRenderBindingLayout.h>

static VkDescriptorType sirVulkanRHIType( uint32_t nType ) noexcept
{
	switch ( nType ) {
	case 0: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case 1: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case 2: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case 3: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case 4: return VK_DESCRIPTOR_TYPE_SAMPLER;
	default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

SIRVulkanResult SIRVulkanDevice::createDescriptorSetLayout( const SIRRenderBinding *pBindings, uint32_t nBindingCount,
                                                              VkDescriptorSetLayout *pLayout ) noexcept
{
	if ( !m_bReady || m_bDeviceLost || pLayout == nullptr || *pLayout != VK_NULL_HANDLE ||
	     ( nBindingCount != 0 && pBindings == nullptr ) || nBindingCount > 32 )
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	VkDescriptorSetLayoutBinding bindings[32]{};
	for ( uint32_t i = 0; i < nBindingCount; ++i ) {
		const SIRRenderBinding& binding = pBindings[i];
		const VkDescriptorType type = sirVulkanRHIType( binding.nType );
		if ( type == VK_DESCRIPTOR_TYPE_MAX_ENUM || binding.nArrayLength == 0 || binding.nStageMask == 0 )
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		bindings[i].binding = binding.nBinding;
		bindings[i].descriptorType = type;
		bindings[i].descriptorCount = binding.nArrayLength;
		bindings[i].stageFlags = ( binding.nStageMask & ( 1U << 0 ) ? VK_SHADER_STAGE_VERTEX_BIT : 0 ) |
		                         ( binding.nStageMask & ( 1U << 1 ) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0 ) |
		                         ( binding.nStageMask & ( 1U << 5 ) ? VK_SHADER_STAGE_COMPUTE_BIT : 0 );
		if ( bindings[i].stageFlags == 0 ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		for ( uint32_t j = 0; j < i; ++j ) if ( bindings[j].binding == bindings[i].binding ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( m_Procs.vkCreateDescriptorSetLayout == nullptr ) return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = nBindingCount;
	info.pBindings = bindings;
	const VkResult result = m_Procs.vkCreateDescriptorSetLayout( m_Device, &info, nullptr, pLayout );
	if ( result != VK_SUCCESS ) {
		m_bDeviceLost = result == VK_ERROR_DEVICE_LOST;
		return result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY ? SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::destroyDescriptorSetLayout( VkDescriptorSetLayout *pLayout ) noexcept
{
	if ( pLayout == nullptr || *pLayout == VK_NULL_HANDLE ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	if ( !m_bReady || m_bDeviceLost || m_Procs.vkDestroyDescriptorSetLayout == nullptr ) return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	m_Procs.vkDestroyDescriptorSetLayout( m_Device, *pLayout, nullptr );
	*pLayout = VK_NULL_HANDLE;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::createDescriptorSet( VkDescriptorSetLayout layout, const SIRRenderBinding *pBindings,
                                                       uint32_t nBindingCount, sirVulkanDescriptorSet_t *pSet ) noexcept
{
	if ( !m_bReady || m_bDeviceLost || layout == VK_NULL_HANDLE || pSet == nullptr || pSet->hSet != VK_NULL_HANDLE ||
	     ( nBindingCount != 0 && pBindings == nullptr ) || nBindingCount > 32 ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	VkDescriptorPoolSize sizes[5]{};
	uint32_t nSizes = 0;
	for ( uint32_t i = 0; i < nBindingCount; ++i ) {
		const VkDescriptorType type = sirVulkanRHIType( pBindings[i].nType );
		if ( type == VK_DESCRIPTOR_TYPE_MAX_ENUM ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		uint32_t j = 0; while ( j < nSizes && sizes[j].type != type ) ++j;
		if ( j == nSizes ) { if ( nSizes == 5 ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT; sizes[j].type = type; ++nSizes; }
		sizes[j].descriptorCount += pBindings[i].nArrayLength;
	}
	if ( nSizes == 0 ) { sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }; nSizes = 1; }
	if ( m_Procs.vkCreateDescriptorPool == nullptr || m_Procs.vkAllocateDescriptorSets == nullptr ) return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	VkDescriptorPoolCreateInfo poolInfo{}; poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; poolInfo.maxSets = 1; poolInfo.poolSizeCount = nSizes; poolInfo.pPoolSizes = sizes;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkResult result = m_Procs.vkCreateDescriptorPool( m_Device, &poolInfo, nullptr, &pool );
	if ( result != VK_SUCCESS ) return result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY ? SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	VkDescriptorSetAllocateInfo allocation{}; allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO; allocation.descriptorPool = pool; allocation.descriptorSetCount = 1; allocation.pSetLayouts = &layout;
	VkDescriptorSet set = VK_NULL_HANDLE;
	result = m_Procs.vkAllocateDescriptorSets( m_Device, &allocation, &set );
	if ( result != VK_SUCCESS ) { if ( m_Procs.vkDestroyDescriptorPool ) m_Procs.vkDestroyDescriptorPool( m_Device, pool, nullptr ); return result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_POOL_MEMORY ? SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE; }
	*pSet = {};
	pSet->hSet = set;
	pSet->hPool = pool;
	pSet->hSetLayout = layout;
	pSet->nDeviceSerial = m_nDeviceSerial;
	++m_nLiveDescriptorSets;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::updateDescriptorSet( const sirVulkanDescriptorSet_t *pSet, const VkWriteDescriptorSet *pWrites,
                                                        uint32_t nCount ) noexcept
{
	if ( !m_bReady || m_bDeviceLost )
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	if ( pSet == nullptr || pSet->hSet == VK_NULL_HANDLE || pSet->hPool == VK_NULL_HANDLE || pSet->nDeviceSerial != m_nDeviceSerial ||
	     pWrites == nullptr || nCount == 0 || nCount > 32 ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pSet->nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion = { m_nDeviceSerial, pSet->nLastGpuUse };
		bool bCompleted = false;
		const SIRVulkanResult ePoll = queryCompletion( &completion, &bCompleted );
		if ( ePoll != SIRVulkanResult::SIR_VULKAN_SUCCESS )
			return ePoll;
		if ( !bCompleted )
			return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( pWrites[i].dstSet != pSet->hSet || pWrites[i].descriptorCount != 1 )
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const PFN_vkUpdateDescriptorSets pfnUpdate = m_Procs.vkUpdateDescriptorSets;
	if ( pfnUpdate == nullptr )
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	pfnUpdate( m_Device, nCount, pWrites, 0, nullptr );
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::destroyDescriptorSet( sirVulkanDescriptorSet_t *pSet ) noexcept
{
	if ( !m_bReady || m_bDeviceLost )
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	if ( pSet == nullptr || pSet->hPool == VK_NULL_HANDLE || pSet->hSet == VK_NULL_HANDLE || pSet->nDeviceSerial != m_nDeviceSerial ||
	     pSet->nLastGpuUse > m_nLastTransferValue )
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	if ( pSet->nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion = { m_nDeviceSerial, pSet->nLastGpuUse };
		bool bCompleted = false;
		const SIRVulkanResult ePoll = queryCompletion( &completion, &bCompleted );
		if ( ePoll != SIRVulkanResult::SIR_VULKAN_SUCCESS )
			return ePoll;
		if ( !bCompleted )
			return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	const PFN_vkDestroyDescriptorPool pfnDestroyPool =
	    m_Procs.vkDestroyDescriptorPool;
	if ( pfnDestroyPool == nullptr )
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	pfnDestroyPool( m_Device, pSet->hPool, nullptr );
	*pSet = {};
	--m_nLiveDescriptorSets;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
