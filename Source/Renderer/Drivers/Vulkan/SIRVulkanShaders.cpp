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

SIRVulkanResult SIRVulkanDevice::createShaderModule( const void *pCode, uint64_t nCodeBytes, const char *pszEntryPoint, VkShaderModule *pModule ) noexcept
{
	if ( !m_bReady || pCode == nullptr || pModule == nullptr || nCodeBytes == 0 || ( nCodeBytes & 3U ) != 0 || m_Procs.vkCreateShaderModule == nullptr ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !sirVulkanValidateSpirv( pCode, nCodeBytes, pszEntryPoint ) ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = nCodeBytes;
	info.pCode = static_cast<const uint32_t *>( pCode );
	return m_Procs.vkCreateShaderModule( m_Device, &info, nullptr, pModule ) == VK_SUCCESS ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
}

SIRVulkanResult SIRVulkanDevice::destroyShaderModule( VkShaderModule *pModule ) noexcept
{
	if ( pModule == nullptr ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	if ( *pModule != VK_NULL_HANDLE && m_Procs.vkDestroyShaderModule != nullptr ) m_Procs.vkDestroyShaderModule( m_Device, *pModule, nullptr );
	*pModule = VK_NULL_HANDLE;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

#include <stdint.h>
#include <string.h>

// A bounded, alignment-safe envelope check. The shader cooker owns full
// SPIR-V instruction/entry-point semantic validation before runtime use.
bool sirVulkanValidateSpirv( const void *pBytes, uint64_t nByteCount, const char *pszEntryPoint ) noexcept
{
	if ( pBytes == nullptr || pszEntryPoint == nullptr || nByteCount < 20 || nByteCount > ( 64ULL << 20 ) || ( nByteCount & 3U ) != 0 ||
	     ( reinterpret_cast<uintptr_t>(pBytes) & 3U ) != 0 ) {
		return false;
	}
	uint32_t szHeader[5];
	memset( szHeader, 0, sizeof( szHeader ) );
	memcpy( szHeader, pBytes, sizeof( szHeader ) );
	const uint32_t nVersion = szHeader[1];
	if ( szHeader[0] != 0x07230203U || nVersion < 0x00010000U || nVersion > 0x00010600U || ( nVersion & 0x000000ffU ) != 0 ||
	     szHeader[3] == 0 || szHeader[4] != 0 ) {
		return false;
	}
	for ( uint32_t iChar = 0; iChar < 128; ++iChar ) {
		if ( pszEntryPoint[iChar] == '\0' ) {
			return iChar != 0;
		}
	}
	return false;
}

SIRVulkanResult SIRVulkanDevice::createComputePipeline( VkShaderModule hShader, const char *pszEntryPoint,
                                                          const VkDescriptorSetLayout *pSetLayouts, uint32_t nSetLayoutCount,
                                                          sirVulkanComputePipeline_t *pPipeline ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pPipeline == nullptr || pPipeline->hPipeline != VK_NULL_HANDLE || pPipeline->hLayout != VK_NULL_HANDLE || hShader == VK_NULL_HANDLE ||
	     ( nSetLayoutCount != 0 && pSetLayouts == nullptr ) || nSetLayoutCount > 4 ||
	     pszEntryPoint == nullptr || pszEntryPoint[0] == '\0' ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const PFN_vkCreatePipelineLayout pfnCreatePipelineLayout =
	    m_Procs.vkCreatePipelineLayout;
	const PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout =
	    m_Procs.vkDestroyPipelineLayout;
	const PFN_vkCreateComputePipelines pfnCreateComputePipelines =
	    m_Procs.vkCreateComputePipelines;
	if ( pfnCreatePipelineLayout == nullptr || pfnDestroyPipelineLayout == nullptr || pfnCreateComputePipelines == nullptr ) {
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}

	VkPipelineLayoutCreateInfo layoutInfo {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = nSetLayoutCount;
	layoutInfo.pSetLayouts = pSetLayouts;
	VkPipelineLayout hLayout = VK_NULL_HANDLE;
	const VkResult eLayout = pfnCreatePipelineLayout( m_Device, &layoutInfo, nullptr, &hLayout );
	if ( eLayout != VK_SUCCESS ) {
		m_bDeviceLost = eLayout == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	VkPipelineShaderStageCreateInfo stage {};
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = hShader;
	stage.pName = pszEntryPoint;
	VkComputePipelineCreateInfo computeInfo {};
	computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computeInfo.stage = stage;
	computeInfo.layout = hLayout;
	VkPipeline hPipeline = VK_NULL_HANDLE;
	const VkResult eCreate = pfnCreateComputePipelines( m_Device, VK_NULL_HANDLE, 1, &computeInfo, nullptr, &hPipeline );
	if ( eCreate != VK_SUCCESS || hPipeline == VK_NULL_HANDLE ) {
		const PFN_vkDestroyPipeline pfnDestroyPipeline = m_Procs.vkDestroyPipeline;
		if ( hPipeline != VK_NULL_HANDLE && pfnDestroyPipeline != nullptr ) {
			pfnDestroyPipeline( m_Device, hPipeline, nullptr );
		}
		pfnDestroyPipelineLayout( m_Device, hLayout, nullptr );
		m_bDeviceLost = eCreate == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	*pPipeline = {};
	pPipeline->hPipeline = hPipeline;
	pPipeline->hLayout = hLayout;
	pPipeline->nDeviceSerial = m_nDeviceSerial;
	pPipeline->nSetLayoutCount = nSetLayoutCount;
	pPipeline->bOwnsSetLayouts = false;
	for ( uint32_t i = 0; i < nSetLayoutCount; ++i )
		pPipeline->szSetLayouts[i] = pSetLayouts[i];
	++m_nLivePipelines;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::destroyComputePipeline( sirVulkanComputePipeline_t *pPipeline ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pPipeline == nullptr || pPipeline->hPipeline == VK_NULL_HANDLE || pPipeline->hLayout == VK_NULL_HANDLE ||
	     pPipeline->nDeviceSerial != m_nDeviceSerial || pPipeline->nLastGpuUse > m_nLastTransferValue ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pPipeline->nLastGpuUse != 0 ) {
		bool bCompleted = false;
		const sirVulkanCompletion_t completion = { m_nDeviceSerial, pPipeline->nLastGpuUse };
		const SIRVulkanResult ePoll = queryCompletion( &completion, &bCompleted );
		if ( ePoll != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return ePoll;
		}
		if ( !bCompleted ) {
			return SIRVulkanResult::SIR_VULKAN_BUSY;
		}
	}
	const PFN_vkDestroyPipeline pfnDestroyPipeline = m_Procs.vkDestroyPipeline;
	const PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout =
	    m_Procs.vkDestroyPipelineLayout;
	if ( pfnDestroyPipeline == nullptr || pfnDestroyPipelineLayout == nullptr ) {
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}
	pfnDestroyPipeline( m_Device, pPipeline->hPipeline, nullptr );
	pfnDestroyPipelineLayout( m_Device, pPipeline->hLayout, nullptr );
	if ( pPipeline->bOwnsSetLayouts && pPipeline->nSetLayoutCount != 0 ) {
		const PFN_vkDestroyDescriptorSetLayout pfnDestroySetLayout =
		    m_Procs.vkDestroyDescriptorSetLayout;
		if ( pfnDestroySetLayout != nullptr ) {
			for ( uint32_t i = 0; i < pPipeline->nSetLayoutCount; ++i ) {
				pfnDestroySetLayout( m_Device, pPipeline->szSetLayouts[i], nullptr );
			}
		}
	}
	*pPipeline = {};
	--m_nLivePipelines;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
