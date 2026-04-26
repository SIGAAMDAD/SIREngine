#ifndef __VULKAN_SWAPCHAIN_H__
#define __VULKAN_SWAPCHAIN_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include "VulkanUtils.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanSwapChain
	{
	public:
		VulkanSwapChain( SDL_Window *pWindow, const SVulkanInstance& instance );
		~VulkanSwapChain();

		const VkExtent2D& GetExtent( void ) const;
		VkFormat GetImageFormat( void ) const;
		const VkSwapchainKHR GetSwapChain( void ) const;
		const VkFramebuffer GetFramebuffer( uint32_t nImageIndex ) const;
		uint32_t GetImageCount( void ) const;

		void Recreate( void );
	private:
		void CreateImageViews( void );
		void CreateFramebuffers( void );

		const SVulkanInstance& m_Instance;

		VkSwapchainKHR m_hSwapChain;

		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent;

		VkImage *m_parrImages;
		VkFramebuffer *m_parrFramebuffers;

		VkImageView *m_parrImageViews;
		uint32_t m_nImageCount;
	};

	SIRENGINE_FORCEINLINE uint32_t VulkanSwapChain::GetImageCount( void ) const
	{
		return m_nImageCount;
	}

	SIRENGINE_FORCEINLINE const VkExtent2D& VulkanSwapChain::GetExtent( void ) const
	{
		return m_SwapChainExtent;
	}

	SIRENGINE_FORCEINLINE VkFormat VulkanSwapChain::GetImageFormat( void ) const
	{
		return m_SwapChainImageFormat;
	}

	SIRENGINE_FORCEINLINE const VkSwapchainKHR VulkanSwapChain::GetSwapChain( void ) const
	{
		return m_hSwapChain;
	}

	SIRENGINE_FORCEINLINE const VkFramebuffer VulkanSwapChain::GetFramebuffer( uint32_t nImageIndex ) const
	{
		return m_parrFramebuffers[ nImageIndex ];
	}
};

#endif