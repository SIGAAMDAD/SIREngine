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

// The Vulkan 1.4 headless device owns a single graphics queue. The eight
// preallocated command buffers avoid native command-buffer allocation in the
// warmed transfer path. There is intentionally no second CPU submission owner.
bool SIRVulkanDevice::initializeTransferQueue( void ) noexcept
{
#define SIR_LOAD_DEVICE_PROC( Name ) m_pfn##Name = ( PFN_vk##Name )m_pfnGetDeviceProcAddr( m_Device, "vk" #Name )
	SIR_LOAD_DEVICE_PROC( DeviceWaitIdle );
	SIR_LOAD_DEVICE_PROC( CreateCommandPool );
	SIR_LOAD_DEVICE_PROC( DestroyCommandPool );
	SIR_LOAD_DEVICE_PROC( AllocateCommandBuffers );
	SIR_LOAD_DEVICE_PROC( ResetCommandBuffer );
	SIR_LOAD_DEVICE_PROC( BeginCommandBuffer );
	SIR_LOAD_DEVICE_PROC( EndCommandBuffer );
	SIR_LOAD_DEVICE_PROC( CmdPipelineBarrier2 );
	SIR_LOAD_DEVICE_PROC( CmdCopyBuffer2 );
	SIR_LOAD_DEVICE_PROC( CmdCopyBufferToImage2 );
	SIR_LOAD_DEVICE_PROC( CmdCopyImageToBuffer2 );
	SIR_LOAD_DEVICE_PROC( QueueSubmit2 );
	SIR_LOAD_DEVICE_PROC( CreateSemaphore );
	SIR_LOAD_DEVICE_PROC( DestroySemaphore );
	SIR_LOAD_DEVICE_PROC( GetSemaphoreCounterValue );
	SIR_LOAD_DEVICE_PROC( WaitSemaphores );
#undef SIR_LOAD_DEVICE_PROC

	if ( m_pfnDeviceWaitIdle == nullptr || m_pfnCreateCommandPool == nullptr || m_pfnDestroyCommandPool == nullptr ||
	     m_pfnAllocateCommandBuffers == nullptr || m_pfnResetCommandBuffer == nullptr || m_pfnBeginCommandBuffer == nullptr ||
	     m_pfnEndCommandBuffer == nullptr || m_pfnCmdPipelineBarrier2 == nullptr || m_pfnCmdCopyBuffer2 == nullptr ||
	     m_pfnCmdCopyBufferToImage2 == nullptr || m_pfnCmdCopyImageToBuffer2 == nullptr || m_pfnQueueSubmit2 == nullptr ||
	     m_pfnCreateSemaphore == nullptr || m_pfnDestroySemaphore == nullptr || m_pfnGetSemaphoreCounterValue == nullptr ||
	     m_pfnWaitSemaphores == nullptr ) {
		return false;
	}

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = m_Adapter.nQueueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if ( m_pfnCreateCommandPool( m_Device, &poolInfo, nullptr, &m_TransferCommandPool ) != VK_SUCCESS ) {
		return false;
	}

	VkCommandBuffer hBuffers[SIR_VULKAN_TRANSFER_SLOT_COUNT] = {};
	VkCommandBufferAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocationInfo.commandPool = m_TransferCommandPool;
	allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocationInfo.commandBufferCount = SIR_VULKAN_TRANSFER_SLOT_COUNT;
	if ( m_pfnAllocateCommandBuffers( m_Device, &allocationInfo, hBuffers ) != VK_SUCCESS ) {
		return false;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
		m_TransferSlots[iSlot].hCommandBuffer = hBuffers[iSlot];
		m_TransferSlots[iSlot].nLastValue = 0;
	}

	VkSemaphoreTypeCreateInfo timelineInfo = {};
	timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineInfo.initialValue = 0;
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = &timelineInfo;
	return m_pfnCreateSemaphore( m_Device, &semaphoreInfo, nullptr, &m_TransferTimeline ) == VK_SUCCESS;
}

