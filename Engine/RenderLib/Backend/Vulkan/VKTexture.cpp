#include "VKBuffer.h"
#include "VKTexture.h"
#include "../ImageLoader.h"

using namespace SIREngine::RenderLib::Backend::Vulkan;

VKTexture::VKTexture( const TextureInit_t& textureInfo )
{
	Upload( textureInfo );
}

VKTexture::~VKTexture()
{
	if ( m_hImage ) {
		vmaDestroyImage( g_pVKContext->GetAllocator(), m_hImage, m_hImageMemory );
	}
}

void VKTexture::StreamBuffer( void )
{
	void *pMemory;

	VK_CALL( vmaMapMemory( g_pVKContext->GetAllocator(), m_hImageMemory, &pMemory ) );
	if ( pMemory ) {
		memcpy( pMemory, m_ImageData.GetBuffer(), m_ImageData.GetSize() );
		vmaUnmapMemory( g_pVKContext->GetAllocator(), m_hImageMemory );
	}
}

void VKTexture::Upload( const TextureInit_t& textureInfo )
{
	VkImageCreateInfo imageInfo;
	memset( &imageInfo, 0, sizeof( imageInfo ) );
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = textureInfo.nWidth;
	imageInfo.extent.height = textureInfo.nHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( m_bIsGPUOnly ) {
		if ( m_nImageFormat == TF_DEPTH || m_nImageFormat == TF_DEPTH_STENCIL ) {
			imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		} else {
			imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}

		switch ( m_nImageFormat ) {
		case TF_R:
			imageInfo.format = VK_FORMAT_R8_SRGB;
			break;
		case TF_RGB:
			imageInfo.format = VK_FORMAT_R8G8B8_SRGB;
			break;
		case TF_RGBA:
			imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		case TF_RGBA16F:
			imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			break;
		case TF_RGBA32F:
			imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		case TF_DEPTH: {
			const std::initializer_list<VkFormat> szFormats = {
				VK_FORMAT_D32_SFLOAT,
				VK_FORMAT_D24_UNORM_S8_UINT
			};
			imageInfo.format = g_pVKContext->GetSupportedFormat( szFormats.begin(), szFormats.size(),
				VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
			break; }
		case TF_DEPTH_STENCIL: {
			const std::initializer_list<VkFormat> szFormats = {
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT,
			};
			imageInfo.format = g_pVKContext->GetSupportedFormat( szFormats.begin(), szFormats.size(),
				VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
			break; }
		};

		VmaAllocationCreateInfo allocInfo;
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocInfo.priority = 1.0f;

		VkBufferCreateInfo bufferInfo;
		memset( &bufferInfo, 0, sizeof( bufferInfo ) );
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.size = m_ImageData.GetSize();
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VK_CALL( vmaCreateImage( g_pVKContext->GetAllocator(), &imageInfo, &allocInfo, &m_hImage, &m_hImageMemory, NULL ) );
	}
	else {
		m_ImageData.Load( textureInfo.filePath) ;

		m_nWidth = m_ImageData.GetWidth();
		m_nHeight = m_ImageData.GetHeight();

		imageInfo.extent.width = m_nWidth;
		imageInfo.extent.height = m_nHeight;

		VmaAllocationCreateInfo allocInfo;
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocInfo.priority = 1.0f;

		if ( m_ImageData.GetChannels() == 3 ) {
			imageInfo.format = VK_FORMAT_R8G8B8_SRGB;
		} else {
			imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		}

		VkBufferCreateInfo bufferInfo;
		memset( &bufferInfo, 0, sizeof( bufferInfo ) );
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferInfo.size = m_ImageData.GetSize();
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationInfo allocData;

		VK_CALL( vmaCreateImage( g_pVKContext->GetAllocator(), &imageInfo, &allocInfo, &m_hImage, &m_hImageMemory, &allocData ) );
		memcpy( allocData.pMappedData, m_ImageData.GetBuffer(), m_ImageData.GetSize() );
		vmaUnmapMemory( g_pVKContext->GetAllocator(), m_hImageMemory );
	}
}