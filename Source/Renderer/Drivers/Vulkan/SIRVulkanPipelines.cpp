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

SIRGraphicsResult SIRVulkanRenderingDevice::createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	const SIRGraphicsResult eValidation = sirGraphicsValidatePipeline( desc );
	if ( eValidation != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( ( desc.eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE && desc.eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) ||
	     ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE && desc.nBindingCount != 0 ) ||
	     ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE
	           ? desc.computeShader.eFormat != SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_SPIRV
			   : desc.vertexShader.eFormat != SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_SPIRV || desc.pixelShader.eFormat != SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_SPIRV ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE &&
	     !sirVulkanValidateSpirv( desc.computeShader.pBytes, desc.computeShader.nByteCount, desc.computeShader.pszEntryPoint ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_PIPELINE_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsPipelineSlot_t *pSlot = &m_szPipelines[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const SIRVulkanResult eCreate = desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE
		                                      ? m_Native.createComputePipeline( desc.computeShader.pBytes, desc.computeShader.nByteCount,
		                                                                        desc.computeShader.pszEntryPoint, &pSlot->native )
		                                      : m_Native.createRasterPipeline( desc, &pSlot->native );
		switch ( eCreate ) {
		case SIRVulkanResult::SIR_VULKAN_SUCCESS:
			break;
		case SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT:
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		case SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED:
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		case SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED:
			return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
		default:
			return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
		}
		pSlot->bGraphics = desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS;
		m_nNativeSerial = pSlot->native.nDeviceSerial;
		pSlot->bActive = true;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE };
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

SIRGraphicsResult SIRVulkanCommandList::bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept
{
	if ( m_eState != SIRVulkanCommandState::SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( hPipeline.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE || hPipeline.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
	     hPipeline.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_PIPELINE_CAPACITY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirVulkanGraphicsPipelineSlot_t *pSlot = &m_pOwner->m_szPipelines[hPipeline.nSlot];
	if ( !pSlot->bActive || pSlot->nGeneration != hPipeline.nGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->bGraphics != m_bInRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_bInRenderPass && m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	m_hBoundPipeline = hPipeline;
	if ( m_bInRenderPass ) {
		sirVulkanGraphicsOperation_t& operation = m_szOperations[m_nOperationCount++];
		operation = {};
		operation.eKind = SIRVulkanGraphicsOpKind::SIR_VULKAN_GRAPHICS_BIND_PIPELINE;
		operation.hPipeline = hPipeline;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRVulkanCommandList::dispatch( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept
{
	if ( m_eState != SIRVulkanCommandState::SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( nGroupCountX == 0 || nGroupCountY == 0 || nGroupCountZ == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsHandle_t hPipeline = m_hBoundPipeline;
	if ( hPipeline.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE || hPipeline.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
	     hPipeline.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_PIPELINE_CAPACITY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirVulkanGraphicsPipelineSlot_t *pSlot = &m_pOwner->m_szPipelines[hPipeline.nSlot];
	if ( !pSlot->bActive || pSlot->nGeneration != hPipeline.nGeneration || pSlot->bGraphics || m_bInRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirVulkanAdapterInfo_t adapter {};
	if ( !m_pOwner->m_Native.queryAdapter( &adapter ) || !adapter.bComputeCapable ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nGroupCountX > adapter.szMaximumComputeGroupCount[0] || nGroupCountY > adapter.szMaximumComputeGroupCount[1] ||
	     nGroupCountZ > adapter.szMaximumComputeGroupCount[2] ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& operation = m_szOperations[m_nOperationCount++];
	operation = {};
	operation.eKind = SIRVulkanGraphicsOpKind::SIR_VULKAN_GRAPHICS_COMPUTE_DISPATCH;
	operation.hPipeline = hPipeline;
	operation.nGroupCountX = nGroupCountX;
	operation.nGroupCountY = nGroupCountY;
	operation.nGroupCountZ = nGroupCountZ;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
