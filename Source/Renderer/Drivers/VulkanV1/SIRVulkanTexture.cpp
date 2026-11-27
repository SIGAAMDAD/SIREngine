#include "SIRVulkanTexture.h"

static VkFormat sirVulkanFormat( SIRRenderFormat format ) noexcept
{
	switch ( format ) {
	case SIRRenderFormat::SIR_RENDER_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RG16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RG32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

static VkImageAspectFlags sirVulkanTextureAspect( VkFormat format ) noexcept
{
	return sirVulkanImageAspectMask( format );
}

static VkImageViewType sirVulkanViewType( const sirVulkanImage_t& image, const SIRRenderViewDesc& description ) noexcept
{
	const uint32_t layers = description.subresources.nLayerCount;
	if ( image.eType == VK_IMAGE_TYPE_1D ) return layers == 1 ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	if ( image.eType == VK_IMAGE_TYPE_3D ) return VK_IMAGE_VIEW_TYPE_3D;
	if ( image.bCube ) return VK_IMAGE_VIEW_TYPE_CUBE;
	return layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
}

static VkImageUsageFlags sirVulkanImageUsage( uint64_t usage ) noexcept
{
	VkImageUsageFlags result = 0;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_SAMPLED ) ) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_STORAGE ) ) result |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_COLOR_ATTACHMENT ) ) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_DEPTH_STENCIL_ATTACHMENT ) ) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_TRANSFER_SOURCE ) ) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_TRANSFER_DESTINATION ) ) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_INPUT_ATTACHMENT ) ) result |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	return result;
}

SIRVulkanTexture::SIRVulkanTexture( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderTextureDesc& description ) noexcept
	: SIRRenderTexture( device, description )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN ) return;
	if ( description.nWidth == 0 || description.nHeight == 0 || description.nDepth == 0 || description.nArrayLayers == 0 ||
	     description.nMipLevels == 0 || description.nSampleCount == 0 || description.eFormat == SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ||
	     description.eMemory != SIRRenderMemoryClass::SIR_RENDER_MEMORY_GPU_LOCAL ||
	     ( description.nResourceFlags & ( static_cast<uint32_t>( SIRRenderResourceFlag::SIR_RENDER_RESOURCE_EXTERNALLY_OWNED ) |
	                                      static_cast<uint32_t>( SIRRenderResourceFlag::SIR_RENDER_RESOURCE_SPARSE ) ) ) != 0 ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_CUBE && ( description.nArrayLayers < 6 || description.nArrayLayers % 6 != 0 ) ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_1D && ( description.nHeight != 1 || description.nDepth != 1 ) ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_2D && description.nDepth != 1 ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_3D && description.nArrayLayers != 1 ) return;
	constexpr uint64_t nUsageMask = static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_SAMPLED ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_STORAGE ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_COLOR_ATTACHMENT ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_DEPTH_STENCIL_ATTACHMENT ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_TRANSFER_SOURCE ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_TRANSFER_DESTINATION ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_INPUT_ATTACHMENT );
	if ( ( description.nUsageMask & ~nUsageMask ) != 0 ) return;
	const VkFormat format = sirVulkanFormat( description.eFormat );
	if ( format == VK_FORMAT_UNDEFINED ) return;
	const bool depth = ( sirVulkanTextureAspect( format ) & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;
	const uint64_t usage = description.nUsageMask;
	if ( ( depth && ( usage & ( static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_STORAGE ) |
	                           static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_COLOR_ATTACHMENT ) ) ) != 0 ) ||
	     ( !depth && ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TEXTURE_DEPTH_STENCIL_ATTACHMENT ) ) != 0 ) ) return;
	VkImageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_1D ? VK_IMAGE_TYPE_1D : description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = { description.nWidth, description.nHeight, description.nDepth };
	info.mipLevels = description.nMipLevels;
	info.arrayLayers = description.nArrayLayers;
	if ( description.nSampleCount != 1 ) return;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = sirVulkanImageUsage( description.nUsageMask );
	if ( info.usage == 0 ) return;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TEXTURE_CUBE ) info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	static_cast<void>( getDevice<SIRVulkanDevice>().createImage( info, &m_Native ) );
}

