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

#include <string.h>

static VkImageLayout sirVulkanRecordedUploadLayout( const sirVulkanImage_t *pImage ) noexcept
{
	if ( ( pImage->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) != 0 ) {
		return VK_IMAGE_LAYOUT_GENERAL;
	}
	return ( pImage->nUsage & VK_IMAGE_USAGE_SAMPLED_BIT ) != 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
}

static sirVulkanImage_t *sirVulkanRecordedShadow( sirVulkanImage_t *pImage, sirVulkanImage_t **ppOriginals, sirVulkanImage_t *pShadows,
                                                  uint32_t *pCount ) noexcept
{
	if ( pImage == nullptr || ppOriginals == nullptr || pShadows == nullptr || pCount == nullptr ) {
		return nullptr;
	}
	for ( uint32_t iImage = 0; iImage < *pCount; ++iImage ) {
		if ( ppOriginals[iImage] == pImage ) {
			return &pShadows[iImage];
		}
	}
	if ( *pCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return nullptr;
	}
	ppOriginals[*pCount] = pImage;
	pShadows[*pCount] = *pImage;
	return &pShadows[( *pCount )++];
}

static sirVulkanResult_t sirVulkanAdvanceRecordedState( sirVulkanImage_t *pShadow, const sirVulkanRecordedCommand_t &command ) noexcept
{
	if ( pShadow == nullptr ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( command.eKind == SIR_VULKAN_RECORDED_TEXTURE_COPY ) {
		const sirVulkanResult_t eCheck = sirVulkanValidateTextureTransfer( command.pBuffer, pShadow, command.textureCopy );
		if ( eCheck != SIR_VULKAN_SUCCESS ) {
			return eCheck;
		}
		VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
		if ( !sirVulkanGetSubresourceLayout( pShadow, command.textureCopy.nMip, command.textureCopy.nLayer, &eOld ) ||
		     ( !command.textureCopy.bBufferToImage && eOld == VK_IMAGE_LAYOUT_UNDEFINED ) ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
		if ( command.textureCopy.bBufferToImage ) {
			const uint32_t nIndex = command.textureCopy.nLayer * pShadow->nMips + command.textureCopy.nMip;
			pShadow->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
			( void )sirVulkanSetSubresourceLayout( pShadow, command.textureCopy.nMip, command.textureCopy.nLayer,
			                                       sirVulkanRecordedUploadLayout( pShadow ) );
		}
		return SIR_VULKAN_SUCCESS;
	}
	if ( command.eKind != SIR_VULKAN_RECORDED_IMAGE_BARRIER ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const sirVulkanImageBarrier_t &barrier = command.imageBarrier;
	VkImageLayout eTracked = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( !sirVulkanGetSubresourceLayout( pShadow, barrier.nMip, barrier.nLayer, &eTracked ) || eTracked != barrier.eBefore ||
	     ( barrier.eAfter != VK_IMAGE_LAYOUT_GENERAL && barrier.eAfter != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ||
	     ( barrier.bUav && ( barrier.eBefore != VK_IMAGE_LAYOUT_GENERAL || barrier.eAfter != VK_IMAGE_LAYOUT_GENERAL ||
	                         ( pShadow->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) == 0 ) ) ||
	     ( !barrier.bUav && barrier.eBefore == barrier.eAfter ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( barrier.eAfter == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		const uint32_t nIndex = barrier.nLayer * pShadow->nMips + barrier.nMip;
		if ( ( pShadow->nUsage & VK_IMAGE_USAGE_SAMPLED_BIT ) == 0 ||
		     ( pShadow->szUploadedSubresources[nIndex / 64] & ( 1ULL << ( nIndex % 64 ) ) ) == 0 ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
	}
	if ( pShadow->eFormat == VK_FORMAT_D16_UNORM || pShadow->eFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
	     pShadow->eFormat == VK_FORMAT_D32_SFLOAT ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	( void )sirVulkanSetSubresourceLayout( pShadow, barrier.nMip, barrier.nLayer, barrier.eAfter );
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t sirVulkanValidateRecordedCommands( const sirVulkanRecordedCommand_t *pCommands, uint32_t nCount ) noexcept
{
	if ( pCommands == nullptr || nCount == 0 || nCount > SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	sirVulkanImage_t *szOriginals[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	sirVulkanImage_t szShadows[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	uint32_t nImages = 0;
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t &command = pCommands[iCommand];
		if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			if ( sirVulkanValidateBufferCopies( &command.bufferCopy, 1 ) != SIR_VULKAN_SUCCESS ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			const sirVulkanBufferBarrier_t &barrier = command.bufferBarrier;
			if ( command.pBuffer == nullptr || command.pBuffer->nSize == 0 || barrier.nDestinationStages == VK_PIPELINE_STAGE_2_NONE ||
			     ( barrier.nSourceStages == VK_PIPELINE_STAGE_2_NONE && barrier.nSourceAccess != VK_ACCESS_2_NONE ) ||
			     ( barrier.bUav && ( ( command.pBuffer->nUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) == 0 ||
			                         ( barrier.nSourceAccess & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ) == 0 ||
			                         ( barrier.nDestinationAccess & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ) == 0 ) ) ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( command.eKind == SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			if ( command.pPipeline == nullptr || command.nGroupCountX == 0 || command.nGroupCountY == 0 || command.nGroupCountZ == 0 ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( command.eKind != SIR_VULKAN_RECORDED_TEXTURE_COPY && command.eKind != SIR_VULKAN_RECORDED_IMAGE_BARRIER ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
		sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( command.pImage, szOriginals, szShadows, &nImages );
		const sirVulkanResult_t eCheck = sirVulkanAdvanceRecordedState( pShadow, command );
		if ( eCheck != SIR_VULKAN_SUCCESS ) {
			return eCheck;
		}
	}
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::submitRecordedCommands( const sirVulkanRecordedCommand_t *pCommands, uint32_t nCount,
                                                           sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || m_nLastTransferValue == UINT64_MAX ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const sirVulkanResult_t eValidation = sirVulkanValidateRecordedCommands( pCommands, nCount );
	if ( eValidation != SIR_VULKAN_SUCCESS ) {
		return eValidation;
	}
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t &command = pCommands[iCommand];
		if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			if ( command.bufferCopy.pSource->hBuffer == VK_NULL_HANDLE || command.bufferCopy.pDestination->hBuffer == VK_NULL_HANDLE ||
			     command.bufferCopy.pSource->hBuffer == command.bufferCopy.pDestination->hBuffer ||
			     command.bufferCopy.pSource->pOwnerAllocator != m_Allocator ||
			     command.bufferCopy.pDestination->pOwnerAllocator != m_Allocator ||
			     command.bufferCopy.pSource->nDeviceSerial != m_nDeviceSerial ||
			     command.bufferCopy.pDestination->nDeviceSerial != m_nDeviceSerial ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
			}
		} else if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			if ( command.pBuffer->hBuffer == VK_NULL_HANDLE || command.pBuffer->pOwnerAllocator != m_Allocator ||
			     command.pBuffer->nDeviceSerial != m_nDeviceSerial ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
			}
		} else if ( command.eKind == SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			if ( !m_Adapter.bComputeCapable ) {
				return SIR_VULKAN_API_UNSUPPORTED;
			}
			if ( command.pPipeline->hPipeline == VK_NULL_HANDLE || command.pPipeline->nDeviceSerial != m_nDeviceSerial ||
			     command.nGroupCountX > m_Adapter.szMaximumComputeGroupCount[0] ||
			     command.nGroupCountY > m_Adapter.szMaximumComputeGroupCount[1] ||
			     command.nGroupCountZ > m_Adapter.szMaximumComputeGroupCount[2] ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
			}
		} else if ( command.pImage->hImage == VK_NULL_HANDLE || command.pImage->pOwnerAllocator != m_Allocator ||
		            command.pImage->nDeviceSerial != m_nDeviceSerial ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIR_VULKAN_RECORDED_TEXTURE_COPY &&
		            ( command.pBuffer == nullptr || command.pBuffer->hBuffer == VK_NULL_HANDLE ||
		              command.pBuffer->pOwnerAllocator != m_Allocator || command.pBuffer->nDeviceSerial != m_nDeviceSerial ) ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
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
	if ( pSlot == nullptr || m_pfnResetCommandBuffer( pSlot->hCommandBuffer, 0 ) != VK_SUCCESS ) {
		return pSlot == nullptr ? SIR_VULKAN_BUSY : SIR_VULKAN_NATIVE_FAILURE;
	}
	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ( m_pfnBeginCommandBuffer( pSlot->hCommandBuffer, &begin ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	sirVulkanImage_t *szOriginals[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	sirVulkanImage_t szShadows[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	uint32_t nImages = 0;
	VkMemoryBarrier2 memory = {};
	memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	memory.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	memory.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	memory.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	memory.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers = &memory;
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t &command = pCommands[iCommand];
		m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );
		if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			VkBufferCopy2 region = {};
			region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
			region.srcOffset = command.bufferCopy.nSourceOffset;
			region.dstOffset = command.bufferCopy.nDestinationOffset;
			region.size = command.bufferCopy.nBytes;
			VkCopyBufferInfo2 info = {};
			info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
			info.srcBuffer = command.bufferCopy.pSource->hBuffer;
			info.dstBuffer = command.bufferCopy.pDestination->hBuffer;
			info.regionCount = 1;
			info.pRegions = &region;
			m_pfnCmdCopyBuffer2( pSlot->hCommandBuffer, &info );
			continue;
		}
		if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			const sirVulkanBufferBarrier_t &barrier = command.bufferBarrier;
			VkBufferMemoryBarrier2 native = {};
			native.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			native.srcStageMask = barrier.nSourceStages;
			native.srcAccessMask = barrier.nSourceAccess;
			native.dstStageMask = barrier.nDestinationStages;
			native.dstAccessMask = barrier.nDestinationAccess;
			native.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			native.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			native.buffer = command.pBuffer->hBuffer;
			native.offset = 0;
			native.size = VK_WHOLE_SIZE;
			VkDependencyInfo bufferDependency = {};
			bufferDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			bufferDependency.bufferMemoryBarrierCount = 1;
			bufferDependency.pBufferMemoryBarriers = &native;
			m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &bufferDependency );
			continue;
		}
		if ( command.eKind == SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			m_pfnCmdBindPipeline( pSlot->hCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, command.pPipeline->hPipeline );
			m_pfnCmdDispatch( pSlot->hCommandBuffer, command.nGroupCountX, command.nGroupCountY, command.nGroupCountZ );
			continue;
		}
		sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( command.pImage, szOriginals, szShadows, &nImages );
		if ( pShadow == nullptr ) {
			return SIR_VULKAN_NATIVE_FAILURE;
		}
		if ( command.eKind == SIR_VULKAN_RECORDED_IMAGE_BARRIER ) {
			const sirVulkanImageBarrier_t &barrier = command.imageBarrier;
			VkImageMemoryBarrier2 imageBarrier = {};
			imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			imageBarrier.srcStageMask =
			    barrier.eBefore == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			imageBarrier.srcAccessMask = barrier.eBefore == VK_IMAGE_LAYOUT_UNDEFINED
			                                 ? VK_ACCESS_2_NONE
			                                 : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
			imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
			imageBarrier.oldLayout = barrier.eBefore;
			imageBarrier.newLayout = barrier.eAfter;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = command.pImage->hImage;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBarrier.subresourceRange.baseMipLevel = barrier.nMip;
			imageBarrier.subresourceRange.levelCount = 1;
			imageBarrier.subresourceRange.baseArrayLayer = barrier.nLayer;
			imageBarrier.subresourceRange.layerCount = 1;
			VkDependencyInfo imageDependency = {};
			imageDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			imageDependency.imageMemoryBarrierCount = 1;
			imageDependency.pImageMemoryBarriers = &imageBarrier;
			m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &imageDependency );
			( void )sirVulkanAdvanceRecordedState( pShadow, command );
			continue;
		}
		const sirVulkanTextureTransfer_t &copy = command.textureCopy;
		const bool bUpload = copy.bBufferToImage;
		VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
		( void )sirVulkanGetSubresourceLayout( pShadow, copy.nMip, copy.nLayer, &eOld );
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
		before.image = command.pImage->hImage;
		before.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		before.subresourceRange.baseMipLevel = copy.nMip;
		before.subresourceRange.levelCount = 1;
		before.subresourceRange.baseArrayLayer = copy.nLayer;
		before.subresourceRange.layerCount = 1;
		VkDependencyInfo imageDependency = {};
		imageDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		imageDependency.imageMemoryBarrierCount = 1;
		imageDependency.pImageMemoryBarriers = &before;
		m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &imageDependency );

		VkBufferImageCopy2 region = {};
		region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
		region.bufferOffset = copy.nBufferOffset;
		region.bufferRowLength = copy.nBufferRowLength;
		region.bufferImageHeight = copy.nBufferImageHeight;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = copy.nMip;
		region.imageSubresource.baseArrayLayer = copy.nLayer;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = copy.offset;
		region.imageExtent = copy.extent;
		if ( bUpload ) {
			VkCopyBufferToImageInfo2 info = {};
			info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
			info.srcBuffer = command.pBuffer->hBuffer;
			info.dstImage = command.pImage->hImage;
			info.dstImageLayout = eTransfer;
			info.regionCount = 1;
			info.pRegions = &region;
			m_pfnCmdCopyBufferToImage2( pSlot->hCommandBuffer, &info );
		} else {
			VkCopyImageToBufferInfo2 info = {};
			info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2;
			info.srcImage = command.pImage->hImage;
			info.srcImageLayout = eTransfer;
			info.dstBuffer = command.pBuffer->hBuffer;
			info.regionCount = 1;
			info.pRegions = &region;
			m_pfnCmdCopyImageToBuffer2( pSlot->hCommandBuffer, &info );
		}
		VkImageMemoryBarrier2 after = before;
		after.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		after.srcAccessMask = bUpload ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_TRANSFER_READ_BIT;
		after.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		after.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		after.oldLayout = eTransfer;
		after.newLayout = bUpload ? sirVulkanRecordedUploadLayout( pShadow ) : eOld;
		imageDependency.pImageMemoryBarriers = &after;
		m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &imageDependency );
		( void )sirVulkanAdvanceRecordedState( pShadow, command );
	}
	if ( m_pfnEndCommandBuffer( pSlot->hCommandBuffer ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	const uint64_t nNext = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo cmd = {};
	cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd.commandBuffer = pSlot->hCommandBuffer;
	VkSemaphoreSubmitInfo signal = {};
	signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal.semaphore = m_TransferTimeline;
	signal.value = nNext;
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.commandBufferInfoCount = 1;
	submit.pCommandBufferInfos = &cmd;
	submit.signalSemaphoreInfoCount = 1;
	submit.pSignalSemaphoreInfos = &signal;
	const VkResult eSubmit = m_pfnQueueSubmit2( m_GraphicsQueue, 1, &submit, VK_NULL_HANDLE );
	if ( eSubmit != VK_SUCCESS ) {
		m_bDeviceLost = eSubmit == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = nNext;
	pSlot->nLastValue = nNext;
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t &command = pCommands[iCommand];
		if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			command.bufferCopy.pSource->nLastGpuUse = nNext;
			command.bufferCopy.pDestination->nLastGpuUse = nNext;
		} else if ( command.eKind == SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			command.pBuffer->nLastGpuUse = nNext;
		} else if ( command.eKind == SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			command.pPipeline->nLastGpuUse = nNext;
		} else if ( command.eKind == SIR_VULKAN_RECORDED_TEXTURE_COPY ) {
			command.pBuffer->nLastGpuUse = nNext;
			command.pImage->nLastGpuUse = nNext;
		} else {
			command.pImage->nLastGpuUse = nNext;
		}
	}
	for ( uint32_t iImage = 0; iImage < nImages; ++iImage ) {
		memcpy( szOriginals[iImage]->szSubresourceLayouts, szShadows[iImage].szSubresourceLayouts,
		        sizeof( szShadows[iImage].szSubresourceLayouts ) );
		memcpy( szOriginals[iImage]->szUploadedSubresources, szShadows[iImage].szUploadedSubresources,
		        sizeof( szShadows[iImage].szUploadedSubresources ) );
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nNext;
	return SIR_VULKAN_SUCCESS;
}
