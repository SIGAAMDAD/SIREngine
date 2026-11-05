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

sirGraphicsResult_t SIRVulkanRenderingDevice::createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eValidation = sirGraphicsValidatePipeline( desc );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( desc.eType != SIR_GRAPHICS_PIPELINE_COMPUTE || desc.nBindingCount != 0 ||
	     desc.computeShader.eFormat != SIR_GRAPHICS_SHADER_SPIRV ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirVulkanValidateSpirv( desc.computeShader.pBytes, desc.computeShader.nByteCount, desc.computeShader.pszEntryPoint ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_PIPELINE_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsPipelineSlot_t *pSlot = &m_szPipelines[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const sirVulkanResult_t eCreate = m_Native.createComputePipeline( desc.computeShader.pBytes, desc.computeShader.nByteCount,
		                                                                  desc.computeShader.pszEntryPoint, &pSlot->native );
		switch ( eCreate ) {
		case SIR_VULKAN_SUCCESS:
			break;
		case SIR_VULKAN_INVALID_ARGUMENT:
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		case SIR_VULKAN_API_UNSUPPORTED:
			return SIR_GRAPHICS_UNSUPPORTED;
		case SIR_VULKAN_ALLOCATION_FAILED:
			return SIR_GRAPHICS_OUT_OF_MEMORY;
		default:
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		m_nNativeSerial = pSlot->native.nDeviceSerial;
		pSlot->bActive = true;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_PIPELINE };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRVulkanCommandList::bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( hPipeline.eType != SIR_GRAPHICS_HANDLE_PIPELINE || hPipeline.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
	     hPipeline.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_PIPELINE_CAPACITY ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirVulkanGraphicsPipelineSlot_t *pSlot = &m_pOwner->m_szPipelines[hPipeline.nSlot];
	if ( !pSlot->bActive || pSlot->nGeneration != hPipeline.nGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	m_hBoundPipeline = hPipeline;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::dispatch( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( nGroupCountX == 0 || nGroupCountY == 0 || nGroupCountZ == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsHandle_t hPipeline = m_hBoundPipeline;
	if ( hPipeline.eType != SIR_GRAPHICS_HANDLE_PIPELINE || hPipeline.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
	     hPipeline.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_PIPELINE_CAPACITY ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirVulkanGraphicsPipelineSlot_t *pSlot = &m_pOwner->m_szPipelines[hPipeline.nSlot];
	if ( !pSlot->bActive || pSlot->nGeneration != hPipeline.nGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirVulkanAdapterInfo_t adapter = {};
	if ( !m_pOwner->m_Native.queryAdapter( &adapter ) || !adapter.bComputeCapable ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nGroupCountX > adapter.szMaximumComputeGroupCount[0] || nGroupCountY > adapter.szMaximumComputeGroupCount[1] ||
	     nGroupCountZ > adapter.szMaximumComputeGroupCount[2] ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanGraphicsOperation_t& operation = m_szOperations[m_nOperationCount++];
	operation = {};
	operation.eKind = SIR_VULKAN_GRAPHICS_COMPUTE_DISPATCH;
	operation.hPipeline = hPipeline;
	operation.nGroupCountX = nGroupCountX;
	operation.nGroupCountY = nGroupCountY;
	operation.nGroupCountZ = nGroupCountZ;
	return SIR_GRAPHICS_SUCCESS;
}
