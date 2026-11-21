/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/Drivers/Vulkan/SIRVulkanCreationGuard.h>
#include <type_traits>
#include <stdint.h>

static uint32_t s_nDestroyed = 0;
static uint64_t s_nDestroyedMask = 0;

static VKAPI_ATTR void VKAPI_CALL vkTestDestroyShaderModule( VkDevice, VkShaderModule hModule, const VkAllocationCallbacks * )
{
	++s_nDestroyed;
	s_nDestroyedMask |= ( uint64_t )hModule;
}

int main( void )
{
	static_assert( !std::is_copy_constructible<SIRVulkanCreationGuard<VkShaderModule, PFN_vkDestroyShaderModule>>::value,
	               "Native ownership cannot copy" );
	static_assert( !std::is_copy_assignable<SIRVulkanCreationGuard<VkShaderModule, PFN_vkDestroyShaderModule>>::value,
	               "Native ownership cannot copy" );
	const VkDevice hFakeDevice = ( VkDevice )( uintptr_t )1;
	const PFN_vkDestroyShaderModule vkDestroyShaderModule = &vkTestDestroyShaderModule;
	{
		VkShaderModule hModule = ( VkShaderModule )1;
		SIRVulkanCreationGuard<VkShaderModule, PFN_vkDestroyShaderModule> guard( hFakeDevice, &hModule, vkDestroyShaderModule );
		guard.reset();
		guard.reset();
		if ( hModule != VK_NULL_HANDLE || s_nDestroyed != 1 ) {
			return 1;
		}
	}
	if ( s_nDestroyed != 1 ) {
		return 2;
	}
	{
		VkShaderModule hModule = ( VkShaderModule )2;
		SIRVulkanCreationGuard<VkShaderModule, PFN_vkDestroyShaderModule> guard( hFakeDevice, &hModule, vkDestroyShaderModule );
		guard.release();
		if ( hModule == VK_NULL_HANDLE ) {
			return 3;
		}
	}
	if ( s_nDestroyed != 1 ) {
		return 4;
	}
	{
		VkShaderModule szHandles[4] = { ( VkShaderModule )4, VK_NULL_HANDLE, ( VkShaderModule )8, VK_NULL_HANDLE };
		SIRVulkanCreationArrayGuard<VkShaderModule, PFN_vkDestroyShaderModule> guard( hFakeDevice, szHandles, 4, vkDestroyShaderModule );
	}
	if ( s_nDestroyed != 3 || s_nDestroyedMask != ( 1U | 4U | 8U ) ) {
		return 5;
	}
	{
		VkShaderModule szHandles[2] = { ( VkShaderModule )16, ( VkShaderModule )32 };
		SIRVulkanCreationArrayGuard<VkShaderModule, PFN_vkDestroyShaderModule> guard( hFakeDevice, szHandles, 2, vkDestroyShaderModule );
		guard.release();
	}
	return s_nDestroyed == 3 ? 0 : 6;
}
