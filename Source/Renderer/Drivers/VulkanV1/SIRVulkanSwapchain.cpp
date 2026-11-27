/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include "SIRVulkanSwapchain.h"
#include "SIRVulkanDevice.h"
#include "SIRVulkanCommandList.h"
#include <new>
#include <algorithm>

namespace {

SIRRenderResult resultFor( VkResult result ) noexcept
{
	switch ( result ) {
	case VK_SUCCESS: return SIRRenderResult::SIR_RENDER_SUCCESS;
	case VK_TIMEOUT: return SIRRenderResult::SIR_RENDER_TIMEOUT;
	case VK_NOT_READY: return SIRRenderResult::SIR_RENDER_BUSY;
	case VK_SUBOPTIMAL_KHR:
	case VK_ERROR_OUT_OF_DATE_KHR:
	case VK_ERROR_SURFACE_LOST_KHR: return SIRRenderResult::SIR_RENDER_SURFACE_CHANGED;
	case VK_ERROR_DEVICE_LOST: return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	case VK_ERROR_OUT_OF_HOST_MEMORY:
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return SIRRenderResult::SIR_RENDER_OUT_OF_MEMORY;
	default: return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	}
}

VkFormat nativeFormat( SIRRenderFormat format ) noexcept
{
	switch ( format ) {
	case SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R10G10B10A2_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	default: return VK_FORMAT_UNDEFINED;
	}
}

SIRRenderFormat renderFormat( VkFormat format ) noexcept
{
	switch ( format ) {
	case VK_FORMAT_B8G8R8A8_UNORM: return SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_UNORM;
	case VK_FORMAT_R8G8B8A8_UNORM: return SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_UNORM;
	case VK_FORMAT_B8G8R8A8_SRGB: return SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_SRGB;
	case VK_FORMAT_R8G8B8A8_SRGB: return SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_SRGB;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return SIRRenderFormat::SIR_RENDER_FORMAT_RGBA16_FLOAT;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return SIRRenderFormat::SIR_RENDER_FORMAT_R10G10B10A2_UNORM;
	default: return SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN;
	}
}

VkPresentModeKHR nativePresentMode( SIRRenderPresentMode mode ) noexcept
{
	switch ( mode ) {
	case SIRRenderPresentMode::SIR_RENDER_PRESENT_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
	case SIRRenderPresentMode::SIR_RENDER_PRESENT_MAILBOX: return VK_PRESENT_MODE_MAILBOX_KHR;
	case SIRRenderPresentMode::SIR_RENDER_PRESENT_FIFO_RELAXED: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	default: return VK_PRESENT_MODE_FIFO_KHR;
	}
}

SIRRenderPresentMode renderPresentMode( VkPresentModeKHR mode ) noexcept
{
	switch ( mode ) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return SIRRenderPresentMode::SIR_RENDER_PRESENT_IMMEDIATE;
	case VK_PRESENT_MODE_MAILBOX_KHR: return SIRRenderPresentMode::SIR_RENDER_PRESENT_MAILBOX;
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return SIRRenderPresentMode::SIR_RENDER_PRESENT_FIFO_RELAXED;
	default: return SIRRenderPresentMode::SIR_RENDER_PRESENT_FIFO;
	}
}

} // namespace

SIRVulkanSwapchain::SIRVulkanSwapchain( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderNativeSurface& native,
                                        const SIRRenderSwapchainDesc& description ) noexcept
    : SIRRenderSwapchain( device, description ), m_Native( native ), m_Requested( description )
{
}

SIRVulkanSwapchain::~SIRVulkanSwapchain() noexcept
{
	SIRVulkanDevice *device = nativeDevice();
	if ( device != nullptr && device->isReady() ) {
		if ( device->getNativeProcs().vkQueueWaitIdle != nullptr ) {
			static_cast<void>( device->getNativeProcs().vkQueueWaitIdle( device->getNativeGraphicsQueue() ) );
		}
		destroySwapchain();
		destroySurface();
	}
}

SIRVulkanDevice *SIRVulkanSwapchain::nativeDevice( void ) const noexcept
{
	return dynamic_cast<SIRVulkanDevice *>( getDevice().get() );
}

