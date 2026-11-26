#include "SIRVulkanTexture.h"

static VkFormat sirVulkanFormat( SIRRenderFormat format ) noexcept
{
	switch ( format ) {
	case SIRRenderFormat::SIR_RENDER_R8: return VK_FORMAT_R8_UNORM;
	case SIRRenderFormat::SIR_RENDER_RG8: return VK_FORMAT_R8G8_UNORM;
	case SIRRenderFormat::SIR_RENDER_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_R16F: return VK_FORMAT_R16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RG16F: return VK_FORMAT_R16G16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_R32F: return VK_FORMAT_R32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RG32F: return VK_FORMAT_R32G32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
	case SIRRenderFormat::SIR_RENDER_D32F: return VK_FORMAT_D32_SFLOAT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

static VkImageAspectFlags sirVulkanTextureAspect( VkFormat format ) noexcept
{
	return sirVulkanImageAspectMask( format );
}

static VkImageViewType sirVulkanViewType( const sirVulkanImage_t& image, const sirRenderTextureViewDescription_t& description ) noexcept
{
	const uint32_t layers = description.range.nLayerCount;
	if ( image.eType == VK_IMAGE_TYPE_1D ) return layers == 1 ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	if ( image.eType == VK_IMAGE_TYPE_3D ) return VK_IMAGE_VIEW_TYPE_3D;
	if ( image.bCube ) return VK_IMAGE_VIEW_TYPE_CUBE;
	return layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
}

static VkImageUsageFlags sirVulkanImageUsage( uint64_t usage ) noexcept
{
	VkImageUsageFlags result = 0;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_SAMPLED ) ) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_STORAGE ) ) result |= VK_IMAGE_USAGE_STORAGE_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) ) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT ) ) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERSOURCE ) ) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERDESTINATION ) ) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_INPUTATTACHMENT ) ) result |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	return result;
}

SIRVulkanTexture::SIRVulkanTexture( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderTextureDescription_t& description ) noexcept
	: SIRRenderTexture( device, description )
{
	if ( device.get() == nullptr ) return;
	if ( description.nWidth == 0 || description.nHeight == 0 || description.nDepth == 0 || description.nLayers == 0 ||
	     description.nMipLevels == 0 || description.nSamples == 0 || description.eFormat == SIRRenderFormat::SIR_RENDER_UNKNOWN ||
	     description.eMemory != SIRRenderMemoryType::SIR_RENDER_DEVICELOCAL || description.bExternallyOwned || description.bSparse ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_CUBE && ( description.nLayers < 6 || description.nLayers % 6 != 0 ) ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_ONE && ( description.nHeight != 1 || description.nDepth != 1 ) ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_TWO && description.nDepth != 1 ) return;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_THREE && description.nLayers != 1 ) return;
	constexpr uint64_t nUsageMask = static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_SAMPLED ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_STORAGE ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERSOURCE ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_TRANSFERDESTINATION ) |
	                                static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_INPUTATTACHMENT );
	if ( ( description.nUsage & ~nUsageMask ) != 0 ) return;
	const VkFormat format = sirVulkanFormat( description.eFormat );
	if ( format == VK_FORMAT_UNDEFINED ) return;
	const bool depth = ( sirVulkanTextureAspect( format ) & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;
	const uint64_t usage = description.nUsage;
	if ( ( depth && ( usage & ( static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_STORAGE ) |
	                           static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_COLORATTACHMENT ) ) ) != 0 ) ||
	     ( !depth && ( usage & static_cast<uint64_t>( SIRRenderTextureUsage::SIR_RENDER_DEPTHSTENCILATTACHMENT ) ) != 0 ) ) return;
	VkImageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = description.eDimension == SIRRenderTextureDimension::SIR_RENDER_ONE ? VK_IMAGE_TYPE_1D : description.eDimension == SIRRenderTextureDimension::SIR_RENDER_THREE ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = { description.nWidth, description.nHeight, description.nDepth };
	info.mipLevels = description.nMipLevels;
	info.arrayLayers = description.nLayers;
	if ( description.nSamples != 1 ) return;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = sirVulkanImageUsage( description.nUsage );
	if ( info.usage == 0 ) return;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( description.eDimension == SIRRenderTextureDimension::SIR_RENDER_CUBE ) info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	static_cast<void>( static_cast<SIRVulkanDevice&>( *device ).createImage( info, &m_Native ) );
}

SIRVulkanTexture::~SIRVulkanTexture() noexcept
{
	if ( m_Native.hImage != VK_NULL_HANDLE && getDevice().get() != nullptr ) {
		SIRVulkanDevice& device = static_cast<SIRVulkanDevice&>( *getDevice() );
		static_cast<void>( device.waitIdle() );
		static_cast<void>( device.destroyImage( &m_Native ) );
	}
}

