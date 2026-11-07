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
#include <Renderer/SIRGraphicsABIOperations.h>
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

sirGraphicsResult_t sirVulkanDescribeAdapter( uint32_t nAdapterIndex, const sirVulkanAdapterInfo_t& native,
                                              sirGraphicsAdapterInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pInfo = {};
	pInfo->nAdapterIndex = nAdapterIndex;
	pInfo->nVendorId = native.nVendorId;
	pInfo->nDeviceId = native.nDeviceId;
	pInfo->nDedicatedVideoBytes = native.nDeviceLocalHeapBytes;
	pInfo->nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | ( native.bComputeCapable ? SIR_GRAPHICS_FEATURE_COMPUTE : 0 );
	pInfo->bSoftwareAdapter = native.bSoftwareAdapter;
	pInfo->nApiVersion = { VK_API_VERSION_MAJOR( native.nApiVersion ), VK_API_VERSION_MINOR( native.nApiVersion ),
	                       VK_API_VERSION_PATCH( native.nApiVersion ) };
	memcpy( pInfo->szName, native.szDeviceName, sizeof( pInfo->szName ) - 1 );
	pInfo->szName[sizeof( pInfo->szName ) - 1] = '\0';
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirVulkanDescribeCapabilities( const sirVulkanAdapterInfo_t& native, uint32_t nGeneration,
                                                   sirGraphicsCapabilities_t *pCapabilities ) noexcept
{
	if ( pCapabilities == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCapabilities = {};
	if ( nGeneration == 0 || native.nApiVersion < VK_API_VERSION_1_4 || native.nMaximumTextureDimension == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pCapabilities->eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	pCapabilities->nApiVersion = { VK_API_VERSION_MAJOR( native.nApiVersion ), VK_API_VERSION_MINOR( native.nApiVersion ),
	                               VK_API_VERSION_PATCH( native.nApiVersion ) };
	// The current backend exposes a direct queue with graphics and compute execution.
	pCapabilities->nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_MRT |
	                              ( native.bComputeCapable ? SIR_GRAPHICS_FEATURE_COMPUTE : 0 );
	pCapabilities->nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	pCapabilities->nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 );
	pCapabilities->nMaximumColorAttachments = 8;
	pCapabilities->nMaximumBindingSlots = 32;
	pCapabilities->nMaximumTextureDimension = native.nMaximumTextureDimension;
	pCapabilities->nMaximumFramesInFlight = SIR_VULKAN_TRANSFER_SLOT_COUNT;
	pCapabilities->nMaximumSampleCount = 1;
	pCapabilities->nDeviceGeneration = nGeneration;
	pCapabilities->bSoftwareAdapter = native.bSoftwareAdapter;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirVulkanValidateDeviceAdmission( const sirGraphicsDeviceCreateDesc_t& request,
                                                      const sirGraphicsCapabilities_t& available ) noexcept
{
	if ( ( available.nFeatureMask & request.nRequiredFeatures ) != request.nRequiredFeatures ||
	     ( available.bSoftwareAdapter && !request.bAllowSoftware ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

// Validate the bounded native batch without requiring a Vulkan device.
sirGraphicsResult_t sirVulkanValidateBatchCounts( const uint32_t *pCounts, uint32_t nListCount, uint32_t *pTotal ) noexcept
{
	if ( pTotal == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pTotal = 0;
	if ( pCounts == nullptr || nListCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nListCount > SIR_VULKAN_TRANSFER_SLOT_COUNT ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	uint32_t nTotal = 0;
	for ( uint32_t iList = 0; iList < nListCount; ++iList ) {
		const uint32_t nCount = pCounts[iList];
		if ( nCount == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( nCount > SIR_VULKAN_COPY_BATCH_CAPACITY - nTotal ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		nTotal += nCount;
	}
	*pTotal = nTotal;
	return SIR_GRAPHICS_SUCCESS;
}

void SIRVulkanCommandList::prepare( SIRVulkanRenderingDevice *pOwner ) noexcept
{
	m_pOwner = pOwner;
	m_eState = SIR_VULKAN_COMMAND_RECORDING;
	m_nOperationCount = 0;
	m_nLastCompletion = 0;
	m_hBoundPipeline = {};
	m_bInRenderPass = false;
}

sirGraphicsResult_t SIRVulkanCommandList::reset( void ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING && m_eState != SIR_VULKAN_COMMAND_CLOSED ) {
		return SIR_GRAPHICS_BUSY;
	}
	m_eState = SIR_VULKAN_COMMAND_RECORDING;
	m_nOperationCount = 0;
	m_hBoundPipeline = {};
	m_bInRenderPass = false;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource,
                                                      uint64_t nSrcOffset, uint64_t nBytes ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_nOperationCount >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
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
	sirVulkanGraphicsOperation_t& operation = m_szOperations[m_nOperationCount++];
	operation = {};
	operation.eKind = SIR_VULKAN_GRAPHICS_BUFFER_COPY;
	operation.hSource = hSource;
	operation.hDestination = hDestination;
	operation.nSourceOffset = nSrcOffset;
	operation.nDestinationOffset = nDstOffset;
	operation.nBytes = nBytes;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanCommandList::close( void ) noexcept
{
	if ( m_eState != SIR_VULKAN_COMMAND_RECORDING || m_bInRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	// Closing an empty list cancels the borrow instead of exhausting the pool.
	m_eState = m_nOperationCount != 0 ? SIR_VULKAN_COMMAND_CLOSED : SIR_VULKAN_COMMAND_FREE;
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
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_PIPELINE_CAPACITY; ++iSlot ) {
		m_szPipelines[iSlot].nGeneration = 1;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BINDING_SET_CAPACITY; ++iSlot ) {
		m_szBindingSets[iSlot].nGeneration = 1;
	}
}

SIRVulkanRenderingDevice::~SIRVulkanRenderingDevice( void ) noexcept
{
	( void )shutdown();
}

sirGraphicsResult_t SIRVulkanRenderingDevice::initialize( const sirVulkanPoolBudget_t *pBudget, uint32_t nAdapterIndex ) noexcept
{
	if ( m_bReady ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirVulkanResult_t eInitialize = m_Native.initialize( pBudget, nAdapterIndex );
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
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BINDING_SET_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[iSlot];
		if ( !set.bRetiring )
			continue;
		if ( set.nRetireValue != 0 ) {
			const sirVulkanCompletion_t completion = { m_nNativeSerial, set.nRetireValue };
			bool bCompleted = false;
			if ( m_Native.queryCompletion( &completion, &bCompleted ) != SIR_VULKAN_SUCCESS || !bCompleted )
				continue;
		}
		if ( m_Native.destroyDescriptorSet( &set.native ) == SIR_VULKAN_SUCCESS ) {
			set.bRetiring = false;
			set.nRetireValue = 0;
			set.nReferenceCount = 0;
		}
	}
	collectImageResources();
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_PIPELINE_CAPACITY; ++iSlot ) {
		sirVulkanGraphicsPipelineSlot_t *pSlot = &m_szPipelines[iSlot];
		if ( !pSlot->bRetiring ) {
			continue;
		}
		bool bSetsLive = false;
		for ( uint32_t iSet = 0; iSet < SIR_VULKAN_BINDING_SET_CAPACITY; ++iSet ) {
			const sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[iSet];
			if ( ( set.bActive || set.bRetiring ) && set.hPipeline.nSlot == iSlot &&
			     set.hPipeline.nDeviceGeneration == m_nDeviceGeneration &&
			     set.hPipeline.nGeneration == pSlot->nGeneration - uint32_t( pSlot->bRetiring ) )
				bSetsLive = true;
		}
		if ( bSetsLive )
			continue;
		if ( pSlot->nRetireValue != 0 ) {
			const sirVulkanCompletion_t completion = { m_nNativeSerial, pSlot->nRetireValue };
			bool bCompleted = false;
			if ( m_Native.queryCompletion( &completion, &bCompleted ) != SIR_VULKAN_SUCCESS || !bCompleted ) {
				continue;
			}
		}
		if ( m_Native.destroyComputePipeline( &pSlot->native ) == SIR_VULKAN_SUCCESS ) {
			pSlot->bRetiring = false;
			pSlot->nRetireValue = 0;
		}
	}
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
	return sirVulkanDescribeCapabilities( adapter, m_nDeviceGeneration, pCapabilities );
}

sirGraphicsResult_t SIRVulkanRenderingDevice::queryPipelineModelCaps( sirGraphicsPipelineModel_t eModel,
                                                                      sirGraphicsABIPipelineModelCaps_t *pCaps ) const noexcept
{
	if ( pCaps == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( eModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsCapabilities_t available = {};
	const sirGraphicsResult_t eResult = queryCapabilities( &available );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( ( available.nFeatureMask & SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = available.nDeviceGeneration;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT |
	                        ( ( available.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 ? SIR_GRAPHICS_STAGE_COMPUTE : 0 );
	caps.nQueueMask = available.nQueueMask;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	caps.nMaximumSamples = available.nMaximumSampleCount;
	caps.nMaximumTextureDimension = available.nMaximumTextureDimension;
	caps.nSupportsNPOT = 1;
	*pCaps = caps;
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
	pBudget->nBudgetBytes = native.nSealedBudgetBytes != 0 ? native.nSealedBudgetBytes : native.nEstimatedBudgetBytes;
	pBudget->nCommittedBytes = native.nBlockBytes;
	pBudget->nResidentBytes = native.nAllocationBytes;
	pBudget->nPeakCommittedBytes = native.nPeakCommittedBytes;
	pBudget->nUploadMappedBytes = native.nUploadMappedBytes;
	pBudget->nReadbackMappedBytes = native.nReadbackMappedBytes;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
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
	// Custom alignment and transient placement remain separate native work.
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
		const sirVulkanBufferPlacement_t ePlacement = desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD     ? SIR_VULKAN_BUFFER_CPU_UPLOAD
		                                              : desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_READBACK ? SIR_VULKAN_BUFFER_CPU_READBACK
		                                              : desc.eMemory == SIR_GRAPHICS_MEMORY_GPU_UPLOAD   ? SIR_VULKAN_BUFFER_GPU_UPLOAD
		                                                                                                 : SIR_VULKAN_BUFFER_GPU_LOCAL;
		const sirVulkanResult_t eCreate = m_Native.createBuffer( desc.nSizeBytes, nUsage, ePlacement, &pSlot->native );
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
	if ( pSlot->native.pMappedData == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pSlot->pMapped = pSlot->native.pMappedData;
	pSlot->nMappedOffset = nOffset;
	pSlot->nMappedBytes = nBytes;
	*pMapped = { ( uint8_t * )pSlot->native.pMappedData + nOffset, nOffset, nBytes };
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

sirGraphicsResult_t SIRVulkanRenderingDevice::submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission,
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
	if ( eQueue != SIR_GRAPHICS_QUEUE_DIRECT || submission.nAcquisitionCount != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t iWait = 0; iWait < submission.nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t& wait = submission.pWaits[iWait];
		if ( wait.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || wait.nValue > m_nLastCompletion ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	if ( submission.nCommandListCount > SIR_VULKAN_TRANSFER_SLOT_COUNT ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	SIRVulkanCommandList *szLists[SIR_VULKAN_TRANSFER_SLOT_COUNT] = {};
	uint32_t szCounts[SIR_VULKAN_TRANSFER_SLOT_COUNT] = {};
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		SIRVulkanCommandList *pList = nullptr;
		for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iSlot ) {
			if ( submission.ppCommandLists[iList] == &m_szCommands[iSlot] ) {
				pList = &m_szCommands[iSlot];
				break;
			}
		}
		if ( pList == nullptr || pList->m_eState != SIR_VULKAN_COMMAND_CLOSED || pList->m_nOperationCount == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t iEarlier = 0; iEarlier < iList; ++iEarlier ) {
			if ( szLists[iEarlier] == pList ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		szLists[iList] = pList;
		szCounts[iList] = pList->m_nOperationCount;
	}
	uint32_t nNativeCount = 0;
	const sirGraphicsResult_t eBatch = sirVulkanValidateBatchCounts( szCounts, submission.nCommandListCount, &nNativeCount );
	if ( eBatch != SIR_GRAPHICS_SUCCESS ) {
		return eBatch;
	}
	sirVulkanRecordedCommand_t commands[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	sirVulkanImage_t *szOriginals[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	sirVulkanImage_t szShadows[SIR_VULKAN_COPY_BATCH_CAPACITY] = {};
	uint32_t nImages = 0;
	uint32_t nRecorded = 0;
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		const SIRVulkanCommandList *pList = szLists[iList];
		for ( uint32_t iCommand = 0; iCommand < pList->m_nOperationCount; ++iCommand ) {
			const sirVulkanGraphicsOperation_t& op = pList->m_szOperations[iCommand];
			sirVulkanRecordedCommand_t& nativeOp = commands[nRecorded++];
			if ( op.eKind == SIR_VULKAN_GRAPHICS_BUFFER_COPY ) {
				sirVulkanGraphicsBufferSlot_t *pSrc = resolveBuffer( op.hSource );
				sirVulkanGraphicsBufferSlot_t *pDst = resolveBuffer( op.hDestination );
				if ( pSrc == nullptr || pDst == nullptr ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				if ( pSrc->pMapped != nullptr || pDst->pMapped != nullptr ) {
					return SIR_GRAPHICS_BUSY;
				}
				nativeOp.eKind = SIR_VULKAN_RECORDED_BUFFER_COPY;
				nativeOp.bufferCopy = { &pSrc->native, &pDst->native, op.nSourceOffset, op.nDestinationOffset, op.nBytes };
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_COMPUTE_DISPATCH ) {
				const sirGraphicsHandle_t hPipeline = op.hPipeline;
				if ( hPipeline.eType != SIR_GRAPHICS_HANDLE_PIPELINE || hPipeline.nDeviceGeneration != m_nDeviceGeneration ||
				     hPipeline.nSlot >= SIR_VULKAN_PIPELINE_CAPACITY ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				sirVulkanGraphicsPipelineSlot_t *pPipeline = &m_szPipelines[hPipeline.nSlot];
				if ( !pPipeline->bActive || pPipeline->nGeneration != hPipeline.nGeneration ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				nativeOp.eKind = SIR_VULKAN_RECORDED_COMPUTE_DISPATCH;
				nativeOp.pPipeline = &pPipeline->native;
				nativeOp.nGroupCountX = op.nGroupCountX;
				nativeOp.nGroupCountY = op.nGroupCountY;
				nativeOp.nGroupCountZ = op.nGroupCountZ;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_RENDER_BEGIN ) {
				nativeOp.eKind = SIR_VULKAN_RECORDED_RENDER_BEGIN;
				nativeOp.nColorCount = op.nColorCount;
				for ( uint32_t iColor = 0; iColor < op.nColorCount; ++iColor ) {
					const sirGraphicsHandle_t hView = op.szColors[iColor].hView;
					if ( !sirGraphicsIsHandleValid( hView, SIR_GRAPHICS_HANDLE_VIEW, m_nDeviceGeneration ) ||
					     hView.nSlot >= SIR_VULKAN_VIEW_CAPACITY ) {
						return SIR_GRAPHICS_INVALID_HANDLE;
					}
					const sirVulkanGraphicsViewSlot_t& view = m_szViews[hView.nSlot];
					if ( !view.bActive || view.nGeneration != hView.nGeneration ) {
						return SIR_GRAPHICS_INVALID_HANDLE;
					}
					sirVulkanGraphicsImageSlot_t& image = m_szImages[view.nImageSlot];
					nativeOp.szColorViews[iColor] = view.native.hView;
					nativeOp.szColorImages[iColor] = &image.native;
					nativeOp.szColorMips[iColor] = view.nBaseMip;
					nativeOp.szColorLayers[iColor] = view.nBaseLayer;
					for ( uint32_t iChannel = 0; iChannel < 4; ++iChannel ) {
						nativeOp.szClearColors[iColor].color.float32[iChannel] = op.szColors[iColor].szClearColor[iChannel];
					}
					nativeOp.szColorClear[iColor] = op.szColors[iColor].bClear;
					nativeOp.szColorStore[iColor] = op.szColors[iColor].bStore;
					nativeOp.extent = { image.nWidth >> view.nBaseMip, image.nHeight >> view.nBaseMip };
					if ( nativeOp.extent.width == 0 )
						nativeOp.extent.width = 1;
					if ( nativeOp.extent.height == 0 )
						nativeOp.extent.height = 1;
				}
				if ( op.hDepthView.nGeneration != 0 ) {
					const sirGraphicsHandle_t hView = op.hDepthView;
					if ( !sirGraphicsIsHandleValid( hView, SIR_GRAPHICS_HANDLE_VIEW, m_nDeviceGeneration ) ||
					     hView.nSlot >= SIR_VULKAN_VIEW_CAPACITY ) {
						return SIR_GRAPHICS_INVALID_HANDLE;
					}
					const sirVulkanGraphicsViewSlot_t& view = m_szViews[hView.nSlot];
					if ( !view.bActive || view.nGeneration != hView.nGeneration ) {
						return SIR_GRAPHICS_INVALID_HANDLE;
					}
					sirVulkanGraphicsImageSlot_t& image = m_szImages[view.nImageSlot];
					nativeOp.hDepthView = view.native.hView;
					nativeOp.pDepthImage = &image.native;
					nativeOp.nDepthMip = view.nBaseMip;
					nativeOp.nDepthLayer = view.nBaseLayer;
					nativeOp.extent = { image.nWidth >> view.nBaseMip, image.nHeight >> view.nBaseMip };
					if ( nativeOp.extent.width == 0 )
						nativeOp.extent.width = 1;
					if ( nativeOp.extent.height == 0 )
						nativeOp.extent.height = 1;
				}
				nativeOp.depthClear.depthStencil = { op.fClearDepth, op.nClearStencil };
				nativeOp.bClearDepth = op.bClearDepth;
				nativeOp.bStoreDepth = op.bStoreDepth;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_RENDER_END ) {
				nativeOp.eKind = SIR_VULKAN_RECORDED_RENDER_END;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_BIND_PIPELINE ) {
				if ( !sirGraphicsIsHandleValid( op.hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, m_nDeviceGeneration ) ||
				     op.hPipeline.nSlot >= SIR_VULKAN_PIPELINE_CAPACITY ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				sirVulkanGraphicsPipelineSlot_t& pipeline = m_szPipelines[op.hPipeline.nSlot];
				if ( !pipeline.bActive || !pipeline.bGraphics || pipeline.nGeneration != op.hPipeline.nGeneration ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				nativeOp.eKind = SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE;
				nativeOp.pPipeline = &pipeline.native;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_VERTEX_BUFFER || op.eKind == SIR_VULKAN_GRAPHICS_INDEX_BUFFER ) {
				sirVulkanGraphicsBufferSlot_t *pBuffer = resolveBuffer( op.hSource );
				if ( pBuffer == nullptr || pBuffer->pMapped != nullptr ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				nativeOp.eKind =
				    op.eKind == SIR_VULKAN_GRAPHICS_VERTEX_BUFFER ? SIR_VULKAN_RECORDED_VERTEX_BUFFER : SIR_VULKAN_RECORDED_INDEX_BUFFER;
				nativeOp.pBuffer = &pBuffer->native;
				nativeOp.nBufferOffset = op.nSourceOffset;
				nativeOp.nBufferSlot = op.nBufferSlot;
				nativeOp.eIndexType = op.bIndex32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_VIEWPORT ) {
				nativeOp.eKind = SIR_VULKAN_RECORDED_VIEWPORT;
				nativeOp.viewport = {
				    op.viewport.fX,           op.viewport.fY, op.viewport.fWidth, op.viewport.fHeight, op.viewport.fMinimumDepth,
				    op.viewport.fMaximumDepth };
				nativeOp.scissor = {
				    { op.scissor.nLeft, op.scissor.nTop },
				    { uint32_t( op.scissor.nRight - op.scissor.nLeft ), uint32_t( op.scissor.nBottom - op.scissor.nTop ) } };
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_DRAW || op.eKind == SIR_VULKAN_GRAPHICS_DRAW_INDEXED ) {
				nativeOp.eKind = op.eKind == SIR_VULKAN_GRAPHICS_DRAW ? SIR_VULKAN_RECORDED_DRAW : SIR_VULKAN_RECORDED_DRAW_INDEXED;
				nativeOp.nDrawCount = op.nDrawCount;
				nativeOp.nInstanceCount = op.nInstanceCount;
				nativeOp.nFirstElement = op.nFirstElement;
				nativeOp.nFirstInstance = op.nFirstInstance;
				nativeOp.nVertexOffset = op.nVertexOffset;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_BIND_DESCRIPTOR_SET ) {
				const sirGraphicsHandle_t hSet = op.hBindingSet;
				if ( !sirGraphicsIsHandleValid( hSet, SIR_GRAPHICS_HANDLE_BINDING_SET, m_nDeviceGeneration ) ||
				     hSet.nSlot >= SIR_VULKAN_BINDING_SET_CAPACITY )
					return SIR_GRAPHICS_INVALID_HANDLE;
				sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[hSet.nSlot];
				if ( !set.bActive || set.nGeneration != hSet.nGeneration || set.nSetIndex != op.nSetIndex ||
				     set.hPipeline.nSlot != op.hPipeline.nSlot || set.hPipeline.nGeneration != op.hPipeline.nGeneration )
					return SIR_GRAPHICS_INVALID_HANDLE;
				if ( op.hPipeline.nSlot >= SIR_VULKAN_PIPELINE_CAPACITY )
					return SIR_GRAPHICS_INVALID_HANDLE;
				sirVulkanGraphicsPipelineSlot_t& pipeline = m_szPipelines[op.hPipeline.nSlot];
				if ( !pipeline.bActive || pipeline.nGeneration != op.hPipeline.nGeneration )
					return SIR_GRAPHICS_INVALID_HANDLE;
				for ( uint32_t iRef = 0; iRef < set.nReferenceCount; ++iRef ) {
					const sirGraphicsHandle_t hResource = set.szReferences[iRef];
					if ( hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
						if ( resolveBuffer( hResource ) == nullptr )
							return SIR_GRAPHICS_INVALID_HANDLE;
					} else if ( hResource.eType == SIR_GRAPHICS_HANDLE_VIEW ) {
						if ( hResource.nSlot >= SIR_VULKAN_VIEW_CAPACITY || !m_szViews[hResource.nSlot].bActive ||
						     m_szViews[hResource.nSlot].nGeneration != hResource.nGeneration )
							return SIR_GRAPHICS_INVALID_HANDLE;
					} else if ( hResource.eType == SIR_GRAPHICS_HANDLE_SAMPLER ) {
						if ( hResource.nSlot >= SIR_VULKAN_SAMPLER_CAPACITY || !m_szSamplers[hResource.nSlot].bActive ||
						     m_szSamplers[hResource.nSlot].nGeneration != hResource.nGeneration )
							return SIR_GRAPHICS_INVALID_HANDLE;
					} else
						return SIR_GRAPHICS_INVALID_HANDLE;
				}
				nativeOp.eKind = SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET;
				nativeOp.pDescriptorSet = &set.native;
				nativeOp.pPipeline = &pipeline.native;
				continue;
			}
			if ( op.eKind == SIR_VULKAN_GRAPHICS_BUFFER_BARRIER ) {
				sirVulkanGraphicsBufferSlot_t *pBuffer = resolveBuffer( op.barrier.hResource );
				if ( pBuffer == nullptr ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				if ( pBuffer->pMapped != nullptr ) {
					return SIR_GRAPHICS_BUSY;
				}
				nativeOp.eKind = SIR_VULKAN_RECORDED_BUFFER_BARRIER;
				nativeOp.pBuffer = &pBuffer->native;
				const sirGraphicsResult_t ePlan = sirVulkanPlanBufferBarrier( op.barrier, &pBuffer->native, &nativeOp.bufferBarrier );
				if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
					return ePlan;
				}
				continue;
			}
			const sirGraphicsHandle_t hTexture =
			    op.eKind == SIR_VULKAN_GRAPHICS_TEXTURE_COPY ? op.textureCopy.hTexture : op.barrier.hResource;
			if ( hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE || hTexture.nDeviceGeneration != m_nDeviceGeneration ||
			     hTexture.nSlot >= SIR_VULKAN_TEXTURE_CAPACITY ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			sirVulkanGraphicsImageSlot_t *pImage = &m_szImages[hTexture.nSlot];
			if ( !pImage->bActive || pImage->nGeneration != hTexture.nGeneration ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			sirVulkanImage_t *pShadow = nullptr;
			for ( uint32_t iImage = 0; iImage < nImages; ++iImage ) {
				if ( szOriginals[iImage] == &pImage->native ) {
					pShadow = &szShadows[iImage];
					break;
				}
			}
			if ( pShadow == nullptr ) {
				if ( nImages >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
					return SIR_GRAPHICS_CAPACITY_EXCEEDED;
				}
				szOriginals[nImages] = &pImage->native;
				szShadows[nImages] = pImage->native;
				pShadow = &szShadows[nImages++];
			}
			nativeOp.pImage = &pImage->native;
			if ( op.eKind == SIR_VULKAN_GRAPHICS_TEXTURE_COPY ) {
				const sirGraphicsBufferTextureCopy_t& copy = op.textureCopy;
				sirVulkanGraphicsBufferSlot_t *pBuffer = resolveBuffer( copy.hBuffer );
				if ( pBuffer == nullptr ) {
					return SIR_GRAPHICS_INVALID_HANDLE;
				}
				if ( pBuffer->pMapped != nullptr ) {
					return SIR_GRAPHICS_BUSY;
				}
				sirVulkanFormatBlock_t block = {};
				if ( !sirVulkanGetFormatBlock( pImage->native.eFormat, &block ) ||
				     ( copy.nBytesPerRow != 0 && copy.nBytesPerRow % block.nBytesPerBlock != 0 ) ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
				sirVulkanTextureTransfer_t transfer = {};
				transfer.nBufferOffset = copy.nBufferOffset;
				transfer.nBufferRowLength = copy.nBytesPerRow == 0 ? 0 : ( copy.nBytesPerRow / block.nBytesPerBlock ) * block.nBlockWidth;
				transfer.nBufferImageHeight = copy.nRowsPerImage;
				transfer.nMip = copy.nMip;
				transfer.nLayer = copy.nLayer;
				transfer.offset = { ( int32_t )copy.nX, ( int32_t )copy.nY, ( int32_t )copy.nZ };
				transfer.extent = { copy.nWidth, copy.nHeight, copy.nDepth };
				transfer.bBufferToImage = copy.bBufferToTexture;
				nativeOp.eKind = SIR_VULKAN_RECORDED_TEXTURE_COPY;
				nativeOp.pBuffer = &pBuffer->native;
				nativeOp.textureCopy = transfer;
				const sirVulkanResult_t eCheck = sirVulkanValidateTextureTransfer( nativeOp.pBuffer, pShadow, transfer );
				if ( eCheck != SIR_VULKAN_SUCCESS ) {
					return sirVulkanToGraphicsResult( eCheck );
				}
				if ( transfer.bBufferToImage ) {
					const VkImageLayout eFinal = sirVulkanUploadFinalLayout( pShadow, transfer );
					if ( sirVulkanTransferCoversSubresource( pShadow, transfer ) ) {
						const uint32_t nIndex = transfer.nLayer * pShadow->nMips + transfer.nMip;
						pShadow->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
					}
					( void )sirVulkanSetSubresourceLayout( pShadow, transfer.nMip, transfer.nLayer, eFinal );
				}
			} else if ( op.eKind == SIR_VULKAN_GRAPHICS_IMAGE_BARRIER ) {
				nativeOp.eKind = SIR_VULKAN_RECORDED_IMAGE_BARRIER;
				const sirGraphicsResult_t ePlan = sirVulkanPlanImageBarrier( op.barrier, pShadow, &nativeOp.imageBarrier );
				if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
					return ePlan;
				}
				for ( uint32_t iLayer = 0; iLayer < nativeOp.imageBarrier.nLayerCount; ++iLayer ) {
					for ( uint32_t iMip = 0; iMip < nativeOp.imageBarrier.nMipCount; ++iMip ) {
						( void )sirVulkanSetSubresourceLayout( pShadow, nativeOp.imageBarrier.nMip + iMip,
						                                       nativeOp.imageBarrier.nLayer + iLayer, nativeOp.imageBarrier.eAfter );
					}
				}
			} else {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	sirVulkanCompletion_t native = {};
	const sirVulkanResult_t eSubmit = m_Native.submitRecordedCommands( commands, nNativeCount, &native );
	if ( eSubmit != SIR_VULKAN_SUCCESS ) {
		return sirVulkanToGraphicsResult( eSubmit );
	}
	m_nLastCompletion = native.nValue;
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		const SIRVulkanCommandList& list = *szLists[iList];
		for ( uint32_t iOp = 0; iOp < list.m_nOperationCount; ++iOp ) {
			const sirVulkanGraphicsOperation_t& op = list.m_szOperations[iOp];
			if ( op.eKind == SIR_VULKAN_GRAPHICS_RENDER_BEGIN ) {
				for ( uint32_t iColor = 0; iColor < op.nColorCount; ++iColor ) {
					m_szViews[op.szColors[iColor].hView.nSlot].nLastGpuUse = native.nValue;
				}
				if ( op.hDepthView.nGeneration != 0 )
					m_szViews[op.hDepthView.nSlot].nLastGpuUse = native.nValue;
			} else if ( op.eKind == SIR_VULKAN_GRAPHICS_BIND_DESCRIPTOR_SET ) {
				const sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[op.hBindingSet.nSlot];
				for ( uint32_t iRef = 0; iRef < set.nReferenceCount; ++iRef ) {
					const sirGraphicsHandle_t hResource = set.szReferences[iRef];
					if ( hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
						sirVulkanGraphicsBufferSlot_t *pBuffer = resolveBuffer( hResource );
						if ( pBuffer != nullptr )
							pBuffer->native.nLastGpuUse = native.nValue;
					} else if ( hResource.eType == SIR_GRAPHICS_HANDLE_VIEW ) {
						sirVulkanGraphicsViewSlot_t& view = m_szViews[hResource.nSlot];
						view.nLastGpuUse = native.nValue;
						m_szImages[view.nImageSlot].native.nLastGpuUse = native.nValue;
					} else if ( hResource.eType == SIR_GRAPHICS_HANDLE_SAMPLER ) {
						m_szSamplers[hResource.nSlot].nLastGpuUse = native.nValue;
					}
				}
			}
		}
	}
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		SIRVulkanCommandList *pList = szLists[iList];
		pList->m_eState = SIR_VULKAN_COMMAND_SUBMITTED;
		pList->m_nLastCompletion = native.nValue;
	}
	*pCompletion = { m_nDeviceGeneration, SIR_GRAPHICS_QUEUE_DIRECT, native.nValue };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanRenderingDevice::poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept
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

sirGraphicsResult_t SIRVulkanRenderingDevice::wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNs ) noexcept
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
                                                              const sirGraphicsCompletionSet_t& completion ) noexcept
{
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_BINDING_SET ) {
		if ( !m_bReady || !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ||
		     ( completion.nQueueMask & ~( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) != 0 ||
		     completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] > m_nLastCompletion )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		if ( !sirGraphicsIsHandleValid( hResource, SIR_GRAPHICS_HANDLE_BINDING_SET, m_nDeviceGeneration ) ||
		     hResource.nSlot >= SIR_VULKAN_BINDING_SET_CAPACITY )
			return SIR_GRAPHICS_INVALID_HANDLE;
		sirVulkanGraphicsBindingSlot_t& set = m_szBindingSets[hResource.nSlot];
		if ( !set.bActive || set.nGeneration != hResource.nGeneration )
			return SIR_GRAPHICS_INVALID_HANDLE;
		if ( completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] < set.native.nLastGpuUse )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		set.bActive = false;
		set.bRetiring = true;
		set.nRetireValue = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
		++set.nGeneration;
		collectRetired();
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( hResource.eType == SIR_GRAPHICS_HANDLE_PIPELINE ) {
		if ( !m_bReady || !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ||
		     ( completion.nQueueMask & ~( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) != 0 ||
		     completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] > m_nLastCompletion ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !sirGraphicsIsHandleValid( hResource, SIR_GRAPHICS_HANDLE_PIPELINE, m_nDeviceGeneration ) ||
		     hResource.nSlot >= SIR_VULKAN_PIPELINE_CAPACITY ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		sirVulkanGraphicsPipelineSlot_t *pSlot = &m_szPipelines[hResource.nSlot];
		if ( !pSlot->bActive || pSlot->nGeneration != hResource.nGeneration ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] < pSlot->native.nLastGpuUse ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		pSlot->bActive = false;
		pSlot->bRetiring = true;
		pSlot->nRetireValue = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
		++pSlot->nGeneration;
		collectRetired();
		return SIR_GRAPHICS_SUCCESS;
	}
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
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_BINDING_SET_CAPACITY; ++iSlot ) {
		if ( m_szBindingSets[iSlot].bActive || m_szBindingSets[iSlot].bRetiring )
			return SIR_GRAPHICS_BUSY;
	}
	for ( uint32_t iSlot = 0; iSlot < SIR_VULKAN_PIPELINE_CAPACITY; ++iSlot ) {
		if ( m_szPipelines[iSlot].bActive || m_szPipelines[iSlot].bRetiring ) {
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
	const sirVulkanResult_t eResult = probe.probeAdapters( 0, pCount, nullptr );
	return sirVulkanToGraphicsResult( eResult );
}

sirGraphicsResult_t SIRVulkanDriver::queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept
{
	if ( pInfo == nullptr || nAdapterIndex >= 32 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pInfo = {};
	SIRVulkanDevice probe;
	sirVulkanAdapterInfo_t native = {};
	const sirVulkanResult_t eResult = probe.probeAdapters( nAdapterIndex, nullptr, &native );
	if ( eResult != SIR_VULKAN_SUCCESS ) {
		return sirVulkanToGraphicsResult( eResult );
	}
	return sirVulkanDescribeAdapter( nAdapterIndex, native, pInfo );
}

sirGraphicsResult_t SIRVulkanDriver::createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept
{
	if ( ppDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDevice = nullptr;
	if ( desc.nAdapterIndex >= 32 || desc.nFramesInFlight == 0 || desc.nFramesInFlight > SIR_VULKAN_TRANSFER_SLOT_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( desc.nRequiredFeatures & ~( uint64_t )( SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_MRT |
	                                               SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !desc.bHeadless ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bHasBudgets = desc.nResidentBudgetBytes != 0 || desc.nUploadBudgetBytes != 0 || desc.nReadbackBudgetBytes != 0;
	const sirVulkanPoolBudget_t budgets = { desc.nResidentBudgetBytes, desc.nUploadBudgetBytes, desc.nReadbackBudgetBytes };
	if ( bHasBudgets && !sirVulkanValidatePoolBudget( &budgets ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eResult = m_Device.initialize( bHasBudgets ? &budgets : nullptr, desc.nAdapterIndex );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	sirGraphicsCapabilities_t capabilities = {};
	if ( m_Device.queryCapabilities( &capabilities ) != SIR_GRAPHICS_SUCCESS ||
	     sirVulkanValidateDeviceAdmission( desc, capabilities ) != SIR_GRAPHICS_SUCCESS ) {
		( void )m_Device.shutdown();
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*ppDevice = &m_Device;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRVulkanDriver::destroyDevice( SIRRenderingDevice *pDevice ) noexcept
{
	if ( pDevice != &m_Device ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return m_Device.shutdown();
}
