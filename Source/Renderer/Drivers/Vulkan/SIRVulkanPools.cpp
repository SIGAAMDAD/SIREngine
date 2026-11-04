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

bool sirVulkanMemoryTypeCompatible( uint32_t nPoolMemoryType, uint32_t nResourceMemoryTypes ) noexcept
{
	return nPoolMemoryType < 32 && ( nResourceMemoryTypes & ( 1U << nPoolMemoryType ) ) != 0;
}

bool sirVulkanPlacementHostVisible( sirVulkanBufferPlacement_t ePlacement ) noexcept
{
	return ePlacement == SIR_VULKAN_BUFFER_CPU_UPLOAD || ePlacement == SIR_VULKAN_BUFFER_CPU_READBACK ||
	       ePlacement == SIR_VULKAN_BUFFER_GPU_UPLOAD;
}

uint32_t sirVulkanPlacementPool( sirVulkanBufferPlacement_t ePlacement ) noexcept
{
	return ePlacement == SIR_VULKAN_BUFFER_GPU_LOCAL ? 0U : ePlacement == SIR_VULKAN_BUFFER_CPU_READBACK ? 2U : 1U;
}

bool sirVulkanValidatePoolBudget( const sirVulkanPoolBudget_t *pBudget ) noexcept
{
	if ( pBudget == nullptr ) {
		return false;
	}
	constexpr uint64_t POOL_ALIGNMENT = 65536;
	const uint64_t szBudgets[3] = { pBudget->nResidentBytes, pBudget->nUploadBytes, pBudget->nReadbackBytes };
	for ( uint32_t iPool = 0; iPool < 3; ++iPool ) {
		if ( szBudgets[iPool] < POOL_ALIGNMENT || ( szBudgets[iPool] % POOL_ALIGNMENT ) != 0 ) {
			return false;
		}
	}
	return szBudgets[0] <= UINT64_MAX - szBudgets[1] && szBudgets[0] + szBudgets[1] <= UINT64_MAX - szBudgets[2];
}

void SIRVulkanDevice::destroySealedPools( void ) noexcept
{
	if ( m_Allocator != nullptr ) {
		for ( uint32_t iPool = 0; iPool < 3; ++iPool ) {
			if ( m_szSealedPools[iPool] != nullptr ) {
				vmaDestroyPool( m_Allocator, m_szSealedPools[iPool] );
				m_szSealedPools[iPool] = nullptr;
			}
			m_szSealedMemoryTypes[iPool] = 0;
		}
	}
	m_bSealedPools = false;
	m_nSealedBudgetBytes = 0;
}

