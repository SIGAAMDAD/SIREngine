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

#include <string.h>

// The Vulkan 1.4 headless device owns a single graphics queue. The eight
// preallocated command buffers avoid native command-buffer allocation in the
// warmed transfer path. There is intentionally no second CPU submission owner.
bool SIRVulkanDevice::initializeTransferQueue( void ) noexcept
{
	if ( m_Procs.vkDeviceWaitIdle == nullptr || m_Procs.vkCreateCommandPool == nullptr || m_Procs.vkDestroyCommandPool == nullptr ||
	     m_Procs.vkAllocateCommandBuffers == nullptr || m_Procs.vkResetCommandBuffer == nullptr || m_Procs.vkBeginCommandBuffer == nullptr ||
	     m_Procs.vkEndCommandBuffer == nullptr || m_Procs.vkCmdPipelineBarrier2 == nullptr || m_Procs.vkCmdCopyBuffer2 == nullptr ||
	     m_Procs.vkCmdBindPipeline == nullptr || m_Procs.vkCmdDispatch == nullptr || m_Procs.vkCmdCopyBufferToImage2 == nullptr ||
	     m_Procs.vkCmdCopyImageToBuffer2 == nullptr || m_Procs.vkQueueSubmit2 == nullptr || m_Procs.vkCreateSemaphore == nullptr ||
	     m_Procs.vkDestroySemaphore == nullptr || m_Procs.vkGetSemaphoreCounterValue == nullptr || m_Procs.vkWaitSemaphores == nullptr ) {
		return false;
	}

	VkCommandPoolCreateInfo poolInfo {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = m_Adapter.nQueueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if ( m_Procs.vkCreateCommandPool( m_Device, &poolInfo, nullptr, &m_TransferCommandPool ) != VK_SUCCESS ) {
		return false;
	}

	VkCommandBuffer hBuffers[SIR_VULKAN_TRANSFER_SLOT_COUNT];
	memset( hBuffers, 0, sizeof( hBuffers ) );
	VkCommandBufferAllocateInfo allocationInfo {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocationInfo.commandPool = m_TransferCommandPool;
	allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocationInfo.commandBufferCount = SIR_VULKAN_TRANSFER_SLOT_COUNT;
	if ( m_Procs.vkAllocateCommandBuffers( m_Device, &allocationInfo, hBuffers ) != VK_SUCCESS ) {
		return false;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
		m_TransferSlots[iSlot].hCommandBuffer = hBuffers[iSlot];
		m_TransferSlots[iSlot].nLastValue = 0;
	}

	VkSemaphoreTypeCreateInfo timelineInfo {};
	timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineInfo.initialValue = 0;
	VkSemaphoreCreateInfo semaphoreInfo {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = &timelineInfo;
	return m_Procs.vkCreateSemaphore( m_Device, &semaphoreInfo, nullptr, &m_TransferTimeline ) == VK_SUCCESS;
}

void SIRVulkanDevice::destroyTransferQueue( void ) noexcept
{
	if ( m_Device == VK_NULL_HANDLE ) {
		return;
	}
	if ( m_TransferTimeline != VK_NULL_HANDLE && m_Procs.vkDestroySemaphore != nullptr ) {
		m_Procs.vkDestroySemaphore( m_Device, m_TransferTimeline, nullptr );
		m_TransferTimeline = VK_NULL_HANDLE;
	}
	if ( m_TransferCommandPool != VK_NULL_HANDLE && m_Procs.vkDestroyCommandPool != nullptr ) {
		m_Procs.vkDestroyCommandPool( m_Device, m_TransferCommandPool, nullptr );
		m_TransferCommandPool = VK_NULL_HANDLE;
	}
	memset( m_TransferSlots, 0, sizeof( m_TransferSlots ) );
	m_iNextTransferSlot = 0;
}

bool SIRVulkanDevice::isBufferIdle( const sirVulkanBuffer_t *pBuffer ) const noexcept
{
	if ( pBuffer == nullptr || pBuffer->nDeviceSerial != m_nDeviceSerial || m_bDeviceLost ) {
		return false;
	}
	if ( pBuffer->nLastGpuUse == 0 ) {
		return true;
	}
	uint64_t nCompleted = 0;
	return m_Procs.vkGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted ) == VK_SUCCESS && nCompleted >= pBuffer->nLastGpuUse;
}

SIRVulkanResult SIRVulkanDevice::queryCompletion( const sirVulkanCompletion_t *pCompletion, bool *pComplete ) const noexcept
{
	if ( pComplete != nullptr ) {
		*pComplete = false;
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || pComplete == nullptr || pCompletion->nDeviceSerial != m_nDeviceSerial || pCompletion->nValue == 0 ||
	     pCompletion->nValue > m_nLastTransferValue ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	uint64_t nCompleted = 0;
	const VkResult eResult = m_Procs.vkGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted );
	if ( eResult != VK_SUCCESS ) {
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	*pComplete = nCompleted >= pCompletion->nValue;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::waitCompletion( const sirVulkanCompletion_t *pCompletion, uint64_t nTimeoutNs ) const noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || pCompletion->nDeviceSerial != m_nDeviceSerial || pCompletion->nValue == 0 ||
	     pCompletion->nValue > m_nLastTransferValue ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}

	VkSemaphoreWaitInfo waitInfo {};
	waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores = &m_TransferTimeline;
	waitInfo.pValues = &pCompletion->nValue;
	const VkResult eResult = m_Procs.vkWaitSemaphores( m_Device, &waitInfo, nTimeoutNs );
	if ( eResult == VK_TIMEOUT ) {
		return SIRVulkanResult::SIR_VULKAN_TIMEOUT;
	}
	return eResult == VK_SUCCESS ? SIRVulkanResult::SIR_VULKAN_SUCCESS : SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
}

SIRVulkanResult SIRVulkanDevice::readBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, void *pData, uint64_t nBytes ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial || !pBuffer->bHostVisible || pData == nullptr || nBytes == 0 ||
	     nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) {
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}

	void *pMapped = nullptr;
	const SIRVulkanResult eMap = mapBuffer( pBuffer, nOffset, nBytes, &pMapped );
	if ( eMap != SIRVulkanResult::SIR_VULKAN_SUCCESS ) return eMap;
	const SIRVulkanResult eInvalidate = invalidateBuffer( pBuffer, nOffset, nBytes );
	if ( eInvalidate == SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		memcpy( pData, pMapped, static_cast<size_t>( nBytes ) );
	}
	const SIRVulkanResult eUnmap = unmapBuffer( pBuffer );
	return eInvalidate != SIRVulkanResult::SIR_VULKAN_SUCCESS ? eInvalidate : eUnmap;
}

