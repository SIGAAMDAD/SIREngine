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

SIRVulkanResult SIRVulkanDevice::submitBufferImageCopy( sirVulkanBuffer_t *pBuffer, sirVulkanImage_t *pImage,
                                                          const sirVulkanTextureTransfer_t& transfer,
                                                          sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pImage == nullptr || pCompletion == nullptr || pBuffer->hBuffer == VK_NULL_HANDLE ||
	     pImage->hImage == VK_NULL_HANDLE || pBuffer->pOwnerAllocator != m_Allocator || pImage->pOwnerAllocator != m_Allocator ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial || pImage->nDeviceSerial != m_nDeviceSerial || m_nLastTransferValue == UINT64_MAX ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const bool bUpload = transfer.bBufferToImage;
	const VkImageAspectFlags nAspect = sirVulkanImageAspectMask( pImage->eFormat );
	// Depth/stencil formats with two aspects require independent plane copies.
	// The current transfer contract describes one tightly packed plane only.
	if ( nAspect != VK_IMAGE_ASPECT_COLOR_BIT && nAspect != VK_IMAGE_ASPECT_DEPTH_BIT ) {
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}
	if ( bUpload &&
	     ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 || ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !bUpload &&
	     ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 || ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const SIRVulkanResult eValidation = sirVulkanValidateTextureTransfer( pBuffer, pImage, transfer );
	if ( eValidation != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return eValidation;
	}
	VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( !sirVulkanGetSubresourceLayout( pImage, transfer.nMip, transfer.nLayer, &eOld ) ||
	     ( !bUpload && eOld == VK_IMAGE_LAYOUT_UNDEFINED ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}

	uint64_t nCompleted = 0;
	const VkResult ePoll = m_Procs.vkGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted );
	if ( ePoll != VK_SUCCESS ) {
		m_bDeviceLost = ePoll == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
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
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	const VkResult eReset = m_Procs.vkResetCommandBuffer( pSlot->hCommandBuffer, 0 );
	if ( eReset != VK_SUCCESS ) {
		m_bDeviceLost = eReset == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	VkCommandBufferBeginInfo begin {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	const VkResult eBegin = m_Procs.vkBeginCommandBuffer( pSlot->hCommandBuffer, &begin );
	if ( eBegin != VK_SUCCESS ) {
		m_bDeviceLost = eBegin == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	const VkImageLayout eTransfer = bUpload ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	VkImageMemoryBarrier2 before {};
	before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	static_cast<void>(sirVulkanImageAccessScope( eOld, &before.srcStageMask, &before.srcAccessMask ));
	static_cast<void>(sirVulkanImageAccessScope( eTransfer, &before.dstStageMask, &before.dstAccessMask ));
	before.oldLayout = eOld;
	before.newLayout = eTransfer;
	before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.image = pImage->hImage;
	before.subresourceRange.aspectMask = nAspect;
	before.subresourceRange.baseMipLevel = transfer.nMip;
	before.subresourceRange.levelCount = 1;
	before.subresourceRange.baseArrayLayer = transfer.nLayer;
	before.subresourceRange.layerCount = 1;

	VkMemoryBarrier2 host {};
	host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	host.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	host.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	host.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	host.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	VkDependencyInfo dependency {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &before;
	if ( bUpload ) {
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &host;
	}
	m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );

	VkBufferImageCopy2 region {};
	region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
	region.bufferOffset = transfer.nBufferOffset;
	region.bufferRowLength = transfer.nBufferRowLength;
	region.bufferImageHeight = transfer.nBufferImageHeight;
	region.imageSubresource.aspectMask = nAspect;
	region.imageSubresource.mipLevel = transfer.nMip;
	region.imageSubresource.baseArrayLayer = transfer.nLayer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = transfer.offset;
	region.imageExtent = transfer.extent;
	if ( bUpload ) {
		VkCopyBufferToImageInfo2 info {};
		info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
		info.srcBuffer = pBuffer->hBuffer;
		info.dstImage = pImage->hImage;
		info.dstImageLayout = eTransfer;
		info.regionCount = 1;
		info.pRegions = &region;
		m_Procs.vkCmdCopyBufferToImage2( pSlot->hCommandBuffer, &info );
	} else {
		VkCopyImageToBufferInfo2 info {};
		info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2;
		info.srcImage = pImage->hImage;
		info.srcImageLayout = eTransfer;
		info.dstBuffer = pBuffer->hBuffer;
		info.regionCount = 1;
		info.pRegions = &region;
		m_Procs.vkCmdCopyImageToBuffer2( pSlot->hCommandBuffer, &info );
	}

	const VkImageLayout eFinal = bUpload ? sirVulkanUploadFinalLayout( pImage, transfer ) : eOld;
	VkImageMemoryBarrier2 after = before;
	static_cast<void>(sirVulkanImageAccessScope( eTransfer, &after.srcStageMask, &after.srcAccessMask ));
	static_cast<void>(sirVulkanImageAccessScope( eFinal, &after.dstStageMask, &after.dstAccessMask ));
	after.oldLayout = eTransfer;
	after.newLayout = eFinal;
	dependency.memoryBarrierCount = 0;
	dependency.pMemoryBarriers = nullptr;
	dependency.pImageMemoryBarriers = &after;
	m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );
	const VkResult eEnd = m_Procs.vkEndCommandBuffer( pSlot->hCommandBuffer );
	if ( eEnd != VK_SUCCESS ) {
		m_bDeviceLost = eEnd == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	const uint64_t nNext = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo command {};
	command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	command.commandBuffer = pSlot->hCommandBuffer;
	VkSemaphoreSubmitInfo signal {};
	signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal.semaphore = m_TransferTimeline;
	signal.value = nNext;
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submission {};
	submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submission.commandBufferInfoCount = 1;
	submission.pCommandBufferInfos = &command;
	submission.signalSemaphoreInfoCount = 1;
	submission.pSignalSemaphoreInfos = &signal;
	const VkResult eSubmit = m_Procs.vkQueueSubmit2( m_GraphicsQueue, 1, &submission, VK_NULL_HANDLE );
	if ( eSubmit != VK_SUCCESS ) {
		m_bDeviceLost = eSubmit == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = nNext;
	pSlot->nLastValue = nNext;
	pBuffer->nLastGpuUse = nNext;
	pImage->nLastGpuUse = nNext;
	// The single owner queue serializes submissions. Publish the final layout
	// only after vkQueueSubmit2 succeeds, never on recording/submit failure.
	static_cast<void>(sirVulkanSetSubresourceLayout( pImage, transfer.nMip, transfer.nLayer, eFinal ));
	if ( bUpload && sirVulkanTransferCoversSubresource( pImage, transfer ) ) {
		const uint32_t nIndex = transfer.nLayer * pImage->nMips + transfer.nMip;
		pImage->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nNext;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}


SIRVulkanResult SIRVulkanDevice::submitImageCopy( const sirVulkanImageCopy_t& copy, sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) *pCompletion = {};
	if ( !m_bReady || m_bDeviceLost ) return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	if ( pCompletion == nullptr || copy.pSource == nullptr || copy.pDestination == nullptr || copy.pSource == copy.pDestination ||
	     copy.pSource->hImage == VK_NULL_HANDLE || copy.pDestination->hImage == VK_NULL_HANDLE || copy.pSource->pOwnerAllocator != m_Allocator ||
	     copy.pDestination->pOwnerAllocator != m_Allocator || copy.pSource->nDeviceSerial != m_nDeviceSerial ||
	     copy.pDestination->nDeviceSerial != m_nDeviceSerial || copy.pSource->eFormat != copy.pDestination->eFormat ||
	     ( copy.pSource->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 || ( copy.pDestination->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ||
	     m_nLastTransferValue == UINT64_MAX || m_Procs.vkCmdCopyImage2 == nullptr ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;

	VkImageCopy2 region = copy.region;
	region.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
	const VkImageAspectFlags imageAspects = sirVulkanImageAspectMask( copy.pSource->eFormat );
	if ( region.srcSubresource.aspectMask == 0 ) region.srcSubresource.aspectMask = imageAspects;
	if ( region.dstSubresource.aspectMask == 0 ) region.dstSubresource.aspectMask = imageAspects;
	if ( region.srcSubresource.aspectMask != region.dstSubresource.aspectMask ||
	     ( region.srcSubresource.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT && region.srcSubresource.aspectMask != VK_IMAGE_ASPECT_DEPTH_BIT ) ||
	     region.extent.width == 0 || region.extent.height == 0 || region.extent.depth == 0 ||
	     region.srcSubresource.mipLevel >= copy.pSource->nMips || region.dstSubresource.mipLevel >= copy.pDestination->nMips ||
	     region.srcSubresource.baseArrayLayer >= copy.pSource->nLayers || region.dstSubresource.baseArrayLayer >= copy.pDestination->nLayers ||
	     region.srcSubresource.layerCount != 1 || region.dstSubresource.layerCount != 1 ||
	     region.srcSubresource.layerCount != region.dstSubresource.layerCount ||
	     region.srcSubresource.layerCount > copy.pSource->nLayers - region.srcSubresource.baseArrayLayer ||
	     region.dstSubresource.layerCount > copy.pDestination->nLayers - region.dstSubresource.baseArrayLayer ||
	     ( copy.pSource->eType != VK_IMAGE_TYPE_2D && copy.pSource->eType != VK_IMAGE_TYPE_3D ) ||
	     ( copy.pDestination->eType != VK_IMAGE_TYPE_2D && copy.pDestination->eType != VK_IMAGE_TYPE_3D ) ||
	     ( copy.pSource->eType == VK_IMAGE_TYPE_3D && ( copy.pSource->nLayers != 1 || region.srcSubresource.baseArrayLayer != 0 ) ) ||
	     ( copy.pDestination->eType == VK_IMAGE_TYPE_3D && ( copy.pDestination->nLayers != 1 || region.dstSubresource.baseArrayLayer != 0 ) ) ||
	     ( copy.pSource->eType == VK_IMAGE_TYPE_2D && ( region.extent.depth != 1 || region.srcOffset.z != 0 || region.dstOffset.z != 0 ) ) ||
	     ( copy.pDestination->eType == VK_IMAGE_TYPE_2D && ( region.extent.depth != 1 || region.srcOffset.z != 0 || region.dstOffset.z != 0 ) ) ||
	     ( copy.pSource->eType == VK_IMAGE_TYPE_3D && ( region.srcOffset.z < 0 || region.dstOffset.z < 0 ) ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t sourceWidth = std::max( 1U, copy.pSource->nWidth >> region.srcSubresource.mipLevel );
	const uint32_t sourceHeight = std::max( 1U, copy.pSource->nHeight >> region.srcSubresource.mipLevel );
	const uint32_t sourceDepth = copy.pSource->eType == VK_IMAGE_TYPE_3D ? std::max( 1U, copy.pSource->nDepth >> region.srcSubresource.mipLevel ) : 1U;
	const uint32_t destinationWidth = std::max( 1U, copy.pDestination->nWidth >> region.dstSubresource.mipLevel );
	const uint32_t destinationHeight = std::max( 1U, copy.pDestination->nHeight >> region.dstSubresource.mipLevel );
	const uint32_t destinationDepth = copy.pDestination->eType == VK_IMAGE_TYPE_3D ? std::max( 1U, copy.pDestination->nDepth >> region.dstSubresource.mipLevel ) : 1U;
	if ( region.srcOffset.x < 0 || region.srcOffset.y < 0 || region.srcOffset.z < 0 || region.dstOffset.x < 0 || region.dstOffset.y < 0 ||
	     region.dstOffset.z < 0 || static_cast<uint32_t>( region.srcOffset.x ) >= sourceWidth || static_cast<uint32_t>( region.srcOffset.y ) >= sourceHeight ||
	     static_cast<uint32_t>( region.srcOffset.z ) >= sourceDepth || static_cast<uint32_t>( region.dstOffset.x ) >= destinationWidth ||
	     static_cast<uint32_t>( region.dstOffset.y ) >= destinationHeight || static_cast<uint32_t>( region.dstOffset.z ) >= destinationDepth ||
	     region.extent.width > sourceWidth - static_cast<uint32_t>( region.srcOffset.x ) || region.extent.height > sourceHeight - static_cast<uint32_t>( region.srcOffset.y ) ||
	     region.extent.depth > sourceDepth - static_cast<uint32_t>( region.srcOffset.z ) || region.extent.width > destinationWidth - static_cast<uint32_t>( region.dstOffset.x ) ||
	     region.extent.height > destinationHeight - static_cast<uint32_t>( region.dstOffset.y ) || region.extent.depth > destinationDepth - static_cast<uint32_t>( region.dstOffset.z ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t sourceIndex = region.srcSubresource.baseArrayLayer * copy.pSource->nMips + region.srcSubresource.mipLevel;
	if ( ( copy.pSource->szUploadedSubresources[sourceIndex / 64] & ( 1ULL << ( sourceIndex % 64 ) ) ) == 0 ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	VkImageLayout sourceOld = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout destinationOld = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( !sirVulkanGetSubresourceLayout( copy.pSource, region.srcSubresource.mipLevel, region.srcSubresource.baseArrayLayer, &sourceOld ) ||
	     !sirVulkanGetSubresourceLayout( copy.pDestination, region.dstSubresource.mipLevel, region.dstSubresource.baseArrayLayer, &destinationOld ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( sourceOld == VK_IMAGE_LAYOUT_UNDEFINED ) return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;

	uint64_t completed = 0;
	if ( m_Procs.vkGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &completed ) != VK_SUCCESS ) return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	sirVulkanTransferSlot_t *slot = nullptr;
	for ( uint32_t iCheck = 0; iCheck < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iCheck ) {
		const uint32_t iSlot = ( m_iNextTransferSlot + iCheck ) % SIR_VULKAN_TRANSFER_SLOT_COUNT;
		if ( m_TransferSlots[iSlot].nLastValue <= completed ) {
			slot = &m_TransferSlots[iSlot];
			m_iNextTransferSlot = ( iSlot + 1 ) % SIR_VULKAN_TRANSFER_SLOT_COUNT;
			break;
		}
	}
	if ( slot == nullptr ) return SIRVulkanResult::SIR_VULKAN_BUSY;
	if ( m_Procs.vkResetCommandBuffer( slot->hCommandBuffer, 0 ) != VK_SUCCESS ) return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	VkCommandBufferBeginInfo begin{};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ( m_Procs.vkBeginCommandBuffer( slot->hCommandBuffer, &begin ) != VK_SUCCESS ) return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;

	VkImageMemoryBarrier2 barriers[2]{};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	static_cast<void>( sirVulkanImageAccessScope( sourceOld, &barriers[0].srcStageMask, &barriers[0].srcAccessMask ) );
	static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &barriers[0].dstStageMask, &barriers[0].dstAccessMask ) );
	barriers[0].oldLayout = sourceOld;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = copy.pSource->hImage;
	barriers[0].subresourceRange = { region.srcSubresource.aspectMask, region.srcSubresource.mipLevel, 1, region.srcSubresource.baseArrayLayer, region.srcSubresource.layerCount };
	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	static_cast<void>( sirVulkanImageAccessScope( destinationOld, &barriers[1].srcStageMask, &barriers[1].srcAccessMask ) );
	static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &barriers[1].dstStageMask, &barriers[1].dstAccessMask ) );
	barriers[1].oldLayout = destinationOld;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = copy.pDestination->hImage;
	barriers[1].subresourceRange = { region.dstSubresource.aspectMask, region.dstSubresource.mipLevel, 1, region.dstSubresource.baseArrayLayer, region.dstSubresource.layerCount };
	VkDependencyInfo dependency{};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 2;
	dependency.pImageMemoryBarriers = barriers;
	m_Procs.vkCmdPipelineBarrier2( slot->hCommandBuffer, &dependency );
	VkCopyImageInfo2 info{};
	info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
	info.srcImage = copy.pSource->hImage;
	info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	info.dstImage = copy.pDestination->hImage;
	info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	info.regionCount = 1;
	info.pRegions = &region;
	m_Procs.vkCmdCopyImage2( slot->hCommandBuffer, &info );

	const bool destinationWasInitialized = destinationOld != VK_IMAGE_LAYOUT_UNDEFINED;
	const VkImageLayout destinationFinal = destinationWasInitialized ? destinationOld :
		sirVulkanUploadFinalLayout( copy.pDestination, { 0, 0, 0, region.dstSubresource.mipLevel, region.dstSubresource.baseArrayLayer, region.dstOffset, region.extent, true } );
	VkImageMemoryBarrier2 after[2] = { barriers[0], barriers[1] };
	static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &after[0].srcStageMask, &after[0].srcAccessMask ) );
	static_cast<void>( sirVulkanImageAccessScope( sourceOld, &after[0].dstStageMask, &after[0].dstAccessMask ) );
	after[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	after[0].newLayout = sourceOld;
	static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &after[1].srcStageMask, &after[1].srcAccessMask ) );
	static_cast<void>( sirVulkanImageAccessScope( destinationFinal, &after[1].dstStageMask, &after[1].dstAccessMask ) );
	after[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	after[1].newLayout = destinationFinal;
	dependency.pImageMemoryBarriers = after;
	m_Procs.vkCmdPipelineBarrier2( slot->hCommandBuffer, &dependency );
	if ( m_Procs.vkEndCommandBuffer( slot->hCommandBuffer ) != VK_SUCCESS ) return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	const uint64_t next = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo command{};
	command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	command.commandBuffer = slot->hCommandBuffer;
	VkSemaphoreSubmitInfo signal{};
	signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal.semaphore = m_TransferTimeline;
	signal.value = next;
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submission{};
	submission.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submission.commandBufferInfoCount = 1;
	submission.pCommandBufferInfos = &command;
	submission.signalSemaphoreInfoCount = 1;
	submission.pSignalSemaphoreInfos = &signal;
	const VkResult submit = m_Procs.vkQueueSubmit2( m_GraphicsQueue, 1, &submission, VK_NULL_HANDLE );
	if ( submit != VK_SUCCESS ) {
		m_bDeviceLost = submit == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = next;
	slot->nLastValue = next;
	copy.pSource->nLastGpuUse = next;
	copy.pDestination->nLastGpuUse = next;
	static_cast<void>( sirVulkanSetSubresourceLayout( copy.pSource, region.srcSubresource.mipLevel, region.srcSubresource.baseArrayLayer, sourceOld ) );
	static_cast<void>( sirVulkanSetSubresourceLayout( copy.pDestination, region.dstSubresource.mipLevel, region.dstSubresource.baseArrayLayer, destinationFinal ) );
	if ( sirVulkanTransferCoversSubresource( copy.pDestination, { 0, 0, 0, region.dstSubresource.mipLevel, region.dstSubresource.baseArrayLayer, region.dstOffset, region.extent, true } ) ) {
		const uint32_t nIndex = region.dstSubresource.baseArrayLayer * copy.pDestination->nMips + region.dstSubresource.mipLevel;
		copy.pDestination->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = next;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