SIRRenderResult SIRVulkanSwapchain::createSurface( void ) noexcept
{
	SIRVulkanDevice *device = nativeDevice();
	if ( device == nullptr || !device->isReady() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	const sirVulkanProcs_t& procs = device->getNativeProcs();
	const VkInstance instance = device->getNativeInstance();
	if ( m_Native.pWindow == nullptr || procs.vkDestroySurfaceKHR == nullptr ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	switch ( m_Native.ePlatform ) {
#if defined( VK_USE_PLATFORM_WIN32_KHR )
	case SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_WIN32: {
		if ( procs.vkCreateWin32SurfaceKHR == nullptr || m_Native.pDisplayOrInstance == nullptr ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
		VkWin32SurfaceCreateInfoKHR info{};
		info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		info.hinstance = static_cast<HINSTANCE>( m_Native.pDisplayOrInstance );
		info.hwnd = static_cast<HWND>( m_Native.pWindow );
		return resultFor( procs.vkCreateWin32SurfaceKHR( instance, &info, nullptr, &m_Surface ) );
	}
#endif
#if defined( VK_USE_PLATFORM_XLIB_KHR )
	case SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_X11: {
		if ( procs.vkCreateXlibSurfaceKHR == nullptr || m_Native.pDisplayOrInstance == nullptr ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
		VkXlibSurfaceCreateInfoKHR info{};
		info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		info.dpy = static_cast<Display *>( m_Native.pDisplayOrInstance );
		info.window = static_cast<Window>( reinterpret_cast<uintptr_t>( m_Native.pWindow ) );
		return resultFor( procs.vkCreateXlibSurfaceKHR( instance, &info, nullptr, &m_Surface ) );
	}
#endif
#if defined( VK_USE_PLATFORM_WAYLAND_KHR )
	case SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_WAYLAND: {
		if ( procs.vkCreateWaylandSurfaceKHR == nullptr || m_Native.pDisplayOrInstance == nullptr ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
		VkWaylandSurfaceCreateInfoKHR info{};
		info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
		info.display = static_cast<wl_display *>( m_Native.pDisplayOrInstance );
		info.surface = static_cast<wl_surface *>( m_Native.pWindow );
		return resultFor( procs.vkCreateWaylandSurfaceKHR( instance, &info, nullptr, &m_Surface ) );
	}
#endif
	default: return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
}

void SIRVulkanSwapchain::destroySurface( void ) noexcept
{
	SIRVulkanDevice *device = nativeDevice();
	if ( m_Surface != VK_NULL_HANDLE && device != nullptr && device->getNativeProcs().vkDestroySurfaceKHR != nullptr ) {
		device->getNativeProcs().vkDestroySurfaceKHR( device->getNativeInstance(), m_Surface, nullptr );
	}
	m_Surface = VK_NULL_HANDLE;
}

void SIRVulkanSwapchain::destroySwapchain( void ) noexcept
{
	SIRVulkanDevice *device = nativeDevice();
	if ( device == nullptr ) return;
	const sirVulkanProcs_t& procs = device->getNativeProcs();
	for ( uint32_t i = 0; i < m_nFrameCount; ++i ) {
		if ( m_ViewHandles ) device->unregisterNativeView( m_ViewHandles[i] );
		if ( m_Views && m_Views[i] != VK_NULL_HANDLE ) procs.vkDestroyImageView( device->getNativeDevice(), m_Views[i], nullptr );
		if ( m_Frames && m_Frames[i].acquired != VK_NULL_HANDLE ) procs.vkDestroySemaphore( device->getNativeDevice(), m_Frames[i].acquired, nullptr );
		if ( m_Frames && m_Frames[i].rendered != VK_NULL_HANDLE ) procs.vkDestroySemaphore( device->getNativeDevice(), m_Frames[i].rendered, nullptr );
	}
	if ( m_Swapchain != VK_NULL_HANDLE ) {
		procs.vkDestroySwapchainKHR( device->getNativeDevice(), m_Swapchain, nullptr );
		--device->m_nLiveSwapchains;
	}
	m_Views.reset();
	m_ViewHandles.reset();
	m_Images.reset();
	m_Frames.reset();
	m_nFrameCount = 0;
	m_Swapchain = VK_NULL_HANDLE;
	m_bAcquired = false;
}

SIRRenderResult SIRVulkanSwapchain::initialize( void ) noexcept
{
	if ( m_Surface != VK_NULL_HANDLE || m_Swapchain != VK_NULL_HANDLE ) return SIRRenderResult::SIR_RENDER_BUSY;
	if ( m_Requested.nWidth == 0 || m_Requested.nHeight == 0 || m_Requested.nImageCount == 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	SIRRenderResult result = createSurface();
	if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) return result;
	result = createSwapchain( m_Requested.nWidth, m_Requested.nHeight );
	if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) destroySurface();
	return result;
}

SIRRenderResult SIRVulkanSwapchain::createSwapchain( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	SIRVulkanDevice *device = nativeDevice();
	if ( device == nullptr || !device->isReady() || m_Surface == VK_NULL_HANDLE ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	const sirVulkanProcs_t& procs = device->getNativeProcs();
	if ( procs.vkGetPhysicalDeviceSurfaceSupportKHR == nullptr || procs.vkGetPhysicalDeviceSurfaceCapabilitiesKHR == nullptr ||
	     procs.vkGetPhysicalDeviceSurfaceFormatsKHR == nullptr || procs.vkGetPhysicalDeviceSurfacePresentModesKHR == nullptr ||
	     procs.vkCreateSwapchainKHR == nullptr || procs.vkDestroySwapchainKHR == nullptr || procs.vkGetSwapchainImagesKHR == nullptr ||
	     procs.vkCreateImageView == nullptr || procs.vkDestroyImageView == nullptr || procs.vkCreateSemaphore == nullptr ||
	     procs.vkDestroySemaphore == nullptr || procs.vkAcquireNextImageKHR == nullptr || procs.vkQueuePresentKHR == nullptr ||
	     procs.vkQueueSubmit2 == nullptr || procs.vkQueueWaitIdle == nullptr ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	VkBool32 supported = VK_FALSE;
	VkResult vkResult = procs.vkGetPhysicalDeviceSurfaceSupportKHR( device->getNativePhysicalDevice(), device->getNativeGraphicsQueueFamilyIndex(), m_Surface, &supported );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	if ( supported == VK_FALSE ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	VkSurfaceCapabilitiesKHR capabilities{};
	vkResult = procs.vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device->getNativePhysicalDevice(), m_Surface, &capabilities );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	if ( ( capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) == 0 ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	uint32_t formatCount = 0, modeCount = 0;
	vkResult = procs.vkGetPhysicalDeviceSurfaceFormatsKHR( device->getNativePhysicalDevice(), m_Surface, &formatCount, nullptr );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	vkResult = procs.vkGetPhysicalDeviceSurfacePresentModesKHR( device->getNativePhysicalDevice(), m_Surface, &modeCount, nullptr );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	if ( formatCount == 0 || modeCount == 0 ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	std::unique_ptr<VkSurfaceFormatKHR[]> formats( new ( std::nothrow ) VkSurfaceFormatKHR[formatCount] );
	std::unique_ptr<VkPresentModeKHR[]> modes( new ( std::nothrow ) VkPresentModeKHR[modeCount] );
	if ( !formats || !modes ) return SIRRenderResult::SIR_RENDER_OUT_OF_MEMORY;
	vkResult = procs.vkGetPhysicalDeviceSurfaceFormatsKHR( device->getNativePhysicalDevice(), m_Surface, &formatCount, formats.get() );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	vkResult = procs.vkGetPhysicalDeviceSurfacePresentModesKHR( device->getNativePhysicalDevice(), m_Surface, &modeCount, modes.get() );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	VkSurfaceFormatKHR selected = formats[0];
	const VkFormat requested = nativeFormat( m_Requested.eFormat );
	if ( requested == VK_FORMAT_UNDEFINED ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	if ( formatCount == 1 && selected.format == VK_FORMAT_UNDEFINED ) {
		selected.format = requested;
		selected.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	for ( uint32_t i = 0; i < formatCount; ++i ) {
		if ( formats[i].format == requested && ( m_Requested.bHdr || formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) ) {
			selected = formats[i];
			break;
		}
	}
	if ( renderFormat( selected.format ) == SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ) {
		for ( uint32_t i = 0; i < formatCount; ++i ) if ( renderFormat( formats[i].format ) != SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ) { selected = formats[i]; break; }
		if ( renderFormat( selected.format ) == SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	VkPresentModeKHR selectedMode = VK_PRESENT_MODE_FIFO_KHR;
	const VkPresentModeKHR requestedMode = nativePresentMode( m_Requested.ePresentMode );
	for ( uint32_t i = 0; i < modeCount; ++i ) if ( modes[i] == requestedMode ) { selectedMode = requestedMode; break; }
	VkExtent2D extent{};
	if ( capabilities.currentExtent.width != UINT32_MAX ) extent = capabilities.currentExtent;
	else {
		extent.width = std::max( capabilities.minImageExtent.width, std::min( nWidth, capabilities.maxImageExtent.width ) );
		extent.height = std::max( capabilities.minImageExtent.height, std::min( nHeight, capabilities.maxImageExtent.height ) );
	}
	if ( extent.width == 0 || extent.height == 0 ) return SIRRenderResult::SIR_RENDER_SURFACE_CHANGED;
	uint32_t imageCount = std::max( m_Requested.nImageCount, capabilities.minImageCount );
	if ( capabilities.maxImageCount != 0 ) imageCount = std::min( imageCount, capabilities.maxImageCount );
	VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if ( ( capabilities.supportedCompositeAlpha & alpha ) == 0 ) {
		const VkCompositeAlphaFlagBitsKHR candidates[] = { VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
		for ( VkCompositeAlphaFlagBitsKHR candidate : candidates ) if ( capabilities.supportedCompositeAlpha & candidate ) { alpha = candidate; break; }
		if ( ( capabilities.supportedCompositeAlpha & alpha ) == 0 ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	VkSwapchainCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = m_Surface;
	info.minImageCount = imageCount;
	info.imageFormat = selected.format;
	info.imageColorSpace = selected.colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = ( capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	info.compositeAlpha = alpha;
	info.presentMode = selectedMode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = m_Swapchain;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	vkResult = procs.vkCreateSwapchainKHR( device->getNativeDevice(), &info, nullptr, &swapchain );
	if ( vkResult != VK_SUCCESS ) return resultFor( vkResult );
	uint32_t actualCount = 0;
	vkResult = procs.vkGetSwapchainImagesKHR( device->getNativeDevice(), swapchain, &actualCount, nullptr );
	if ( vkResult != VK_SUCCESS || actualCount == 0 ) {
		procs.vkDestroySwapchainKHR( device->getNativeDevice(), swapchain, nullptr );
		return vkResult == VK_SUCCESS ? SIRRenderResult::SIR_RENDER_DRIVER_ERROR : resultFor( vkResult );
	}
	std::unique_ptr<VkImage[]> images( new ( std::nothrow ) VkImage[actualCount]{} );
	std::unique_ptr<VkImageView[]> views( new ( std::nothrow ) VkImageView[actualCount]{} );
	std::unique_ptr<SIRRenderHandle[]> handles( new ( std::nothrow ) SIRRenderHandle[actualCount]{} );
	std::unique_ptr<FrameSync[]> frames( new ( std::nothrow ) FrameSync[actualCount]{} );
	if ( !images || !views || !handles || !frames ) {
		procs.vkDestroySwapchainKHR( device->getNativeDevice(), swapchain, nullptr );
		return SIRRenderResult::SIR_RENDER_OUT_OF_MEMORY;
	}
	vkResult = procs.vkGetSwapchainImagesKHR( device->getNativeDevice(), swapchain, &actualCount, images.get() );
	if ( vkResult != VK_SUCCESS ) {
		procs.vkDestroySwapchainKHR( device->getNativeDevice(), swapchain, nullptr );
		return resultFor( vkResult );
	}
	SIRRenderResult setupResult = SIRRenderResult::SIR_RENDER_SUCCESS;
	for ( uint32_t i = 0; i < actualCount; ++i ) {
		VkImageViewCreateInfo view{};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image = images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = selected.format;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.layerCount = 1;
		vkResult = procs.vkCreateImageView( device->getNativeDevice(), &view, nullptr, &views[i] );
		if ( vkResult != VK_SUCCESS ) { setupResult = resultFor( vkResult ); break; }
		setupResult = device->registerNativeView( views[i], nullptr, &handles[i] );
		if ( setupResult != SIRRenderResult::SIR_RENDER_SUCCESS ) break;
		VkSemaphoreCreateInfo semaphore{};
		semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkResult = procs.vkCreateSemaphore( device->getNativeDevice(), &semaphore, nullptr, &frames[i].acquired );
		if ( vkResult != VK_SUCCESS ) { setupResult = resultFor( vkResult ); break; }
		vkResult = procs.vkCreateSemaphore( device->getNativeDevice(), &semaphore, nullptr, &frames[i].rendered );
		if ( vkResult != VK_SUCCESS ) { setupResult = resultFor( vkResult ); break; }
	}
	if ( setupResult != SIRRenderResult::SIR_RENDER_SUCCESS ) {
		for ( uint32_t i = 0; i < actualCount; ++i ) {
			device->unregisterNativeView( handles[i] );
			if ( views[i] ) procs.vkDestroyImageView( device->getNativeDevice(), views[i], nullptr );
			if ( frames[i].acquired ) procs.vkDestroySemaphore( device->getNativeDevice(), frames[i].acquired, nullptr );
			if ( frames[i].rendered ) procs.vkDestroySemaphore( device->getNativeDevice(), frames[i].rendered, nullptr );
		}
		procs.vkDestroySwapchainKHR( device->getNativeDevice(), swapchain, nullptr );
		return setupResult;
	}
	destroySwapchain();
	m_Swapchain = swapchain;
	++device->m_nLiveSwapchains;
	m_Images = std::move( images );
	m_Views = std::move( views );
	m_ViewHandles = std::move( handles );
	m_Frames = std::move( frames );
	m_nFrameCount = actualCount;
	m_iNextFrame = 0;
	m_bSurfaceChanged = false;
	SIRRenderSwapchainDesc actual = m_Requested;
	actual.nWidth = extent.width;
	actual.nHeight = extent.height;
	actual.nImageCount = actualCount;
	actual.eFormat = renderFormat( selected.format );
	actual.ePresentMode = renderPresentMode( selectedMode );
	setPresentationProperties( actual );
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanSwapchain::acquire( uint64_t timeout, uint32_t *pImageIndex ) noexcept
{
	if ( pImageIndex == nullptr ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pImageIndex = UINT32_MAX;
	SIRVulkanDevice *device = nativeDevice();
	if ( device == nullptr || !device->isReady() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( m_Swapchain == VK_NULL_HANDLE || m_bSurfaceChanged ) return SIRRenderResult::SIR_RENDER_SURFACE_CHANGED;
	if ( m_bAcquired ) return SIRRenderResult::SIR_RENDER_BUSY;
	const sirVulkanProcs_t& procs = device->getNativeProcs();
	FrameSync& frame = m_Frames[m_iNextFrame];
	if ( frame.presented ) {
		const SIRRenderResult idle = resultFor( procs.vkQueueWaitIdle( device->getNativeGraphicsQueue() ) );
		if ( idle != SIRRenderResult::SIR_RENDER_SUCCESS ) return idle;
		frame.presented = false;
	}
	uint32_t image = UINT32_MAX;
	const VkResult vkResult = procs.vkAcquireNextImageKHR( device->getNativeDevice(), m_Swapchain, timeout, frame.acquired, VK_NULL_HANDLE, &image );
	if ( vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_ERROR_SURFACE_LOST_KHR ) m_bSurfaceChanged = true;
	if ( vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR ) return resultFor( vkResult );
	if ( image >= m_nFrameCount ) return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	m_iAcquiredFrame = m_iNextFrame;
	m_iAcquiredImage = image;
	m_iNextFrame = ( m_iNextFrame + 1 ) % m_nFrameCount;
	m_bAcquired = true;
	*pImageIndex = image;
	if ( vkResult == VK_SUBOPTIMAL_KHR ) m_bSurfaceChanged = true;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanSwapchain::present( const SIRRenderSubmission& submission ) noexcept
{
	SIRVulkanDevice *device = nativeDevice();
	if ( device == nullptr || !device->isReady() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( !m_bAcquired ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	if ( submission.waits.getSize() != 0 || submission.signals.getSize() != 0 ||
	     submission.waitValues.getSize() != 0 || submission.signalValues.getSize() != 0 ||
	     ( submission.commands.getSize() != 0 && submission.commands.getData() == nullptr ) ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	for ( uint32_t i = 0; i < submission.commands.getSize(); ++i ) {
		const auto *command = dynamic_cast<const SIRVulkanCommandList *>( submission.commands[i] );
		if ( command == nullptr || command->getDevice().get() != device || command->getQueueType() != SIRRenderQueueType::SIR_RENDER_QUEUE_GRAPHICS ) {
			return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
		}
	}
	const sirVulkanProcs_t& procs = device->getNativeProcs();
	const VkQueue queue = device->getNativeGraphicsQueue();
	FrameSync& frame = m_Frames[m_iAcquiredFrame];
	VkSemaphoreSubmitInfo wait{};
	wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	wait.semaphore = frame.acquired;
	wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 before{};
	before.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	before.waitSemaphoreInfoCount = 1;
	before.pWaitSemaphoreInfos = &wait;
	SIRRenderResult result = resultFor( procs.vkQueueSubmit2( queue, 1, &before, VK_NULL_HANDLE ) );
	if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) return result;
	if ( submission.commands.getSize() != 0 ) {
		result = device->getGraphicsQueue()->submit( submission );
		if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) return result;
	}
	VkSemaphoreSubmitInfo signal{};
	signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal.semaphore = frame.rendered;
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 after{};
	after.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	after.signalSemaphoreInfoCount = 1;
	after.pSignalSemaphoreInfos = &signal;
	result = resultFor( procs.vkQueueSubmit2( queue, 1, &after, VK_NULL_HANDLE ) );
	if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) return result;
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.rendered;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &m_iAcquiredImage;
	const VkResult vkResult = procs.vkQueuePresentKHR( queue, &presentInfo );
	if ( vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_ERROR_SURFACE_LOST_KHR || vkResult == VK_SUBOPTIMAL_KHR ) m_bSurfaceChanged = true;
	frame.presented = true;
	m_bAcquired = false;
	return resultFor( vkResult );
}

SIRRenderResult SIRVulkanSwapchain::resize( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( nWidth == 0 || nHeight == 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	SIRVulkanDevice *device = nativeDevice();
	if ( device == nullptr || !device->isReady() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if ( m_Surface == VK_NULL_HANDLE ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	if ( m_bAcquired ) return SIRRenderResult::SIR_RENDER_BUSY;
	const SIRRenderResult idle = resultFor( device->getNativeProcs().vkQueueWaitIdle( device->getNativeGraphicsQueue() ) );
	if ( idle != SIRRenderResult::SIR_RENDER_SUCCESS ) return idle;
	const SIRRenderResult result = createSwapchain( nWidth, nHeight );
	if ( result == SIRRenderResult::SIR_RENDER_SUCCESS ) {
		m_Requested.nWidth = nWidth;
		m_Requested.nHeight = nHeight;
	}
	return result;
}

VkImage SIRVulkanSwapchain::getNativeImage( uint32_t iImage ) const noexcept
{
	return iImage < m_nFrameCount ? m_Images[iImage] : VK_NULL_HANDLE;
}

VkImageView SIRVulkanSwapchain::getNativeImageView( uint32_t iImage ) const noexcept
{
	return iImage < m_nFrameCount ? m_Views[iImage] : VK_NULL_HANDLE;
}

SIRRenderHandle SIRVulkanSwapchain::getImageViewHandle( uint32_t iImage ) const noexcept
{
	return iImage < m_nFrameCount && m_ViewHandles ? m_ViewHandles[iImage] : SIRRenderHandle{};
}
