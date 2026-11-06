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
		*pLayout = sirVulkanImageAspectMask( pImage->eFormat ) == VK_IMAGE_ASPECT_COLOR_BIT
		               ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		               : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		if ( sirVulkanImageAspectMask( pImage->eFormat ) != VK_IMAGE_ASPECT_COLOR_BIT ||
		     ( pImage->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_GENERAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_DEPTH_WRITE:
		if ( sirVulkanImageAspectMask( pImage->eFormat ) == VK_IMAGE_ASPECT_COLOR_BIT ||
		     ( pImage->nUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_DEPTH_READ:
		if ( sirVulkanImageAspectMask( pImage->eFormat ) == VK_IMAGE_ASPECT_COLOR_BIT ||
		     ( pImage->nUsage & ( VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ) ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
		if ( ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		if ( ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		return SIR_GRAPHICS_SUCCESS;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
}

sirGraphicsResult_t sirVulkanPlanImageBarrier( const sirGraphicsBarrier_t& barrier, const sirVulkanImage_t *pImage,
                                               sirVulkanImageBarrier_t *pNative ) noexcept
{
	if ( pNative == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	if ( pImage == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( barrier.eType != SIR_GRAPHICS_BARRIER_TRANSITION && barrier.eType != SIR_GRAPHICS_BARRIER_UAV ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirGraphicsTextureBarrierRangeValid( barrier, pImage->nMips, pImage->nLayers ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	VkImageLayout eBefore = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout eAfter = VK_IMAGE_LAYOUT_UNDEFINED;
	const sirGraphicsResult_t eBeforeResult = sirVulkanBarrierState( barrier.eBefore, pImage, &eBefore );
	if ( eBeforeResult != SIR_GRAPHICS_SUCCESS ) {
		return eBeforeResult;
	}
	const sirGraphicsResult_t eAfterResult = sirVulkanBarrierState( barrier.eAfter, pImage, &eAfter );
	if ( eAfterResult != SIR_GRAPHICS_SUCCESS ) {
		return eAfterResult;
	}
	if ( eAfter == VK_IMAGE_LAYOUT_UNDEFINED ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( barrier.eType == SIR_GRAPHICS_BARRIER_UAV ) {
		if ( eBefore != VK_IMAGE_LAYOUT_GENERAL || eAfter != VK_IMAGE_LAYOUT_GENERAL ||
		     barrier.eBefore != SIR_GRAPHICS_STATE_SHADER_WRITE || barrier.eAfter != SIR_GRAPHICS_STATE_SHADER_WRITE ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( eBefore == eAfter ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanImageBarrier_t native = {};
	native.eBefore = eBefore;
	native.eAfter = eAfter;
	native.nMip = barrier.nBaseMip;
	native.nLayer = barrier.nBaseLayer;
	native.nMipCount = barrier.nMipCount;
	native.nLayerCount = barrier.nLayerCount;
	native.bUav = barrier.eType == SIR_GRAPHICS_BARRIER_UAV;
	if ( !sirVulkanValidateNativeImageBarrier( pImage, native ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pNative->eBefore = eBefore;
	pNative->eAfter = eAfter;
	pNative->nMip = barrier.nBaseMip;
	pNative->nLayer = barrier.nBaseLayer;
	pNative->nMipCount = barrier.nMipCount;
	pNative->nLayerCount = barrier.nLayerCount;
	pNative->bUav = barrier.eType == SIR_GRAPHICS_BARRIER_UAV;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_pOwner == nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( nCount == 0 ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( pBarriers == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nCount > SIR_VULKAN_COPY_BATCH_CAPACITY - m_nOperationCount ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	// Prevalidate all handles and buffer access scopes before recording.
	// Image layout transitions after earlier operations are revalidated
	// against the ordered shadow state during submission.
	for ( uint32_t iBarrier = 0; iBarrier < nCount; ++iBarrier ) {
		const sirGraphicsBarrier_t& barrier = pBarriers[iBarrier];
		if ( barrier.eType != SIR_GRAPHICS_BARRIER_TRANSITION && barrier.eType != SIR_GRAPHICS_BARRIER_UAV ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( barrier.hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
			const sirVulkanGraphicsBufferSlot_t *pBuffer = m_pOwner->resolveBuffer( barrier.hResource );
			if ( pBuffer == nullptr ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			if ( pBuffer->pMapped != nullptr ) {
				return SIR_GRAPHICS_BUSY;
			}
			sirVulkanBufferBarrier_t native = {};
			const sirGraphicsResult_t ePlan = sirVulkanPlanBufferBarrier( barrier, &pBuffer->native, &native );
			if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
				return ePlan;
			}
			continue;
		}
		if ( barrier.hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE ||
		     barrier.hResource.nDeviceGeneration != m_pOwner->m_nDeviceGeneration ||
		     barrier.hResource.nSlot >= SIRVulkanRenderingDevice::SIR_VULKAN_TEXTURE_CAPACITY ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const sirVulkanGraphicsImageSlot_t *pImage = &m_pOwner->m_szImages[barrier.hResource.nSlot];
		if ( !pImage->bActive || pImage->nGeneration != barrier.hResource.nGeneration ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( !sirGraphicsTextureBarrierRangeValid( barrier, pImage->native.nMips, pImage->native.nLayers ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( m_nOperationCount == 0 && iBarrier == 0 ) {
			sirVulkanImageBarrier_t planned = {};
			const sirGraphicsResult_t ePlan = sirVulkanPlanImageBarrier( barrier, &pImage->native, &planned );
			if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
				return ePlan;
			}
		}
	}
	for ( uint32_t iBarrier = 0; iBarrier < nCount; ++iBarrier ) {
		sirVulkanGraphicsOperation_t& operation = m_szOperations[m_nOperationCount++];
		operation = {};
		operation.eKind = pBarriers[iBarrier].hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER ? SIR_VULKAN_GRAPHICS_BUFFER_BARRIER
		                                                                                    : SIR_VULKAN_GRAPHICS_IMAGE_BARRIER;
		operation.barrier = pBarriers[iBarrier];
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::submitImageBarrier( sirVulkanImage_t *pImage, const sirVulkanImageBarrier_t& barrier,
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
	if ( !sirVulkanValidateNativeImageBarrier( pImage, barrier ) ) {
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
	transition.subresourceRange.aspectMask = sirVulkanImageAspectMask( pImage->eFormat );
	transition.subresourceRange.baseMipLevel = barrier.nMip;
	transition.subresourceRange.levelCount = barrier.nMipCount;
	transition.subresourceRange.baseArrayLayer = barrier.nLayer;
	transition.subresourceRange.layerCount = barrier.nLayerCount;
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
	for ( uint32_t iLayer = 0; iLayer < barrier.nLayerCount; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip < barrier.nMipCount; ++iMip ) {
			( void )sirVulkanSetSubresourceLayout( pImage, barrier.nMip + iMip, barrier.nLayer + iLayer, barrier.eAfter );
		}
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nNext;
	return SIR_VULKAN_SUCCESS;
}
