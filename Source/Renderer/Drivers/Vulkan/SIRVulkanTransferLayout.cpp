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

// CPU-only Vulkan image footprint contract. No Vulkan loader, device or GPU
// is needed to validate format blocks, row pitches and subresource ranges.
bool sirVulkanGetFormatBlock( VkFormat eFormat, sirVulkanFormatBlock_t *pBlock ) noexcept
{
	if ( pBlock == nullptr ) {
		return false;
	}
	*pBlock = {};
	switch ( eFormat ) {
	case VK_FORMAT_R8_UNORM:
		*pBlock = { 1, 1, 1 };
		break;
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R16_SFLOAT:
		*pBlock = { 1, 1, 2 };
		break;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32_UINT:
		*pBlock = { 1, 1, 4 };
		break;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R32G32_SFLOAT:
		*pBlock = { 1, 1, 8 };
		break;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		*pBlock = { 1, 1, 16 };
		break;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		*pBlock = { 4, 4, 8 };
		break;
	case VK_FORMAT_BC3_UNORM_BLOCK:
	case VK_FORMAT_BC5_UNORM_BLOCK:
	case VK_FORMAT_BC7_UNORM_BLOCK:
		*pBlock = { 4, 4, 16 };
		break;
	default:
		return false;
	}
	return true;
}

sirVulkanResult_t sirVulkanValidateTextureTransfer( const sirVulkanBuffer_t *pBuffer, const sirVulkanImage_t *pImage,
                                                    const sirVulkanTextureTransfer_t &transfer ) noexcept
{
	if ( pBuffer == nullptr || pImage == nullptr || pImage->eType != VK_IMAGE_TYPE_2D ||
	     pImage->nMips == 0 || pImage->nMips > 32 || pImage->nLayers == 0 ||
	     pImage->nLayers > SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / pImage->nMips ||
	     transfer.nMip >= pImage->nMips || transfer.nLayer >= pImage->nLayers ||
	     transfer.extent.width == 0 || transfer.extent.height == 0 || transfer.extent.depth != 1 ||
	     transfer.offset.z != 0 || transfer.offset.x < 0 || transfer.offset.y < 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	sirVulkanFormatBlock_t block = {};
	if ( !sirVulkanGetFormatBlock( pImage->eFormat, &block ) ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	uint32_t nMipWidth = pImage->nWidth >> transfer.nMip;
	uint32_t nMipHeight = pImage->nHeight >> transfer.nMip;
	if ( nMipWidth == 0 ) {
		nMipWidth = 1;
	}
	if ( nMipHeight == 0 ) {
		nMipHeight = 1;
	}
	const uint32_t nX = ( uint32_t )transfer.offset.x;
	const uint32_t nY = ( uint32_t )transfer.offset.y;
	if ( nX >= nMipWidth || nY >= nMipHeight ||
	     transfer.extent.width > nMipWidth - nX || transfer.extent.height > nMipHeight - nY ||
	     ( nX % block.nBlockWidth ) != 0 || ( nY % block.nBlockHeight ) != 0 ||
	     ( transfer.extent.width % block.nBlockWidth != 0 && transfer.extent.width != nMipWidth - nX ) ||
	     ( transfer.extent.height % block.nBlockHeight != 0 && transfer.extent.height != nMipHeight - nY ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( transfer.nBufferRowLength != 0 &&
	     ( transfer.nBufferRowLength < transfer.extent.width ||
	       transfer.nBufferRowLength % block.nBlockWidth != 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( transfer.nBufferImageHeight != 0 &&
	     ( transfer.nBufferImageHeight < transfer.extent.height ||
	       transfer.nBufferImageHeight % block.nBlockHeight != 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t nOffsetAlignment = block.nBytesPerBlock >= 4 ? block.nBytesPerBlock : 4;
	if ( transfer.nBufferOffset % nOffsetAlignment != 0 || transfer.nBufferOffset > pBuffer->nSize ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	const bool bUpload = transfer.bBufferToImage;
	if ( bUpload && ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 ||
	                  ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !bUpload && ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 ||
	                   ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t nIndex = transfer.nLayer * pImage->nMips + transfer.nMip;
	if ( !bUpload && ( pImage->szUploadedSubresources[nIndex / 64] & ( 1ULL << ( nIndex % 64 ) ) ) == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	const uint64_t nCopyBlockRows = ( uint64_t( transfer.extent.height ) + block.nBlockHeight - 1 ) / block.nBlockHeight;
	const uint64_t nCopyBlockColumns = ( uint64_t( transfer.extent.width ) + block.nBlockWidth - 1 ) / block.nBlockWidth;
	const uint64_t nRowBlocks = transfer.nBufferRowLength != 0 ? transfer.nBufferRowLength / block.nBlockWidth : nCopyBlockColumns;
	const uint64_t nRowPitch = nRowBlocks * block.nBytesPerBlock;
	const uint64_t nLastRowBytes = nCopyBlockColumns * block.nBytesPerBlock;
	if ( nRowPitch == 0 || nCopyBlockRows == 0 ||
	     nCopyBlockRows - 1 > ( UINT64_MAX - nLastRowBytes ) / nRowPitch ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint64_t nRequiredBytes = ( nCopyBlockRows - 1 ) * nRowPitch + nLastRowBytes;
	if ( nRequiredBytes > pBuffer->nSize - transfer.nBufferOffset ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	return SIR_VULKAN_SUCCESS;
}