void SIRVulkanDevice::destroyTransferQueue( void ) noexcept
{
	if ( m_Device == VK_NULL_HANDLE ) {
		return;
	}
	if ( m_TransferTimeline != VK_NULL_HANDLE && m_pfnDestroySemaphore != nullptr ) {
		m_pfnDestroySemaphore( m_Device, m_TransferTimeline, nullptr );
		m_TransferTimeline = VK_NULL_HANDLE;
	}
	if ( m_TransferCommandPool != VK_NULL_HANDLE && m_pfnDestroyCommandPool != nullptr ) {
		m_pfnDestroyCommandPool( m_Device, m_TransferCommandPool, nullptr );
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
	return m_pfnGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted ) == VK_SUCCESS && nCompleted >= pBuffer->nLastGpuUse;
}

sirVulkanResult_t SIRVulkanDevice::queryCompletion( const sirVulkanCompletion_t *pCompletion, bool *pComplete ) const noexcept
{
	if ( pComplete != nullptr ) {
		*pComplete = false;
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || pComplete == nullptr || pCompletion->nDeviceSerial != m_nDeviceSerial || pCompletion->nValue == 0 ||
	     pCompletion->nValue > m_nLastTransferValue ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	uint64_t nCompleted = 0;
	const VkResult eResult = m_pfnGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted );
	if ( eResult != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	*pComplete = nCompleted >= pCompletion->nValue;
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::waitCompletion( const sirVulkanCompletion_t *pCompletion, uint64_t nTimeoutNs ) const noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || pCompletion->nDeviceSerial != m_nDeviceSerial || pCompletion->nValue == 0 ||
	     pCompletion->nValue > m_nLastTransferValue ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	VkSemaphoreWaitInfo waitInfo = {};
	waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores = &m_TransferTimeline;
	waitInfo.pValues = &pCompletion->nValue;
	const VkResult eResult = m_pfnWaitSemaphores( m_Device, &waitInfo, nTimeoutNs );
	if ( eResult == VK_TIMEOUT ) {
		return SIR_VULKAN_TIMEOUT;
	}
	return eResult == VK_SUCCESS ? SIR_VULKAN_SUCCESS : SIR_VULKAN_NATIVE_FAILURE;
}

sirVulkanResult_t SIRVulkanDevice::readBuffer( sirVulkanBuffer_t *pBuffer, uint64_t nOffset, void *pData, uint64_t nBytes ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pBuffer == nullptr || pBuffer->pAllocation == nullptr || pBuffer->pOwnerAllocator != m_Allocator ||
	     pBuffer->nDeviceSerial != m_nDeviceSerial || !pBuffer->bHostVisible || pData == nullptr || nBytes == 0 ||
	     nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !isBufferIdle( pBuffer ) ) {
		return SIR_VULKAN_BUSY;
	}

	void *pMapped = nullptr;
	if ( vmaMapMemory( m_Allocator, pBuffer->pAllocation, &pMapped ) != VK_SUCCESS || pMapped == nullptr ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	const VkResult eResult = vmaInvalidateAllocation( m_Allocator, pBuffer->pAllocation, nOffset, nBytes );
	if ( eResult == VK_SUCCESS ) {
		memcpy( pData, ( const uint8_t * )pMapped + nOffset, ( size_t )nBytes );
	}
	vmaUnmapMemory( m_Allocator, pBuffer->pAllocation );
	return eResult == VK_SUCCESS ? SIR_VULKAN_SUCCESS : SIR_VULKAN_NATIVE_FAILURE;
}

sirVulkanResult_t sirVulkanValidateBufferCopies( const sirVulkanBufferCopy_t *pCopies, uint32_t nCount ) noexcept
{
	if ( pCopies == nullptr || nCount == 0 || nCount > SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		const sirVulkanBufferCopy_t &copy = pCopies[iCopy];
		if ( copy.pSource == nullptr || copy.pDestination == nullptr || copy.pSource == copy.pDestination || copy.nBytes == 0 ||
		     ( copy.pSource->nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 ||
		     ( copy.pDestination->nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 || copy.nSourceOffset > copy.pSource->nSize ||
		     copy.nBytes > copy.pSource->nSize - copy.nSourceOffset || copy.nDestinationOffset > copy.pDestination->nSize ||
		     copy.nBytes > copy.pDestination->nSize - copy.nDestinationOffset ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
	}
	return SIR_VULKAN_SUCCESS;
}

sirVulkanResult_t SIRVulkanDevice::submitBufferCopy( sirVulkanBuffer_t *pSource, uint64_t nSourceOffset, sirVulkanBuffer_t *pDestination,
                                                     uint64_t nDestinationOffset, uint64_t nBytes,
                                                     sirVulkanCompletion_t *pCompletion ) noexcept
{
	const sirVulkanBufferCopy_t copy = { pSource, pDestination, nSourceOffset, nDestinationOffset, nBytes };
	return submitBufferCopies( &copy, 1, pCompletion );
}

sirVulkanResult_t SIRVulkanDevice::submitBufferCopies( const sirVulkanBufferCopy_t *pCopies, uint32_t nCount,
                                                       sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || m_nLastTransferValue == UINT64_MAX ||
	     sirVulkanValidateBufferCopies( pCopies, nCount ) != SIR_VULKAN_SUCCESS ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	// Validate the entire batch before recording any native command. A failed
	// validation must neither submit partial work nor change completion state.
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		const sirVulkanBufferCopy_t &copy = pCopies[iCopy];
		if ( copy.pSource->hBuffer == VK_NULL_HANDLE || copy.pDestination->hBuffer == VK_NULL_HANDLE ||
		     copy.pSource->hBuffer == copy.pDestination->hBuffer || copy.pSource->pOwnerAllocator != m_Allocator ||
		     copy.pDestination->pOwnerAllocator != m_Allocator || copy.pSource->nDeviceSerial != m_nDeviceSerial ||
		     copy.pDestination->nDeviceSerial != m_nDeviceSerial ) {
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
	if ( pSlot == nullptr ) {
		return SIR_VULKAN_BUSY;
	}
	if ( m_pfnResetCommandBuffer( pSlot->hCommandBuffer, 0 ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ( m_pfnBeginCommandBuffer( pSlot->hCommandBuffer, &beginInfo ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	// Between each copy, order writes so a later copy can read an earlier
	// destination. This supports CPU upload -> GPU staging -> readback within
	// one native command buffer and one timeline value.
	VkMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COPY_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers = &barrier;
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		const sirVulkanBufferCopy_t &copy = pCopies[iCopy];
		m_pfnCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );
		VkBufferCopy2 region = {};
		region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
		region.srcOffset = copy.nSourceOffset;
		region.dstOffset = copy.nDestinationOffset;
		region.size = copy.nBytes;
		VkCopyBufferInfo2 copyInfo = {};
		copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
		copyInfo.srcBuffer = copy.pSource->hBuffer;
		copyInfo.dstBuffer = copy.pDestination->hBuffer;
		copyInfo.regionCount = 1;
		copyInfo.pRegions = &region;
		m_pfnCmdCopyBuffer2( pSlot->hCommandBuffer, &copyInfo );
	}
	if ( m_pfnEndCommandBuffer( pSlot->hCommandBuffer ) != VK_SUCCESS ) {
		return SIR_VULKAN_NATIVE_FAILURE;
	}

	const uint64_t nSignalValue = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo commandInfo = {};
	commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandInfo.commandBuffer = pSlot->hCommandBuffer;
	VkSemaphoreSubmitInfo signalInfo = {};
	signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfo.semaphore = m_TransferTimeline;
	signalInfo.value = nSignalValue;
	signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalInfo;
	const VkResult eSubmit = m_pfnQueueSubmit2( m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
	if ( eSubmit != VK_SUCCESS ) {
		m_bDeviceLost = eSubmit == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = nSignalValue;
	pSlot->nLastValue = nSignalValue;
	for ( uint32_t iCopy = 0; iCopy < nCount; ++iCopy ) {
		pCopies[iCopy].pSource->nLastGpuUse = nSignalValue;
		pCopies[iCopy].pDestination->nLastGpuUse = nSignalValue;
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nSignalValue;
	return SIR_VULKAN_SUCCESS;
}
