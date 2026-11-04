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
#include <Core/Platform/SIRAtomic.h>

#include <string.h>

static SIRAtomic<uint32_t> s_nNextVulkanGraphicsGeneration( 1 );

static sirGraphicsResult_t sirVulkanToGraphicsResult( sirVulkanResult_t eResult ) noexcept
{
	switch ( eResult ) {
	case SIR_VULKAN_SUCCESS:
		return SIR_GRAPHICS_SUCCESS;
	case SIR_VULKAN_INVALID_ARGUMENT:
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	case SIR_VULKAN_ALLOCATION_FAILED:
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	case SIR_VULKAN_BUSY:
		return SIR_GRAPHICS_BUSY;
	case SIR_VULKAN_TIMEOUT:
		return SIR_GRAPHICS_TIMEOUT;
	case SIR_VULKAN_API_UNSUPPORTED:
	case SIR_VULKAN_LOADER_UNAVAILABLE:
	case SIR_VULKAN_DEVICE_UNAVAILABLE:
		return SIR_GRAPHICS_UNSUPPORTED;
	case SIR_VULKAN_INVALID_STATE:
		return SIR_GRAPHICS_BUSY;
	default:
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
}

void SIRVulkanCommandList::prepare( SIRVulkanRenderingDevice *pOwner ) noexcept
{
	m_pOwner = pOwner;
	m_eState = SIR_VULKAN_COMMAND_RECORDING;
	m_hSource = {};
	m_hDestination = {};
	m_nSourceOffset = 0;
	m_nDestinationOffset = 0;
	m_nBytes = 0;
	m_nLastCompletion = 0;
	m_bHasCopy = false;
	m_bHasTextureCopy = false;
	m_TextureCopy = {};
}

sirGraphicsResult_t SIRVulkanCommandList::reset( void ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING && m_eState != SIR_VULKAN_COMMAND_CLOSED ) {
		return SIR_GRAPHICS_BUSY;
	}
	m_eState = SIR_VULKAN_COMMAND_RECORDING;
	m_bHasCopy = false;
	m_bHasTextureCopy = false;
	m_TextureCopy = {};
	m_hSource = {};
	m_hDestination = {};
	m_nBytes = 0;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource,
                                                      uint64_t nSrcOffset, uint64_t nBytes ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_bHasCopy ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_pOwner == nullptr || nBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirVulkanGraphicsBufferSlot_t *pSource = m_pOwner->resolveBuffer( hSource );
	const sirVulkanGraphicsBufferSlot_t *pDestination = m_pOwner->resolveBuffer( hDestination );
	if ( pSource == nullptr || pDestination == nullptr || pSource == pDestination ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSource->pMapped != nullptr || pDestination->pMapped != nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( nSrcOffset > pSource->native.nSize || nBytes > pSource->native.nSize - nSrcOffset || nDstOffset > pDestination->native.nSize ||
	     nBytes > pDestination->native.nSize - nDstOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pSource->native.nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 ||
	     ( pDestination->native.nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_hSource = hSource;
	m_hDestination = hDestination;
	m_nSourceOffset = nSrcOffset;
	m_nDestinationOffset = nDstOffset;
	m_nBytes = nBytes;
	m_bHasCopy = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::close( void ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING ) {
		return SIR_GRAPHICS_BUSY;
	}
	// Closing an empty list cancels the borrow instead of exhausting the pool.
	m_eState = m_bHasCopy ? SIR_VULKAN_COMMAND_CLOSED : SIR_VULKAN_COMMAND_FREE;
	return SIR_GRAPHICS_SUCCESS;
}

SIRVulkanRenderingDevice::SIRVulkanRenderingDevice( void ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BUFFER_CAPACITY; ++iSlot ) {
		m_szBuffers[iSlot].nGeneration = 1;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TEXTURE_CAPACITY; ++iSlot ) {
		m_szImages[iSlot].nGeneration = 1;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_VIEW_CAPACITY; ++iSlot ) {
		m_szViews[iSlot].nGeneration = 1;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_SAMPLER_CAPACITY; ++iSlot ) {
		m_szSamplers[iSlot].nGeneration = 1;
	}
}

SIRVulkanRenderingDevice::~SIRVulkanRenderingDevice( void ) noexcept
{
	( void )shutdown();
}

sirGraphicsResult_t SIRVulkanRenderingDevice::initialize( void ) noexcept
{
	if ( m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirVulkanResult_t eInitialize = m_Native.initialize();
	if ( eInitialize != SIR_VULKAN_SUCCESS ) {
		return sirVulkanToGraphicsResult( eInitialize );
	}
	m_nDeviceGeneration = s_nNextVulkanGraphicsGeneration.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
	if ( m_nDeviceGeneration == 0 ) {
		( void )m_Native.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_nNativeSerial = 0;
	m_nLastCompletion = 0;
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
		m_szCommands[iSlot].m_eState = SIR_VULKAN_COMMAND_FREE;
		m_szCommands[iSlot].m_pOwner = nullptr;
	}
	m_bReady = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirVulkanGraphicsBufferSlot_t *SIRVulkanRenderingDevice::resolveBuffer( sirGraphicsHandle_t handle ) noexcept
{
	if ( !m_bReady || !sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, m_nDeviceGeneration ) ||
	     handle.nSlot >= SIR_VULKAN_BUFFER_CAPACITY ) {
		return nullptr;
	}
	sirVulkanGraphicsBufferSlot_t *pSlot = &m_szBuffers[handle.nSlot];
	return pSlot->bActive && pSlot->nGeneration == handle.nGeneration ? pSlot : nullptr;
}

const sirVulkanGraphicsBufferSlot_t *SIRVulkanRenderingDevice::resolveBuffer( sirGraphicsHandle_t handle ) const noexcept
{
	return const_cast<SIRVulkanRenderingDevice *>( this )->resolveBuffer( handle );
}

void SIRVulkanRenderingDevice::collectRetired( void ) noexcept
{
	if ( !m_bReady ) {
		return;
	}
	collectImageResources();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BUFFER_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsBufferSlot_t *pSlot = &m_szBuffers[iSlot];
		if ( !pSlot->bRetiring ) {
			continue;
		}
		if ( pSlot->nRetireValue != 0 ) {
			sirVulkanCompletion_t completion = { m_nNativeSerial, pSlot->nRetireValue };
			bool bCompleted = false;
			if ( m_Native.queryCompletion( &completion, &bCompleted ) != SIR_VULKAN_SUCCESS || !bCompleted ) {
				continue;
			}
		}
		if ( m_Native.destroyIdleBuffer( &pSlot->native ) == SIR_VULKAN_SUCCESS ) {
			pSlot->bRetiring = false;
			pSlot->nRetireValue = 0;
		}
	}
}

sirGraphicsResult_t SIRVulkanRenderingDevice::queryCapabilities( sirGraphicsCapabilities_t *pCapabilities ) const noexcept
{
	if ( pCapabilities == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCapabilities = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirVulkanAdapterInfo_t adapter = {};
	if ( !m_Native.queryAdapter( &adapter ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pCapabilities->eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	pCapabilities->nApiVersion = { VK_API_VERSION_MAJOR( adapter.nApiVersion ), VK_API_VERSION_MINOR( adapter.nApiVersion ),
	                               VK_API_VERSION_PATCH( adapter.nApiVersion ) };
	// Transfer is implemented; graphics drawing and compute remain unavailable.
	pCapabilities->nFeatureMask = 0;
	pCapabilities->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	pCapabilities->nMaximumFramesInFlight = SIR_VULKAN_TRANSFER_SLOT_COUNT;
	pCapabilities->nDeviceGeneration = m_nDeviceGeneration;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::queryMemoryBudget( sirGraphicsMemoryBudget_t *pBudget ) const noexcept
{
	if ( pBudget == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBudget = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirVulkanMemoryStats_t native = {};
	if ( !m_Native.queryMemoryStats( &native ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pBudget->nBudgetBytes = native.nEstimatedBudgetBytes;
	pBudget->nCommittedBytes = native.nBlockBytes;
	pBudget->nResidentBytes = native.nAllocationBytes;
	pBudget->nPeakCommittedBytes = native.nBlockBytes;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::createBuffer( const sirGraphicsBufferDesc_t &desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eValidation = sirGraphicsValidateBuffer( desc );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	// The native bootstrap has no custom-alignment or sealed-pool contract yet.
	if ( desc.nAlignmentBytes != 0 || desc.eMemory == SIR_GRAPHICS_MEMORY_TRANSIENT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	VkBufferUsageFlags nUsage = 0;
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_VERTEX ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_INDEX ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_CONSTANT ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_STORAGE ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_INDIRECT ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_COPY_SOURCE ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	if ( ( desc.nUsage & SIR_GRAPHICS_USAGE_COPY_DESTINATION ) != 0 ) {
		nUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if ( nUsage == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BUFFER_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsBufferSlot_t *pSlot = &m_szBuffers[iSlot];
		if ( pSlot->bActive || pSlot->bRetiring || pSlot->nGeneration == 0 ) {
			continue;
		}
		const bool bHostVisible = desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD || desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK ||
		                          desc.eMemory == SIR_GRAPHICS_MEMORY_GPU_UPLOAD;
		const sirVulkanResult_t eCreate = m_Native.createBuffer( desc.nSizeBytes, nUsage, bHostVisible, &pSlot->native );
		if ( eCreate != SIR_VULKAN_SUCCESS ) {
			return sirVulkanToGraphicsResult( eCreate );
		}
		m_nNativeSerial = pSlot->native.nDeviceSerial;
		pSlot->bActive = true;
		*pHandle = { iSlot, pSlot->nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_BUFFER };
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes,
                                                         sirGraphicsMappedBuffer_t *pMapped ) noexcept
{
	if ( pMapped == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pMapped = {};
	sirVulkanGraphicsBufferSlot_t *pSlot = resolveBuffer( hBuffer );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !pSlot->native.bHostVisible || pSlot->pMapped != nullptr || nBytes == 0 || nOffset > pSlot->native.nSize ||
	     nBytes > pSlot->native.nSize - nOffset || nOffset > SIZE_MAX || nBytes > SIZE_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->native.nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion = { pSlot->native.nDeviceSerial, pSlot->native.nLastGpuUse };
		const sirVulkanResult_t eWait = m_Native.waitCompletion( &completion, 0 );
		if ( eWait != SIR_VULKAN_SUCCESS ) {
			return sirVulkanToGraphicsResult( eWait );
		}
	}
	void *pBase = nullptr;
	const VkResult eMap = vmaMapMemory( pSlot->native.pOwnerAllocator, pSlot->native.pAllocation, &pBase );
	if ( eMap != VK_SUCCESS || pBase == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pSlot->pMapped = pBase;
	pSlot->nMappedOffset = nOffset;
	pSlot->nMappedBytes = nBytes;
	*pMapped = { ( uint8_t * )pBase + nOffset, nOffset, nBytes };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	sirVulkanGraphicsBufferSlot_t *pSlot = resolveBuffer( hBuffer );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->pMapped == nullptr || nOffset != pSlot->nMappedOffset || nBytes != pSlot->nMappedBytes ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	vmaUnmapMemory( pSlot->native.pOwnerAllocator, pSlot->native.pAllocation );
	pSlot->pMapped = nullptr;
	pSlot->nMappedOffset = 0;
	pSlot->nMappedBytes = 0;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	sirVulkanGraphicsBufferSlot_t *pSlot = resolveBuffer( hBuffer );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->pMapped == nullptr || nBytes == 0 || nOffset < pSlot->nMappedOffset ||
	     nOffset > pSlot->nMappedOffset + pSlot->nMappedBytes || nBytes > pSlot->nMappedOffset + pSlot->nMappedBytes - nOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const VkResult eFlush = vmaFlushAllocation( pSlot->native.pOwnerAllocator, pSlot->native.pAllocation, nOffset, nBytes );
	return eFlush == VK_SUCCESS ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	sirVulkanGraphicsBufferSlot_t *pSlot = resolveBuffer( hBuffer );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->pMapped == nullptr || nBytes == 0 || nOffset < pSlot->nMappedOffset ||
	     nOffset > pSlot->nMappedOffset + pSlot->nMappedBytes || nBytes > pSlot->nMappedOffset + pSlot->nMappedBytes - nOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const VkResult eInvalidate = vmaInvalidateAllocation( pSlot->native.pOwnerAllocator, pSlot->native.pAllocation, nOffset, nBytes );
	return eInvalidate == VK_SUCCESS ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::acquireCommands( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept
{
	if ( ppCommands == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppCommands = nullptr;
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
		SIRVulkanCommandList *pList = &m_szCommands[iSlot];
		if ( pList->m_eState == SIR_VULKAN_COMMAND_SUBMITTED ) {
			sirVulkanCompletion_t completion = { m_nNativeSerial, pList->m_nLastCompletion };
			bool bCompleted = false;
			if ( m_Native.queryCompletion( &completion, &bCompleted ) != SIR_VULKAN_SUCCESS || !bCompleted ) {
				continue;
			}
			pList->m_eState = SIR_VULKAN_COMMAND_FREE;
		}
		if ( pList->m_eState != SIR_VULKAN_COMMAND_FREE ) {
			continue;
		}
		pList->prepare( this );
		*ppCommands = pList;
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_BUSY;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t &submission,
                                                      sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( !m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eValidation = sirGraphicsValidateSubmission( eQueue, submission, m_nDeviceGeneration );
	if ( eValidation != SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( eQueue != SIR_GRAPHICS_QUEUE_DIRECT || submission.nCommandListCount != 1 || submission.nAcquisitionCount != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t iWait = 0; iWait < submission.nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t &wait = submission.pWaits[iWait];
		if ( wait.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || wait.nValue > m_nLastCompletion ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	SIRVulkanCommandList *pList = nullptr;
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
		if ( submission.ppCommandLists[0] == &m_szCommands[iSlot] ) {
			pList = &m_szCommands[iSlot];
			break;
		}
	}
	if ( pList == nullptr || pList->m_eState != SIR_VULKAN_COMMAND_CLOSED || !pList->m_bHasCopy ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanCompletion_t native = {};
	sirVulkanResult_t eSubmit = SIR_VULKAN_INVALID_ARGUMENT;
	if ( pList->m_bHasTextureCopy ) {
		const sirGraphicsBufferTextureCopy_t &copy = pList->m_TextureCopy;
		sirVulkanGraphicsBufferSlot_t *pBuffer = resolveBuffer( copy.hBuffer );
		if ( pBuffer == nullptr || copy.hTexture.nSlot >= SIR_VULKAN_TEXTURE_CAPACITY ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		sirVulkanGraphicsImageSlot_t *pImage = &m_szImages[copy.hTexture.nSlot];
		if ( !pImage->bActive || pImage->nGeneration != copy.hTexture.nGeneration ||
		     copy.hTexture.nDeviceGeneration != m_nDeviceGeneration || copy.hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( pBuffer->pMapped != nullptr ) {
			return SIR_GRAPHICS_BUSY;
		}
		sirVulkanTextureTransfer_t transfer = {};
		transfer.nBufferOffset = copy.nBufferOffset;
		sirVulkanFormatBlock_t block = {};
		if ( !sirVulkanGetFormatBlock( pImage->native.eFormat, &block ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		transfer.nBufferRowLength = copy.nBytesPerRow == 0 ? 0 :
		                            ( copy.nBytesPerRow / block.nBytesPerBlock ) * block.nBlockWidth;
		transfer.nBufferImageHeight = copy.nRowsPerImage;
		transfer.nMip = copy.nMip;
		transfer.nLayer = copy.nLayer;
		transfer.offset = { ( int32_t )copy.nX, ( int32_t )copy.nY, 0 };
		transfer.extent = { copy.nWidth, copy.nHeight, 1 };
		transfer.bBufferToImage = copy.bBufferToTexture;
		eSubmit = m_Native.submitBufferImageCopy( &pBuffer->native, &pImage->native, transfer, &native );
	} else {
		sirVulkanGraphicsBufferSlot_t *pSource = resolveBuffer( pList->m_hSource );
		sirVulkanGraphicsBufferSlot_t *pDestination = resolveBuffer( pList->m_hDestination );
		if ( pSource == nullptr || pDestination == nullptr ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( pSource->pMapped != nullptr || pDestination->pMapped != nullptr ) {
			return SIR_GRAPHICS_BUSY;
		}
		eSubmit = m_Native.submitBufferCopy( &pSource->native, pList->m_nSourceOffset, &pDestination->native, pList->m_nDestinationOffset,
		                                     pList->m_nBytes, &native );
	}
	if ( eSubmit != SIR_VULKAN_SUCCESS ) {
		return sirVulkanToGraphicsResult( eSubmit );
	}
	m_nLastCompletion = native.nValue;
	pList->m_eState = SIR_VULKAN_COMMAND_SUBMITTED;
	pList->m_nLastCompletion = native.nValue;
	*pCompletion = { m_nDeviceGeneration, SIR_GRAPHICS_QUEUE_DIRECT, native.nValue };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::poll( const sirGraphicsCompletion_t &completion, bool *pCompleted ) noexcept
{
	if ( pCompleted == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompleted = false;
	if ( !m_bReady || completion.nDeviceGeneration != m_nDeviceGeneration || completion.eQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	     completion.nValue == 0 || completion.nValue > m_nLastCompletion ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirVulkanCompletion_t native = { m_nNativeSerial, completion.nValue };
	const sirVulkanResult_t ePoll = m_Native.queryCompletion( &native, pCompleted );
	if ( ePoll == SIR_VULKAN_SUCCESS ) {
		collectRetired();
	}
	return sirVulkanToGraphicsResult( ePoll );
}

sirGraphicsResult_t SIRVulkanRenderingDevice::wait( const sirGraphicsCompletion_t &completion, uint64_t nTimeoutNs ) noexcept
{
	if ( !m_bReady || completion.nDeviceGeneration != m_nDeviceGeneration || completion.eQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	     completion.nValue == 0 || completion.nValue > m_nLastCompletion ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirVulkanCompletion_t native = { m_nNativeSerial, completion.nValue };
	const sirVulkanResult_t eWait = m_Native.waitCompletion( &native, nTimeoutNs );
	if ( eWait == SIR_VULKAN_SUCCESS ) {
		collectRetired();
	}
	return sirVulkanToGraphicsResult( eWait );
}

sirGraphicsResult_t SIRVulkanRenderingDevice::retireResource( sirGraphicsHandle_t hResource,
                                                              const sirGraphicsCompletionSet_t &completion ) noexcept
{
	if ( hResource.eType != SIR_GRAPHICS_HANDLE_BUFFER ) {
		return retireImageResource( hResource, completion );
	}
	sirVulkanGraphicsBufferSlot_t *pSlot = resolveBuffer( hResource );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ||
	     ( completion.nQueueMask & ~( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->pMapped != nullptr ) {
		return SIR_GRAPHICS_BUSY;
	}
	const uint64_t nFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
	if ( nFence > m_nLastCompletion || nFence < pSlot->native.nLastGpuUse ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pSlot->bActive = false;
	pSlot->bRetiring = true;
	pSlot->nRetireValue = nFence;
	++pSlot->nGeneration;
	collectRetired();
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::shutdown( void ) noexcept
{
	if ( !m_bReady ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	collectRetired();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BUFFER_CAPACITY; ++iSlot ) {
		if ( m_szBuffers[iSlot].bActive || m_szBuffers[iSlot].bRetiring ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
		SIRVulkanCommandList *pList = &m_szCommands[iSlot];
		if ( pList->m_eState == SIR_VULKAN_COMMAND_RECORDING || pList->m_eState == SIR_VULKAN_COMMAND_CLOSED ) {
			return SIR_GRAPHICS_BUSY;
		}
		if ( pList->m_eState == SIR_VULKAN_COMMAND_SUBMITTED ) {
			const sirVulkanCompletion_t completion = { m_nNativeSerial, pList->m_nLastCompletion };
			bool bCompleted = false;
			if ( m_Native.queryCompletion( &completion, &bCompleted ) != SIR_VULKAN_SUCCESS || !bCompleted ) {
				return SIR_GRAPHICS_BUSY;
			}
			pList->m_eState = SIR_VULKAN_COMMAND_FREE;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TEXTURE_CAPACITY; ++iSlot ) {
		if ( m_szImages[iSlot].bActive || m_szImages[iSlot].bRetiring ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_VIEW_CAPACITY; ++iSlot ) {
		if ( m_szViews[iSlot].bActive || m_szViews[iSlot].bRetiring ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_SAMPLER_CAPACITY; ++iSlot ) {
		if ( m_szSamplers[iSlot].bActive || m_szSamplers[iSlot].bRetiring ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	if ( !m_Native.shutdown() ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bReady = false;
	m_nDeviceGeneration = 0;
	m_nNativeSerial = 0;
	m_nLastCompletion = 0;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanDriver::getAdapterCount( uint32_t *pCount ) const noexcept
{
	if ( pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	SIRVulkanDevice probe;
	const sirVulkanResult_t eResult = probe.initialize();
	if ( eResult != SIR_VULKAN_SUCCESS ) {
		return sirVulkanToGraphicsResult( eResult );
	}
	*pCount = 1;
	return probe.shutdown() ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t SIRVulkanDriver::queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept
{
	if ( pInfo == nullptr || nAdapterIndex != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pInfo = {};
	SIRVulkanDevice probe;
	const sirVulkanResult_t eResult = probe.initialize();
	if ( eResult != SIR_VULKAN_SUCCESS ) {
		return sirVulkanToGraphicsResult( eResult );
	}
	sirVulkanAdapterInfo_t native = {};
	if ( !probe.queryAdapter( &native ) ) {
		( void )probe.shutdown();
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pInfo->nAdapterIndex = 0;
	pInfo->nVendorId = native.nVendorId;
	pInfo->nDeviceId = native.nDeviceId;
	pInfo->nDedicatedVideoBytes = native.nDeviceLocalHeapBytes;
	pInfo->nFeatureMask = 0;
	pInfo->nApiVersion = { VK_API_VERSION_MAJOR( native.nApiVersion ), VK_API_VERSION_MINOR( native.nApiVersion ),
	                       VK_API_VERSION_PATCH( native.nApiVersion ) };
	memcpy( pInfo->szName, native.szDeviceName, sizeof( pInfo->szName ) );
	return probe.shutdown() ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t SIRVulkanDriver::createDevice( const sirGraphicsDeviceCreateDesc_t &desc, SIRRenderingDevice **ppDevice ) noexcept
{
	if ( ppDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDevice = nullptr;
	if ( desc.nAdapterIndex != 0 || desc.nFramesInFlight == 0 || desc.nFramesInFlight > SIR_VULKAN_TRANSFER_SLOT_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nRequiredFeatures != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	// Device-budget sealing, custom residency and presentation are separate
	// native work packages. They must not silently succeed in this slice.
	if ( !desc.bHeadless || desc.nResidentBudgetBytes != 0 || desc.nUploadBudgetBytes != 0 || desc.nReadbackBudgetBytes != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsResult_t eResult = m_Device.initialize();
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		*ppDevice = &m_Device;
	}
	return eResult;
}

sirGraphicsResult_t SIRVulkanDriver::destroyDevice( SIRRenderingDevice *pDevice ) noexcept
{
	if ( pDevice != &m_Device ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return m_Device.shutdown();
}
