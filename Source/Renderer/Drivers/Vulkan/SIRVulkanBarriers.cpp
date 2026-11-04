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

static sirGraphicsResult_t sirVulkanBarrierState( sirGraphicsState_t eState, const sirVulkanImage_t *pImage,
                                                  VkImageLayout *pLayout ) noexcept
{
	if ( pImage == nullptr || pLayout == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_UNDEFINED:
		*pLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_COMMON:
		*pLayout = VK_IMAGE_LAYOUT_GENERAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_SHADER_READ:
		if ( ( pImage->nUsage & VK_IMAGE_USAGE_SAMPLED_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		if ( ( pImage->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_GENERAL;
		return SIR_GRAPHICS_SUCCESS;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
}

sirGraphicsResult_t sirVulkanPlanImageBarrier( const sirGraphicsBarrier_t &barrier, const sirVulkanImage_t *pImage,
                                               sirVulkanImageBarrier_t *pNative ) noexcept
{
	if ( pNative == nullptr || pImage == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	if ( barrier.eType != SIR_GRAPHICS_BARRIER_TRANSITION && barrier.eType != SIR_GRAPHICS_BARRIER_UAV ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( barrier.nMipCount != 1 || barrier.nLayerCount != 1 || barrier.nBaseMip >= pImage->nMips ||
	     barrier.nBaseLayer >= pImage->nLayers ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pImage->eFormat == VK_FORMAT_D16_UNORM || pImage->eFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
	     pImage->eFormat == VK_FORMAT_D32_SFLOAT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout eBefore = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout eAfter = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( !sirVulkanGetSubresourceLayout( pImage, barrier.nBaseMip, barrier.nBaseLayer, &eOld ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eBeforeResult = sirVulkanBarrierState( barrier.eBefore, pImage, &eBefore );
	if ( eBeforeResult != SIR_GRAPHICS_SUCCESS ) {
		return eBeforeResult;
	}
	const sirGraphicsResult_t eAfterResult = sirVulkanBarrierState( barrier.eAfter, pImage, &eAfter );
	if ( eAfterResult != SIR_GRAPHICS_SUCCESS ) {
		return eAfterResult;
	}
	if ( eOld != eBefore || eAfter == VK_IMAGE_LAYOUT_UNDEFINED ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( barrier.eType == SIR_GRAPHICS_BARRIER_UAV ) {
		if ( eOld != VK_IMAGE_LAYOUT_GENERAL || eAfter != VK_IMAGE_LAYOUT_GENERAL || barrier.eBefore != SIR_GRAPHICS_STATE_SHADER_WRITE ||
		     barrier.eAfter != SIR_GRAPHICS_STATE_SHADER_WRITE ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( eOld == eAfter ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( eAfter == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		const uint32_t nIndex = barrier.nBaseLayer * pImage->nMips + barrier.nBaseMip;
		if ( ( pImage->szUploadedSubresources[nIndex / 64] & ( 1ULL << ( nIndex % 64 ) ) ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	pNative->eBefore = eOld;
	pNative->eAfter = eAfter;
	pNative->nMip = barrier.nBaseMip;
	pNative->nLayer = barrier.nBaseLayer;
	pNative->bUav = barrier.eType == SIR_GRAPHICS_BARRIER_UAV;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_bHasCopy || m_bHasBarrier || m_pOwner == nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( nCount == 0 ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( pBarriers == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nCount != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsBarrier_t &barrier = pBarriers[0];
	if ( barrier.hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE || barrier.hResource.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
	     barrier.hResource.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_TEXTURE_CAPACITY ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirVulkanGraphicsImageSlot_t *pImage = &m_pOwner->m_szImages[barrier.hResource.nSlot];
	if ( !pImage->bActive || pImage->nGeneration != barrier.hResource.nGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirVulkanImageBarrier_t native = {};
	const sirGraphicsResult_t ePlan = sirVulkanPlanImageBarrier( barrier, &pImage->native, &native );
	if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
		return ePlan;
	}
	m_Barrier = barrier;
	m_bHasBarrier = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::submitImageBarrier( sirVulkanImage_t *pImage, const sirVulkanImageBarrier_t &barrier,
                                                       sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pImage == nullptr || pCompletion == nullptr || pImage->hImage == VK_NULL_HANDLE || pImage->pOwnerAllocator != m_Allocator ||
	     pImage->nDeviceSerial != m_nDeviceSerial || m_nLastTransferValue == UINT64_MAX ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	VkImageLayout eTracked = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( !sirVulkanGetSubresourceLayout( pImage, barrier.nMip, barrier.nLayer, &eTracked ) || eTracked != barrier.eBefore ||
	     ( barrier.bUav && ( barrier.eBefore != VK_IMAGE_LAYOUT_GENERAL || barrier.eAfter != VK_IMAGE_LAYOUT_GENERAL ||
	                         ( pImage->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) == 0 ) ) ||
	     ( !barrier.bUav && barrier.eBefore == barrier.eAfter ) ||
	     ( barrier.eAfter != VK_IMAGE_LAYOUT_GENERAL && barrier.eAfter != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ||
	     ( barrier.eAfter == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && ( pImage->nUsage & VK_IMAGE_USAGE_SAMPLED_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	uint64_t nCompleted = 0;
	const VkResult ePoll = m_pfnGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted );
	if ( ePoll != VK_SUCCESS ) {
		m_bDeviceLost = ePoll == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	sirVulkanTransferSlot_t *pSlot = nullptr;
	for ( uint32_t iCheck = 0; iCheck < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iCheck ) {
		const uint32_t iSlot = ( m_iNextTransferSlot + iCheck ) % SIR_VULKAN_TRANSFER_SLOT_COUNT;
		if ( m_TransferSlots[iSlot].nLastValue <= nCompleted ) {
			pSlot = &m_TransferSlots[iSlot];
			m_iNextTransferSlot = ( iSlot + 1 ) % SIR_VULKAN_TRANSFER_SLOT_COUNT;
			break;
		}
	}
	if ( pSlot == nullptr ) {
		return SIR_VULKAN_BUSY;
	}
	if ( m_pfnResetCommandBuffer( pSlot->hCommandBuffer, 0 ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ( m_pfnBeginCommandBuffer( pSlot->hCommandBuffer, &begin ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	VkImageMemoryBarrier2 transition = {};
	transition.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	transition.srcStageMask =
	    barrier.eBefore == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	transition.srcAccessMask =
	    barrier.eBefore == VK_IMAGE_LAYOUT_UNDEFINED ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	transition.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	transition.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	transition.oldLayout = barrier.eBefore;
	transition.newLayout = barrier.eAfter;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = pImage->hImage;
	transition.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	transition.subresourceRange.baseMipLevel = barrier.nMip;
	transition.subresourceRange.levelCount = 1;
	transition.subresourceRange.baseArrayLayer = barrier.nLayer;
	transition.subresourceRange.layerCount = 1;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &transition;
	m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );
	if ( m_pfnEndCommandBuffer( pSlot->hCommandBuffer ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	const uint64_t nNext = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo command = {};
	command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	command.commandBuffer = pSlot->hCommandBuffer;
	VkSemaphoreSubmitInfo signal = {};
	signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal.semaphore = m_TransferTimeline;
	signal.value = nNext;
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submission = {};
	submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submission.commandBufferInfoCount = 1;
	submission.pCommandBufferInfos = &command;
	submission.signalSemaphoreInfoCount = 1;
	submission.pSignalSemaphoreInfos = &signal;
	const VkResult eSubmit = m_pfnQueueSubmit2( m_GraphicsQueue, 1, &submission, VK_NULL_HANDLE );
	if ( eSubmit != VK_SUCCESS ) {
		m_bDeviceLost = eSubmit == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = nNext;
	pSlot->nLastValue = nNext;
	pImage->nLastGpuUse = nNext;
	( void )sirVulkanSetSubresourceLayout( pImage, barrier.nMip, barrier.nLayer, barrier.eAfter );
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nNext;
	return SIR_VULKAN_SUCCESS;
}
