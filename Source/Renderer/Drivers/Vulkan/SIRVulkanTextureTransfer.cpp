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

sirVulkanResult_t SIRVulkanDevice::submitBufferImageCopy( sirVulkanBuffer_t *pBuffer, sirVulkanImage_t *pImage,
                                                          const sirVulkanTextureTransfer_t& transfer,
                                                          sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pImage == nullptr || pCompletion == nullptr || pBuffer->hBuffer == VK_NULL_HANDLE ||
	     pImage->hImage == VK_NULL_HANDLE || pBuffer->pOwnerAllocator != m_Allocator || pImage->pOwnerAllocator != m_Allocator ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial || pImage->nDeviceSerial != m_nDeviceSerial || m_nLastTransferValue == UINT64_MAX ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const bool bUpload = transfer.bBufferToImage;
	if ( bUpload &&
	     ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 || ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !bUpload &&
	     ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 || ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const sirVulkanResult_t eValidation = sirVulkanValidateTextureTransfer( pBuffer, pImage, transfer );
	if ( eValidation != SIR_VULKAN_SUCCESS ) {
		return eValidation;
	}
	VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( !sirVulkanGetSubresourceLayout( pImage, transfer.nMip, transfer.nLayer, &eOld ) ||
	     ( !bUpload && eOld == VK_IMAGE_LAYOUT_UNDEFINED ) ) {
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
	const VkResult eReset = m_pfnResetCommandBuffer( pSlot->hCommandBuffer, 0 );
	if ( eReset != VK_SUCCESS ) {
		m_bDeviceLost = eReset == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	const VkResult eBegin = m_pfnBeginCommandBuffer( pSlot->hCommandBuffer, &begin );
	if ( eBegin != VK_SUCCESS ) {
		m_bDeviceLost = eBegin == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	const VkImageLayout eTransfer = bUpload ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	VkImageMemoryBarrier2 before = {};
	before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	before.srcStageMask = eOld == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	before.srcAccessMask =
	    eOld == VK_IMAGE_LAYOUT_UNDEFINED ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	before.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	before.dstAccessMask = bUpload ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_TRANSFER_READ_BIT;
	before.oldLayout = eOld;
	before.newLayout = eTransfer;
	before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.image = pImage->hImage;
	before.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	before.subresourceRange.baseMipLevel = transfer.nMip;
	before.subresourceRange.levelCount = 1;
	before.subresourceRange.baseArrayLayer = transfer.nLayer;
	before.subresourceRange.layerCount = 1;

	VkMemoryBarrier2 host = {};
	host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	host.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	host.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	host.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	host.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &before;
	if ( bUpload ) {
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &host;
	}
	m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );

	VkBufferImageCopy2 region = {};
	region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
	region.bufferOffset = transfer.nBufferOffset;
	region.bufferRowLength = transfer.nBufferRowLength;
	region.bufferImageHeight = transfer.nBufferImageHeight;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = transfer.nMip;
	region.imageSubresource.baseArrayLayer = transfer.nLayer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = transfer.offset;
	region.imageExtent = transfer.extent;
	if ( bUpload ) {
		VkCopyBufferToImageInfo2 info = {};
		info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
		info.srcBuffer = pBuffer->hBuffer;
		info.dstImage = pImage->hImage;
		info.dstImageLayout = eTransfer;
		info.regionCount = 1;
		info.pRegions = &region;
		m_pfnCmdCopyBufferToImage2( pSlot->hCommandBuffer, &info );
	} else {
		VkCopyImageToBufferInfo2 info = {};
		info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2;
		info.srcImage = pImage->hImage;
		info.srcImageLayout = eTransfer;
		info.dstBuffer = pBuffer->hBuffer;
		info.regionCount = 1;
		info.pRegions = &region;
		m_pfnCmdCopyImageToBuffer2( pSlot->hCommandBuffer, &info );
	}

	const VkImageLayout eFinal = bUpload ? sirVulkanUploadFinalLayout( pImage, transfer ) : eOld;
	VkImageMemoryBarrier2 after = before;
	after.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	after.srcAccessMask = bUpload ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_TRANSFER_READ_BIT;
	after.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	after.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	after.oldLayout = eTransfer;
	after.newLayout = eFinal;
	dependency.memoryBarrierCount = 0;
	dependency.pMemoryBarriers = nullptr;
	dependency.pImageMemoryBarriers = &after;
	m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );
	const VkResult eEnd = m_pfnEndCommandBuffer( pSlot->hCommandBuffer );
	if ( eEnd != VK_SUCCESS ) {
		m_bDeviceLost = eEnd == VK_ERROR_DEVICE_LOST;
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
	pBuffer->nLastGpuUse = nNext;
	pImage->nLastGpuUse = nNext;
	// The single owner queue serializes submissions. Publish the final layout
	// only after vkQueueSubmit2 succeeds, never on recording/submit failure.
	( void )sirVulkanSetSubresourceLayout( pImage, transfer.nMip, transfer.nLayer, eFinal );
	if ( bUpload && sirVulkanTransferCoversSubresource( pImage, transfer ) ) {
		const uint32_t nIndex = transfer.nLayer * pImage->nMips + transfer.nMip;
		pImage->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nNext;
	return SIR_VULKAN_SUCCESS;
}