SIRVulkanResult sirVulkanValidateBufferCopies( const sirVulkanBufferCopy_t *pCopies, uint32_t nCount ) noexcept
{
	if ( pCopies == nullptr || nCount == 0 || nCount > SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		const sirVulkanBufferCopy_t& copy = pCopies[iCopy];
		if ( copy.pSource == nullptr || copy.pDestination == nullptr || copy.pSource == copy.pDestination || copy.nBytes == 0 ||
		     ( copy.pSource->nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 ||
		     ( copy.pDestination->nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 || copy.nSourceOffset > copy.pSource->nSize ||
		     copy.nBytes > copy.pSource->nSize - copy.nSourceOffset || copy.nDestinationOffset > copy.pDestination->nSize ||
		     copy.nBytes > copy.pDestination->nSize - copy.nDestinationOffset ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
	}
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::submitBufferCopy( sirVulkanBuffer_t *pSource, uint64_t nSourceOffset, sirVulkanBuffer_t *pDestination,
                                                     uint64_t nDestinationOffset, uint64_t nBytes,
                                                     sirVulkanCompletion_t *pCompletion ) noexcept
{
	const sirVulkanBufferCopy_t copy = { pSource, pDestination, nSourceOffset, nDestinationOffset, nBytes };
	return submitBufferCopies( &copy, 1, pCompletion );
}

SIRVulkanResult SIRVulkanDevice::submitBufferCopies( const sirVulkanBufferCopy_t *pCopies, uint32_t nCount,
                                                       sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || m_nLastTransferValue == UINT64_MAX ||
	     sirVulkanValidateBufferCopies( pCopies, nCount ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	// Validate the entire batch before recording any native command. A failed
	// validation must neither submit partial work nor change completion state.
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		const sirVulkanBufferCopy_t& copy = pCopies[iCopy];
		if ( copy.pSource->hBuffer == VK_NULL_HANDLE || copy.pDestination->hBuffer == VK_NULL_HANDLE ||
		     copy.pSource->hBuffer == copy.pDestination->hBuffer || copy.pSource->pOwnerAllocator != m_Allocator ||
		     copy.pDestination->pOwnerAllocator != m_Allocator || copy.pSource->nDeviceSerial != m_nDeviceSerial ||
		     copy.pDestination->nDeviceSerial != m_nDeviceSerial ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
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

	VkCommandBufferBeginInfo beginInfo {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	const VkResult eBegin = m_Procs.vkBeginCommandBuffer( pSlot->hCommandBuffer, &beginInfo );
	if ( eBegin != VK_SUCCESS ) {
		m_bDeviceLost = eBegin == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	// Between each copy, order writes so a later copy can read an earlier
	// destination. This supports CPU upload -> GPU staging -> readback within
	// one native command buffer and one timeline value.
	VkMemoryBarrier2 barrier {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COPY_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
	VkDependencyInfo dependency {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers = &barrier;
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		const sirVulkanBufferCopy_t& copy = pCopies[iCopy];
		m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );
		VkBufferCopy2 region {};
		region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
		region.srcOffset = copy.nSourceOffset;
		region.dstOffset = copy.nDestinationOffset;
		region.size = copy.nBytes;
		VkCopyBufferInfo2 copyInfo {};
		copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
		copyInfo.srcBuffer = copy.pSource->hBuffer;
		copyInfo.dstBuffer = copy.pDestination->hBuffer;
		copyInfo.regionCount = 1;
		copyInfo.pRegions = &region;
		m_Procs.vkCmdCopyBuffer2( pSlot->hCommandBuffer, &copyInfo );
	}
	const VkResult eEnd = m_Procs.vkEndCommandBuffer( pSlot->hCommandBuffer );
	if ( eEnd != VK_SUCCESS ) {
		m_bDeviceLost = eEnd == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	const uint64_t nSignalValue = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo commandInfo {};
	commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandInfo.commandBuffer = pSlot->hCommandBuffer;
	VkSemaphoreSubmitInfo signalInfo {};
	signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfo.semaphore = m_TransferTimeline;
	signalInfo.value = nSignalValue;
	signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submitInfo {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalInfo;
	const VkResult eSubmit = m_Procs.vkQueueSubmit2( m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
	if ( eSubmit != VK_SUCCESS ) {
		m_bDeviceLost = eSubmit == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = nSignalValue;
	pSlot->nLastValue = nSignalValue;
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		pCopies[iCopy].pSource->nLastGpuUse = nSignalValue;
		pCopies[iCopy].pDestination->nLastGpuUse = nSignalValue;
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nSignalValue;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
