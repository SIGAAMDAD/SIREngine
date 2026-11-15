/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVULKANCREATIONGUARD_H__
#define __SIRENGINE_SIRVULKANCREATIONGUARD_H__

#include <vulkan/vulkan.h>

// Owns an unpublished native handle in caller-provided stack storage. The
// caller releases ownership only after the driver publishes the object in its
// existing fixed-capacity slot table. No heap allocation or additional GPU
// backing is created by these guards.
template <typename Handle, typename DestroyProc>
class SIRVulkanCreationGuard final
{
private:
	VkDevice m_Device;
	Handle *m_pHandle;
	DestroyProc vkDestroy;

public:
	SIRVulkanCreationGuard( VkDevice hDevice, Handle *pHandle, DestroyProc destroyProc ) noexcept
	    : m_Device( hDevice ), m_pHandle( pHandle ), vkDestroy( destroyProc )
	{
	}

	SIRVulkanCreationGuard( const SIRVulkanCreationGuard& ) = delete;
	SIRVulkanCreationGuard& operator=( const SIRVulkanCreationGuard& ) = delete;

	~SIRVulkanCreationGuard( void ) noexcept
	{
		reset();
	}

	void reset( void ) noexcept
	{
		if ( m_pHandle != nullptr && *m_pHandle != VK_NULL_HANDLE ) {
			vkDestroy( m_Device, *m_pHandle, nullptr );
			*m_pHandle = VK_NULL_HANDLE;
		}
	}

	void release( void ) noexcept
	{
		m_pHandle = nullptr;
	}
};

template <typename Handle, typename DestroyProc>
class SIRVulkanCreationArrayGuard final
{
private:
	VkDevice m_Device;
	Handle *m_pHandles;
	uint32_t m_nCount;
	DestroyProc vkDestroy;

public:
	SIRVulkanCreationArrayGuard( VkDevice hDevice, Handle *pHandles, uint32_t nCount, DestroyProc destroyProc ) noexcept
	    : m_Device( hDevice ), m_pHandles( pHandles ), m_nCount( nCount ), vkDestroy( destroyProc )
	{
	}

	SIRVulkanCreationArrayGuard( const SIRVulkanCreationArrayGuard& ) = delete;
	SIRVulkanCreationArrayGuard& operator=( const SIRVulkanCreationArrayGuard& ) = delete;

	~SIRVulkanCreationArrayGuard( void ) noexcept
	{
		if ( m_pHandles != nullptr ) {
			for ( uint32_t i = 0; i < m_nCount; ++i ) {
				if ( m_pHandles[i] != VK_NULL_HANDLE ) {
					vkDestroy( m_Device, m_pHandles[i], nullptr );
					m_pHandles[i] = VK_NULL_HANDLE;
				}
			}
		}
	}

	void release( void ) noexcept
	{
		m_pHandles = nullptr;
	}
};

#endif // __SIRENGINE_SIRVULKANCREATIONGUARD_H__
