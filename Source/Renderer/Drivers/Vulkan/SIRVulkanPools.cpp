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
	return true;
}

void SIRVulkanDevice::destroySealedPools( void ) noexcept
{
	if ( m_Allocator != nullptr ) {
		for ( uint32_t iPool = 0; iPool < 3; ++iPool ) {
			if ( m_szSealedPools[iPool] != nullptr ) {
				vmaDestroyPool( m_Allocator, m_szSealedPools[iPool] );
				m_szSealedPools[iPool] = nullptr;
			}
		}
	}
	m_bSealedPools = false;
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
		if ( vmaFindMemoryTypeIndexForBufferInfo( m_Allocator, &probe, &selection, &nMemoryType ) != VK_SUCCESS ) {
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
	}
	// Every physical block now exists. Resource creation can only
	// suballocate these blocks; NEVER_ALLOCATE enforces the seal in VMA.
	m_bSealedPools = true;
	return SIR_VULKAN_SUCCESS;
}