SIRRenderResult SIRVulkanTexture::getSubresourceLayout( const sirRenderSubresourceRange_t& range, sirRenderBufferRange_t *pLayout ) const noexcept
{
	if ( pLayout == nullptr || !isValid() || range.nMipCount != 1 || range.nLayerCount != 1 || range.nBaseMip >= m_Description.nMipLevels ||
	     range.nBaseLayer >= m_Description.nLayers ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	*pLayout = {};
	const VkImageAspectFlags imageAspects = sirVulkanImageAspectMask( m_Native.eFormat );
	const VkImageAspectFlags requestedAspects = range.nAspectMask == 0 ? imageAspects : static_cast<VkImageAspectFlags>( range.nAspectMask );
	if ( requestedAspects == 0 || ( requestedAspects & ~imageAspects ) != 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	VkImageSubresource subresource{};
	subresource.aspectMask = requestedAspects;
	subresource.mipLevel = range.nBaseMip;
	subresource.arrayLayer = range.nBaseLayer;
	VkSubresourceLayout nativeLayout{};
	if ( m_Native.eTiling == VK_IMAGE_TILING_OPTIMAL ) return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	const SIRVulkanResult result = static_cast<const SIRVulkanDevice&>( *getDevice() ).getImageSubresourceLayout( &m_Native, subresource, &nativeLayout );
	if ( result != SIRVulkanResult::SIR_VULKAN_SUCCESS ) return result == SIRVulkanResult::SIR_VULKAN_INVALID_STATE ? SIRRenderResult::SIR_RENDER_DEVICELOST : SIRRenderResult::SIR_RENDER_BACKENDERROR;
	pLayout->nOffset = nativeLayout.offset;
	pLayout->nSize = nativeLayout.size;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRVulkanTextureView::SIRVulkanTextureView( const SIRSharedPtr<SIRRenderDevice>& device, const SIRSharedPtr<SIRRenderTexture>& texture,
                                            const sirRenderTextureViewDescription_t& description ) noexcept
	: SIRRenderTextureView( device, texture, description )
{
	if ( device.get() == nullptr || texture.get() == nullptr ) return;
	const SIRVulkanTexture& nativeTexture = static_cast<const SIRVulkanTexture&>( *texture );
	const sirRenderSubresourceRange_t& range = description.range;
	if ( !nativeTexture.isValid() || range.nMipCount == 0 || range.nLayerCount == 0 || range.nBaseMip >= nativeTexture.getMipLevels() ||
	     range.nMipCount > nativeTexture.getMipLevels() - range.nBaseMip || range.nBaseLayer >= nativeTexture.getArrayLayers() ||
	     range.nLayerCount > nativeTexture.getArrayLayers() - range.nBaseLayer ) return;
	const VkFormat format = description.eFormat == SIRRenderFormat::SIR_RENDER_UNKNOWN ? nativeTexture.getNativeImage().eFormat : sirVulkanFormat( description.eFormat );
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
	static_cast<void>( static_cast<SIRVulkanDevice&>( *device ).createImageView( &nativeTexture.getNativeImage(), info, &m_hView ) );
}

SIRVulkanTextureView::~SIRVulkanTextureView() noexcept
{
	if ( m_hView.hView != VK_NULL_HANDLE && getDevice().get() != nullptr ) static_cast<void>( static_cast<SIRVulkanDevice&>( *getDevice() ).destroyImageView( &m_hView ) );
}

SIRVulkanSampler::SIRVulkanSampler( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderSamplerDescription& description ) noexcept
	: SIRRenderSampler( device, description )
{
	if ( device.get() == nullptr ) return;
	VkSamplerCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.minFilter = description.nMinFilter == static_cast<uint8_t>( SIRRenderFilter::SIR_RENDER_LINEAR ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.magFilter = description.nMagFilter == static_cast<uint8_t>( SIRRenderFilter::SIR_RENDER_LINEAR ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	info.mipmapMode = description.nMipmapFilter == static_cast<uint8_t>( SIRRenderMipFilter::SIR_RENDER_LINEAR ) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	const auto addressMode = []( uint8_t mode ) noexcept {
		switch ( static_cast<SIRRenderAddressMode>( mode ) ) {
		case SIRRenderAddressMode::SIR_RENDER_MIRROREDREPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case SIRRenderAddressMode::SIR_RENDER_CLAMPTOEDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case SIRRenderAddressMode::SIR_RENDER_CLAMPTOBORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	};
	info.addressModeU = addressMode( description.nAddressU );
	info.addressModeV = addressMode( description.nAddressV );
	info.addressModeW = addressMode( description.nAddressW );
	// The device helper deliberately rejects anisotropy until feature negotiation is exposed by the RHI.
	info.anisotropyEnable = VK_FALSE;
	info.maxAnisotropy = 1.0F;
	info.compareEnable = description.bCompare ? VK_TRUE : VK_FALSE;
	info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	info.minLod = description.fMinLod;
	info.maxLod = description.fMaxLod;
	info.mipLodBias = description.fLodBias;
	static_cast<void>( static_cast<SIRVulkanDevice&>( *device ).createSampler( info, &m_hSampler ) );
}

SIRVulkanSampler::~SIRVulkanSampler() noexcept
{
	if ( m_hSampler.hSampler != VK_NULL_HANDLE && getDevice().get() != nullptr ) static_cast<void>( static_cast<SIRVulkanDevice&>( *getDevice() ).destroySampler( &m_hSampler ) );
}
