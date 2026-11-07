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

#include <stdint.h>
#include <string.h>

// A bounded, alignment-safe envelope check. The shader cooker owns full
// SPIR-V instruction/entry-point semantic validation before runtime use.
bool sirVulkanValidateSpirv( const void *pBytes, uint64_t nByteCount, const char *pszEntryPoint ) noexcept
{
	if ( pBytes == nullptr || pszEntryPoint == nullptr || nByteCount < 20 || nByteCount > ( 64ULL << 20 ) || ( nByteCount & 3U ) != 0 ||
	     ( ( uintptr_t )pBytes & 3U ) != 0 ) {
		return false;
	}
	uint32_t szHeader[5] = {};
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

sirVulkanResult_t SIRVulkanDevice::createComputePipeline( const void *pCode, uint64_t nCodeBytes, const char *pszEntryPoint,
                                                          sirVulkanComputePipeline_t *pPipeline ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pPipeline == nullptr || pPipeline->hPipeline != VK_NULL_HANDLE || pPipeline->hLayout != VK_NULL_HANDLE ||
	     !sirVulkanValidateSpirv( pCode, nCodeBytes, pszEntryPoint ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const PFN_vkCreateShaderModule pfnCreateShaderModule =
	    ( PFN_vkCreateShaderModule )m_pfnGetDeviceProcAddr( m_Device, "vkCreateShaderModule" );
	const PFN_vkDestroyShaderModule pfnDestroyShaderModule =
	    ( PFN_vkDestroyShaderModule )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyShaderModule" );
	const PFN_vkCreatePipelineLayout pfnCreatePipelineLayout =
	    ( PFN_vkCreatePipelineLayout )m_pfnGetDeviceProcAddr( m_Device, "vkCreatePipelineLayout" );
	const PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout =
	    ( PFN_vkDestroyPipelineLayout )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyPipelineLayout" );
	const PFN_vkCreateComputePipelines pfnCreateComputePipelines =
	    ( PFN_vkCreateComputePipelines )m_pfnGetDeviceProcAddr( m_Device, "vkCreateComputePipelines" );
	if ( pfnCreateShaderModule == nullptr || pfnDestroyShaderModule == nullptr || pfnCreatePipelineLayout == nullptr ||
	     pfnDestroyPipelineLayout == nullptr || pfnCreateComputePipelines == nullptr ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}

	VkShaderModuleCreateInfo shaderInfo = {};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = ( size_t )nCodeBytes;
	shaderInfo.pCode = ( const uint32_t * )pCode;
	VkShaderModule hModule = VK_NULL_HANDLE;
	const VkResult eModule = pfnCreateShaderModule( m_Device, &shaderInfo, nullptr, &hModule );
	if ( eModule != VK_SUCCESS ) {
		m_bDeviceLost = eModule == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VkPipelineLayout hLayout = VK_NULL_HANDLE;
	const VkResult eLayout = pfnCreatePipelineLayout( m_Device, &layoutInfo, nullptr, &hLayout );
	if ( eLayout != VK_SUCCESS ) {
		pfnDestroyShaderModule( m_Device, hModule, nullptr );
		m_bDeviceLost = eLayout == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	VkPipelineShaderStageCreateInfo stage = {};
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = hModule;
	stage.pName = pszEntryPoint;
	VkComputePipelineCreateInfo computeInfo = {};
	computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computeInfo.stage = stage;
	computeInfo.layout = hLayout;
	VkPipeline hPipeline = VK_NULL_HANDLE;
	const VkResult eCreate = pfnCreateComputePipelines( m_Device, VK_NULL_HANDLE, 1, &computeInfo, nullptr, &hPipeline );
	pfnDestroyShaderModule( m_Device, hModule, nullptr );
	if ( eCreate != VK_SUCCESS || hPipeline == VK_NULL_HANDLE ) {
		const PFN_vkDestroyPipeline pfnDestroyPipeline = ( PFN_vkDestroyPipeline )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyPipeline" );
		if ( hPipeline != VK_NULL_HANDLE && pfnDestroyPipeline != nullptr ) {
			pfnDestroyPipeline( m_Device, hPipeline, nullptr );
		}
		pfnDestroyPipelineLayout( m_Device, hLayout, nullptr );
		m_bDeviceLost = eCreate == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	*pPipeline = {};
	pPipeline->hPipeline = hPipeline;
	pPipeline->hLayout = hLayout;
	pPipeline->nDeviceSerial = m_nDeviceSerial;
	++m_nLivePipelines;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::destroyComputePipeline( sirVulkanComputePipeline_t *pPipeline ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pPipeline == nullptr || pPipeline->hPipeline == VK_NULL_HANDLE || pPipeline->hLayout == VK_NULL_HANDLE ||
	     pPipeline->nDeviceSerial != m_nDeviceSerial || pPipeline->nLastGpuUse > m_nLastTransferValue ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( pPipeline->nLastGpuUse != 0 ) {
		bool bCompleted = false;
		const sirVulkanCompletion_t completion = { m_nDeviceSerial, pPipeline->nLastGpuUse };
		const sirVulkanResult_t ePoll = queryCompletion( &completion, &bCompleted );
		if ( ePoll != SIR_VULKAN_SUCCESS ) {
			return ePoll;
		}
		if ( !bCompleted ) {
			return SIR_VULKAN_BUSY;
		}
	}
	const PFN_vkDestroyPipeline pfnDestroyPipeline = ( PFN_vkDestroyPipeline )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyPipeline" );
	const PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout =
	    ( PFN_vkDestroyPipelineLayout )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyPipelineLayout" );
	if ( pfnDestroyPipeline == nullptr || pfnDestroyPipelineLayout == nullptr ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	pfnDestroyPipeline( m_Device, pPipeline->hPipeline, nullptr );
	pfnDestroyPipelineLayout( m_Device, pPipeline->hLayout, nullptr );
	*pPipeline = {};
	--m_nLivePipelines;
	return SIR_VULKAN_SUCCESS;
}