SIRVulkanTexture::~SIRVulkanTexture() noexcept
{
	if ( m_Native.hImage != VK_NULL_HANDLE && getDevice().get() != nullptr ) {
		SIRVulkanDevice& device = getDevice<SIRVulkanDevice>();
		if ( m_Native.nLastGpuUse != 0 ) {
			const sirVulkanCompletion_t completion{ m_Native.nDeviceSerial, m_Native.nLastGpuUse };
			if ( device.waitCompletion( &completion, UINT64_MAX ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
				return;
			}
		}
		static_cast<void>( device.destroyImage( &m_Native ) );
	}
}

SIRRenderResult SIRVulkanTexture::getSubresourceLayout( const SIRRenderSubresourceRange& range, SIRRenderBufferRange *pLayout ) const noexcept
{
	if ( pLayout == nullptr || !isValid() || range.nMipCount != 1 || range.nLayerCount != 1 || range.nBaseMip >= m_Description.nMipLevels ||
	     range.nBaseLayer >= m_Description.nArrayLayers ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pLayout = {};
	const VkImageAspectFlags imageAspects = sirVulkanImageAspectMask( m_Native.eFormat );
	const VkImageAspectFlags requestedAspects = range.nAspectMask == 0 ? imageAspects : static_cast<VkImageAspectFlags>( range.nAspectMask );
	if ( requestedAspects == 0 || ( requestedAspects & ~imageAspects ) != 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	VkImageSubresource subresource{};
	subresource.aspectMask = requestedAspects;
	subresource.mipLevel = range.nBaseMip;
	subresource.arrayLayer = range.nBaseLayer;
	VkSubresourceLayout nativeLayout{};
	if ( m_Native.eTiling == VK_IMAGE_TILING_OPTIMAL ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	const SIRVulkanResult result = static_cast<const SIRVulkanDevice&>( *getDevice() ).getImageSubresourceLayout( &m_Native, subresource, &nativeLayout );
	if ( result != SIRVulkanResult::SIR_VULKAN_SUCCESS ) return result == SIRVulkanResult::SIR_VULKAN_INVALID_STATE ? SIRRenderResult::SIR_RENDER_DEVICE_LOST : SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	pLayout->nOffsetBytes = nativeLayout.offset;
	pLayout->nSizeBytes = nativeLayout.size;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRVulkanTextureView::SIRVulkanTextureView( const SIRSharedPtr<SIRRenderDevice>& device, const SIRSharedPtr<SIRRenderTexture>& texture,
                                            const SIRRenderViewDesc& description ) noexcept
	: SIRRenderTextureView( device, texture, description )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN || texture.get() == nullptr ||
	     texture->getDevice().get() != device.get() ) return;
	const SIRVulkanTexture *pTexture = dynamic_cast<const SIRVulkanTexture *>( texture.get() );
	if ( pTexture == nullptr ) return;
	const SIRVulkanTexture& nativeTexture = *pTexture;
	const SIRRenderSubresourceRange& range = description.subresources;
	if ( !nativeTexture.isValid() || range.nMipCount == 0 || range.nLayerCount == 0 || range.nBaseMip >= nativeTexture.getMipLevels() ||
	     range.nMipCount > nativeTexture.getMipLevels() - range.nBaseMip || range.nBaseLayer >= nativeTexture.getArrayLayers() ||
	     range.nLayerCount > nativeTexture.getArrayLayers() - range.nBaseLayer ) return;
	const VkFormat format = description.eFormat == SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ? nativeTexture.getNativeImage().eFormat : sirVulkanFormat( description.eFormat );
	if ( format == VK_FORMAT_UNDEFINED || format != nativeTexture.getNativeImage().eFormat ) return;
	const VkImageAspectFlags aspect = sirVulkanTextureAspect( format );
	if ( range.nAspectMask == 0 || ( static_cast<VkImageAspectFlags>( range.nAspectMask ) & ~aspect ) != 0 ) return;
	if ( nativeTexture.getNativeImage().bCube && ( range.nBaseLayer % 6 != 0 || range.nLayerCount != 6 ) ) return;
	VkImageViewCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = nativeTexture.getNativeImage().hImage;
	info.viewType = sirVulkanViewType( nativeTexture.getNativeImage(), description );
	info.format = format;
	info.subresourceRange = { static_cast<VkImageAspectFlags>( range.nAspectMask ), range.nBaseMip, range.nMipCount, range.nBaseLayer, range.nLayerCount };
	SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
	if ( nativeDevice.createImageView( &nativeTexture.getNativeImage(), info, &m_hView ) == SIRVulkanResult::SIR_VULKAN_SUCCESS &&
	     nativeDevice.registerNativeView( m_hView.hView, this, &m_Handle ) != SIRRenderResult::SIR_RENDER_SUCCESS ) {
		static_cast<void>( nativeDevice.destroyImageView( &m_hView ) );
	}
}

SIRVulkanTextureView::~SIRVulkanTextureView() noexcept
{
	if ( m_hView.hView != VK_NULL_HANDLE && getDevice().get() != nullptr ) {
		SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
		nativeDevice.unregisterNativeView( m_Handle );
		static_cast<void>( nativeDevice.destroyImageView( &m_hView ) );
	}
}

SIRVulkanSampler::SIRVulkanSampler( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderSamplerDesc& description ) noexcept
	: SIRRenderSampler( device, description )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN ) return;
	VkSamplerCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.minFilter = description.eMinFilter == SIRRenderFilter::SIR_RENDER_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.magFilter = description.eMagFilter == SIRRenderFilter::SIR_RENDER_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.mipmapMode = description.eMipFilter == SIRRenderMipFilter::SIR_RENDER_MIP_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	const auto addressMode = []( SIRRenderAddressMode mode ) noexcept {
		switch ( mode ) {
		case SIRRenderAddressMode::SIR_RENDER_ADDRESS_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case SIRRenderAddressMode::SIR_RENDER_ADDRESS_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case SIRRenderAddressMode::SIR_RENDER_ADDRESS_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	};
	info.addressModeU = addressMode( description.eAddressU );
	info.addressModeV = addressMode( description.eAddressV );
	info.addressModeW = addressMode( description.eAddressW );
	// The device helper deliberately rejects anisotropy until feature negotiation is exposed by the RHI.
	info.anisotropyEnable = VK_FALSE;
	info.maxAnisotropy = 1.0F;
	info.compareEnable = description.bCompareEnabled ? VK_TRUE : VK_FALSE;
	info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	info.minLod = description.fMinimumLod;
	info.maxLod = description.fMaximumLod;
	info.mipLodBias = description.fLodBias;
	static_cast<void>( getDevice<SIRVulkanDevice>().createSampler( info, &m_hSampler ) );
}

SIRVulkanSampler::~SIRVulkanSampler() noexcept
{
	if ( m_hSampler.hSampler != VK_NULL_HANDLE && getDevice().get() != nullptr ) static_cast<void>( static_cast<SIRVulkanDevice&>( *getDevice() ).destroySampler( &m_hSampler ) );
}
