/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIR_VULKAN_SWAPCHAIN_H__
#define __SIR_VULKAN_SWAPCHAIN_H__

#include <Renderer/RHI/SIRRenderSwapchain.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanShared.h>
#include <memory>

class SIRVulkanDevice;

// Owns the VkSurfaceKHR created from native. The native window and display
// remain owned by the caller and must outlive this object.
class SIRVulkanSwapchain final : public SIRRenderSwapchain
{
public:
	SIRVulkanSwapchain( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderNativeSurface& native,
	                    const SIRRenderSwapchainDesc& description ) noexcept;
	~SIRVulkanSwapchain() noexcept override;

	SIRRenderResult initialize( void ) noexcept;
	SIRRenderResult acquire( uint64_t nTimeoutNanoseconds, uint32_t *pImageIndex ) noexcept override;
	SIRRenderResult present( const SIRRenderSubmission& submission ) noexcept override;
	SIRRenderResult resize( uint32_t nWidth, uint32_t nHeight ) noexcept override;

	VkImage getNativeImage( uint32_t iImage ) const noexcept;
	VkImageView getNativeImageView( uint32_t iImage ) const noexcept;
	SIRRenderHandle getImageViewHandle( uint32_t iImage ) const noexcept;
	VkSurfaceKHR getNativeSurface( void ) const noexcept { return m_Surface; }

private:
	struct FrameSync {
		VkSemaphore acquired{ VK_NULL_HANDLE };
		VkSemaphore rendered{ VK_NULL_HANDLE };
		bool presented{};
	};

	SIRRenderResult createSurface( void ) noexcept;
	SIRRenderResult createSwapchain( uint32_t nWidth, uint32_t nHeight ) noexcept;
	void destroySwapchain( void ) noexcept;
	void destroySurface( void ) noexcept;
	SIRVulkanDevice *nativeDevice( void ) const noexcept;

	SIRRenderNativeSurface m_Native{};
	SIRRenderSwapchainDesc m_Requested{};
	VkSurfaceKHR m_Surface{ VK_NULL_HANDLE };
	VkSwapchainKHR m_Swapchain{ VK_NULL_HANDLE };
	std::unique_ptr<VkImage[]> m_Images;
	std::unique_ptr<VkImageView[]> m_Views;
	std::unique_ptr<SIRRenderHandle[]> m_ViewHandles;
	std::unique_ptr<FrameSync[]> m_Frames;
	uint32_t m_nFrameCount{};
	uint32_t m_iNextFrame{};
	uint32_t m_iAcquiredFrame{};
	uint32_t m_iAcquiredImage{};
	bool m_bAcquired{};
	bool m_bSurfaceChanged{};
};

#endif // __SIR_VULKAN_SWAPCHAIN_H__