sirVulkanResult_t SIRVulkanDevice::chooseResidentMemoryType( uint32_t *pMemoryType ) noexcept
{
	if ( m_Allocator == nullptr || m_Device == VK_NULL_HANDLE || pMemoryType == nullptr ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	*pMemoryType = UINT32_MAX;
	const PFN_vkCreateBuffer pfnCreateBuffer = ( PFN_vkCreateBuffer )m_pfnGetDeviceProcAddr( m_Device, "vkCreateBuffer" );
	const PFN_vkDestroyBuffer pfnDestroyBuffer = ( PFN_vkDestroyBuffer )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyBuffer" );
	const PFN_vkGetBufferMemoryRequirements pfnGetBufferRequirements =
	    ( PFN_vkGetBufferMemoryRequirements )m_pfnGetDeviceProcAddr( m_Device, "vkGetBufferMemoryRequirements" );
	const PFN_vkCreateImage pfnCreateImage = ( PFN_vkCreateImage )m_pfnGetDeviceProcAddr( m_Device, "vkCreateImage" );
	const PFN_vkDestroyImage pfnDestroyImage = ( PFN_vkDestroyImage )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyImage" );
	const PFN_vkGetImageMemoryRequirements pfnGetImageRequirements =
	    ( PFN_vkGetImageMemoryRequirements )m_pfnGetDeviceProcAddr( m_Device, "vkGetImageMemoryRequirements" );
	if ( pfnCreateBuffer == nullptr || pfnDestroyBuffer == nullptr || pfnGetBufferRequirements == nullptr || pfnCreateImage == nullptr ||
	     pfnDestroyImage == nullptr || pfnGetImageRequirements == nullptr ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	VkBufferCreateInfo probeBuffer = {};
	probeBuffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	probeBuffer.size = 4096;
	probeBuffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	probeBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkBuffer hBuffer = VK_NULL_HANDLE;
	if ( pfnCreateBuffer( m_Device, &probeBuffer, nullptr, &hBuffer ) != VK_SUCCESS ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	VkMemoryRequirements bufferRequirements = {};
	pfnGetBufferRequirements( m_Device, hBuffer, &bufferRequirements );
	pfnDestroyBuffer( m_Device, hBuffer, nullptr );

	VkImageCreateInfo probeImage = {};
	probeImage.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	probeImage.imageType = VK_IMAGE_TYPE_2D;
	probeImage.format = VK_FORMAT_R8G8B8A8_UNORM;
	probeImage.extent = { 64, 64, 1 };
	probeImage.mipLevels = 1;
	probeImage.arrayLayers = 1;
	probeImage.samples = VK_SAMPLE_COUNT_1_BIT;
	probeImage.tiling = VK_IMAGE_TILING_OPTIMAL;
	probeImage.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	probeImage.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	probeImage.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage hImage = VK_NULL_HANDLE;
	if ( pfnCreateImage( m_Device, &probeImage, nullptr, &hImage ) != VK_SUCCESS ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	VkMemoryRequirements imageRequirements = {};
	pfnGetImageRequirements( m_Device, hImage, &imageRequirements );
	pfnDestroyImage( m_Device, hImage, nullptr );

	const uint32_t nSharedTypes = bufferRequirements.memoryTypeBits & imageRequirements.memoryTypeBits;
	if ( nSharedTypes == 0 ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	VmaAllocationCreateInfo selection = {};
	selection.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	return vmaFindMemoryTypeIndex( m_Allocator, nSharedTypes, &selection, pMemoryType ) == VK_SUCCESS ? SIR_VULKAN_SUCCESS
	                                                                                                  : SIR_VULKAN_API_UNSUPPORTED;
}

sirVulkanResult_t SIRVulkanDevice::initializeSealedPools( const sirVulkanPoolBudget_t *pBudget ) noexcept
{
	if ( m_Allocator == nullptr || m_bSealedPools || !sirVulkanValidatePoolBudget( pBudget ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint64_t szBudgets[3] = { pBudget->nResidentBytes, pBudget->nUploadBytes, pBudget->nReadbackBytes };
	for ( uint32_t iPool = 0; iPool < 3; ++iPool ) {
		VkBufferCreateInfo probe = {};
		probe.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		probe.size = 4096;
		probe.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		probe.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo selection = {};
		selection.usage = VMA_MEMORY_USAGE_AUTO;
		if ( iPool == 0 ) {
			selection.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		} else {
			selection.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			selection.flags =
			    iPool == 1 ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
			if ( iPool == 2 ) {
				selection.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			}
		}
		uint32_t nMemoryType = UINT32_MAX;
		if ( iPool == 0 ) {
			if ( chooseResidentMemoryType( &nMemoryType ) != SIR_VULKAN_SUCCESS ) {
				destroySealedPools();
				return SIR_VULKAN_API_UNSUPPORTED;
			}
		} else if ( vmaFindMemoryTypeIndexForBufferInfo( m_Allocator, &probe, &selection, &nMemoryType ) != VK_SUCCESS ) {
			destroySealedPools();
			return SIR_VULKAN_API_UNSUPPORTED;
		}
		VmaPoolCreateInfo poolInfo = {};
		poolInfo.memoryTypeIndex = nMemoryType;
		poolInfo.blockSize = szBudgets[iPool];
		poolInfo.minBlockCount = 1;
		poolInfo.maxBlockCount = 1;
		if ( vmaCreatePool( m_Allocator, &poolInfo, &m_szSealedPools[iPool] ) != VK_SUCCESS ) {
			destroySealedPools();
			return SIR_VULKAN_ALLOCATION_FAILED;
		}
		m_szSealedMemoryTypes[iPool] = nMemoryType;
	}
	// Every physical block now exists. Resource creation can only
	// suballocate these blocks; NEVER_ALLOCATE enforces the seal in VMA.
	m_bSealedPools = true;
	m_nSealedBudgetBytes = szBudgets[0] + szBudgets[1] + szBudgets[2];
	updateMemoryHighWater();
	return SIR_VULKAN_SUCCESS;
}

void sirVulkanAdvanceMemoryPeak( uint64_t nCommittedBytes, uint64_t nAllocatedBytes, uint64_t *pPeakCommitted,
                                 uint64_t *pPeakAllocated ) noexcept
{
	if ( pPeakCommitted == nullptr || pPeakAllocated == nullptr ) {
		return;
	}
	if ( nCommittedBytes > *pPeakCommitted ) {
		*pPeakCommitted = nCommittedBytes;
	}
	if ( nAllocatedBytes > *pPeakAllocated ) {
		*pPeakAllocated = nAllocatedBytes;
	}
}

void SIRVulkanDevice::updateMemoryHighWater( void ) noexcept
{
	if ( m_Allocator == nullptr ) {
		return;
	}
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
	vmaGetHeapBudgets( m_Allocator, budgets );
	uint64_t nCommitted = 0;
	uint64_t nAllocated = 0;
	for ( uint32_t iHeap = 0; iHeap < m_Adapter.nMemoryHeapCount; ++iHeap ) {
		nCommitted += budgets[iHeap].statistics.blockBytes;
		nAllocated += budgets[iHeap].statistics.allocationBytes;
	}
	sirVulkanAdvanceMemoryPeak( nCommitted, nAllocated, &m_nPeakCommittedBytes, &m_nPeakAllocationBytes );
}

bool SIRVulkanDevice::isSealedMemoryType( const VkImageCreateInfo &info ) const noexcept
{
	if ( !m_bSealedPools ) {
		return true;
	}
	const PFN_vkCreateImage pfnCreateImage = ( PFN_vkCreateImage )m_pfnGetDeviceProcAddr( m_Device, "vkCreateImage" );
	const PFN_vkGetImageMemoryRequirements pfnGetRequirements =
	    ( PFN_vkGetImageMemoryRequirements )m_pfnGetDeviceProcAddr( m_Device, "vkGetImageMemoryRequirements" );
	const PFN_vkDestroyImage pfnDestroyImage = ( PFN_vkDestroyImage )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyImage" );
	if ( pfnCreateImage == nullptr || pfnGetRequirements == nullptr || pfnDestroyImage == nullptr ) {
		return false;
	}
	VkImage hProbe = VK_NULL_HANDLE;
	if ( pfnCreateImage( m_Device, &info, nullptr, &hProbe ) != VK_SUCCESS ) {
		return false;
	}
	VkMemoryRequirements requirements = {};
	pfnGetRequirements( m_Device, hProbe, &requirements );
	pfnDestroyImage( m_Device, hProbe, nullptr );
	const uint32_t nType = m_szSealedMemoryTypes[0];
	return sirVulkanMemoryTypeCompatible( nType, requirements.memoryTypeBits );
}

bool SIRVulkanDevice::isSealedBufferMemoryType( const VkBufferCreateInfo &info, uint32_t iPool ) const noexcept
{
	if ( !m_bSealedPools ) {
		return true;
	}
	if ( iPool >= 3 ) {
		return false;
	}
	const PFN_vkCreateBuffer pfnCreateBuffer = ( PFN_vkCreateBuffer )m_pfnGetDeviceProcAddr( m_Device, "vkCreateBuffer" );
	const PFN_vkGetBufferMemoryRequirements pfnGetRequirements =
	    ( PFN_vkGetBufferMemoryRequirements )m_pfnGetDeviceProcAddr( m_Device, "vkGetBufferMemoryRequirements" );
	const PFN_vkDestroyBuffer pfnDestroyBuffer = ( PFN_vkDestroyBuffer )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyBuffer" );
	if ( pfnCreateBuffer == nullptr || pfnGetRequirements == nullptr || pfnDestroyBuffer == nullptr ) {
		return false;
	}
	VkBuffer hProbe = VK_NULL_HANDLE;
	if ( pfnCreateBuffer( m_Device, &info, nullptr, &hProbe ) != VK_SUCCESS ) {
		return false;
	}
	VkMemoryRequirements requirements = {};
	pfnGetRequirements( m_Device, hProbe, &requirements );
	pfnDestroyBuffer( m_Device, hProbe, nullptr );
	return sirVulkanMemoryTypeCompatible( m_szSealedMemoryTypes[iPool], requirements.memoryTypeBits );
}
