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

VkImageAspectFlags sirVulkanImageAspectMask( VkFormat eFormat ) noexcept
{
	switch ( eFormat ) {
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	default:
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

// A single mip/layer state is shared between depth and stencil; D24S8 barriers
// always cover both aspects. Independent stencil layouts require a new tracker.
static bool sirVulkanImageLayoutAllowed( const sirVulkanImage_t *pImage, VkImageLayout eLayout ) noexcept
{
	const VkImageAspectFlags nAspects = sirVulkanImageAspectMask( pImage->eFormat );
	const bool bDepth = ( nAspects & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;
	switch ( eLayout ) {
	case VK_IMAGE_LAYOUT_UNDEFINED:
	case VK_IMAGE_LAYOUT_GENERAL:
		return true;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return !bDepth && ( pImage->nUsage & VK_IMAGE_USAGE_SAMPLED_BIT ) != 0;
	case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
		return !bDepth && ( pImage->nUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) != 0;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) != 0;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) != 0;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return bDepth && ( pImage->nUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		return bDepth && ( pImage->nUsage & ( VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ) ) != 0;
	default:
		return false;
	}
}

bool sirVulkanValidateNativeImageBarrier( const sirVulkanImage_t *pImage, const sirVulkanImageBarrier_t& barrier ) noexcept
{
	if ( pImage == nullptr || pImage->nMips == 0 || pImage->nMips > 32 || pImage->nLayers == 0 ||
	     pImage->nLayers > SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / pImage->nMips || barrier.nMipCount == 0 || barrier.nLayerCount == 0 ||
	     barrier.nMip >= pImage->nMips || barrier.nLayer >= pImage->nLayers || barrier.nMipCount > pImage->nMips - barrier.nMip ||
	     barrier.nLayerCount > pImage->nLayers - barrier.nLayer || !sirVulkanImageLayoutAllowed( pImage, barrier.eBefore ) ||
	     !sirVulkanImageLayoutAllowed( pImage, barrier.eAfter ) || barrier.eAfter == VK_IMAGE_LAYOUT_UNDEFINED ) {
		return false;
	}
	if ( barrier.bUav ) {
		if ( sirVulkanImageAspectMask( pImage->eFormat ) != VK_IMAGE_ASPECT_COLOR_BIT ||
		     ( pImage->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) == 0 || barrier.eBefore != VK_IMAGE_LAYOUT_GENERAL ||
		     barrier.eAfter != VK_IMAGE_LAYOUT_GENERAL ) {
			return false;
		}
	} else if ( barrier.eBefore == barrier.eAfter ) {
		return false;
	}
	const bool bRead = barrier.eAfter == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
	                   barrier.eAfter == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
	                   barrier.eAfter == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	for ( uint32_t iLayer = 0; iLayer < barrier.nLayerCount; ++iLayer ) {
		const uint32_t nLayer = barrier.nLayer + iLayer;
		for ( uint32_t iMip = 0; iMip < barrier.nMipCount; ++iMip ) {
			const uint32_t nMip = barrier.nMip + iMip;
			VkImageLayout eTracked = VK_IMAGE_LAYOUT_UNDEFINED;
			if ( !sirVulkanGetSubresourceLayout( pImage, nMip, nLayer, &eTracked ) || eTracked != barrier.eBefore ) {
				return false;
			}
			if ( bRead ) {
				const uint32_t nIndex = nLayer * pImage->nMips + nMip;
				if ( ( pImage->szUploadedSubresources[nIndex / 64] & ( 1ULL << ( nIndex % 64 ) ) ) == 0 ) {
					return false;
				}
			}
		}
	}
	return true;
}

// Each 4-bit entry addresses one independently tracked mip/layer pair.
// Values are final queued layouts; transient transfer layouts are command-local.
static bool sirVulkanSubresourceIndex( const sirVulkanImage_t *pImage, uint32_t nMip, uint32_t nLayer, uint32_t *pIndex ) noexcept
{
	if ( pImage == nullptr || pIndex == nullptr || pImage->nMips == 0 || pImage->nMips > 32 || pImage->nLayers == 0 ||
	     pImage->nLayers > SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / pImage->nMips || nMip >= pImage->nMips || nLayer >= pImage->nLayers ) {
		return false;
	}
	*pIndex = nLayer * pImage->nMips + nMip;
	return true;
}

bool sirVulkanGetSubresourceLayout( const sirVulkanImage_t *pImage, uint32_t nMip, uint32_t nLayer, VkImageLayout *pLayout ) noexcept
{
	if ( pLayout == nullptr ) {
		return false;
	}
	*pLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	uint32_t nIndex = 0;
	if ( !sirVulkanSubresourceIndex( pImage, nMip, nLayer, &nIndex ) ) {
		return false;
	}
	const uint32_t nShift = ( nIndex & 1U ) * 4U;
	const uint8_t nValue = ( pImage->szSubresourceLayouts[nIndex / 2] >> nShift ) & 15U;
	switch ( nValue ) {
	case 0:
		return true;
	case 1:
		*pLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return true;
	case 2:
		*pLayout = VK_IMAGE_LAYOUT_GENERAL;
		return true;
	case 3:
		*pLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		return true;
	case 4:
		*pLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		return true;
	case 5:
		*pLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		return true;
	case 6:
		*pLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		return true;
	case 7:
		*pLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		return true;
	default:
		return false;
	}
}

bool sirVulkanSetSubresourceLayout( sirVulkanImage_t *pImage, uint32_t nMip, uint32_t nLayer, VkImageLayout eLayout ) noexcept
{
	uint32_t nIndex = 0;
	if ( !sirVulkanSubresourceIndex( pImage, nMip, nLayer, &nIndex ) ) {
		return false;
	}
	uint8_t nValue = 0;
	switch ( eLayout ) {
	case VK_IMAGE_LAYOUT_UNDEFINED:
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		nValue = 1;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		nValue = 2;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		nValue = 3;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		nValue = 4;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		nValue = 5;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		nValue = 6;
		break;
	case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
		nValue = 7;
		break;
	default:
		return false;
	}
	const uint32_t nShift = ( nIndex & 1U ) * 4U;
	const uint8_t nMask = ( uint8_t )( 15U << nShift );
	uint8_t *pEntry = &pImage->szSubresourceLayouts[nIndex / 2];
	*pEntry = ( uint8_t )( ( *pEntry & ~nMask ) | ( nValue << nShift ) );
	return true;
}

bool sirVulkanTransferCoversSubresource( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer ) noexcept
{
	if ( pImage == nullptr || !transfer.bBufferToImage || pImage->nMips == 0 || pImage->nMips > 32 || transfer.nMip >= pImage->nMips ||
	     pImage->nLayers == 0 || pImage->nLayers > SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / pImage->nMips ||
	     transfer.nLayer >= pImage->nLayers || pImage->nWidth == 0 || pImage->nHeight == 0 ||
	     ( pImage->eType == VK_IMAGE_TYPE_3D && pImage->nDepth == 0 ) ) {
		return false;
	}
	const uint32_t nWidth = pImage->nWidth >> transfer.nMip;
	const uint32_t nHeight = pImage->nHeight >> transfer.nMip;
	const uint32_t nDepth = pImage->eType == VK_IMAGE_TYPE_3D ? pImage->nDepth >> transfer.nMip : 1U;
	return transfer.offset.x == 0 && transfer.offset.y == 0 && transfer.offset.z == 0 &&
	       transfer.extent.width == ( nWidth != 0 ? nWidth : 1U ) && transfer.extent.height == ( nHeight != 0 ? nHeight : 1U ) &&
	       transfer.extent.depth == ( nDepth != 0 ? nDepth : 1U );
}

VkImageLayout sirVulkanUploadFinalLayout( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer ) noexcept
{
	if ( pImage == nullptr || transfer.nMip >= pImage->nMips || transfer.nLayer >= pImage->nLayers || pImage->nMips == 0 ||
	     pImage->nMips > 32 || pImage->nLayers == 0 || pImage->nLayers > SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / pImage->nMips ) {
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
	const uint32_t nIndex = transfer.nLayer * pImage->nMips + transfer.nMip;
	const bool bInitialized = sirVulkanTransferCoversSubresource( pImage, transfer ) ||
	                          ( pImage->szUploadedSubresources[nIndex / 64] & ( 1ULL << ( nIndex % 64 ) ) ) != 0;
	if ( bInitialized && ( pImage->nUsage & VK_IMAGE_USAGE_SAMPLED_BIT ) != 0 && ( pImage->nUsage & VK_IMAGE_USAGE_STORAGE_BIT ) == 0 ) {
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	return VK_IMAGE_LAYOUT_GENERAL;
}

bool sirVulkanTextureTransferFootprint( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer,
                                        uint64_t *pBytes ) noexcept
{
	if ( pBytes == nullptr ) {
		return false;
	}
	*pBytes = 0;
	if ( pImage == nullptr || transfer.extent.width == 0 || transfer.extent.height == 0 || transfer.extent.depth == 0 ) {
		return false;
	}

	sirVulkanFormatBlock_t block = {};
	if ( !sirVulkanGetFormatBlock( pImage->eFormat, &block ) ) {
		return false;
	}
	if ( transfer.nBufferRowLength != 0 &&
	     ( transfer.nBufferRowLength < transfer.extent.width || transfer.nBufferRowLength % block.nBlockWidth != 0 ) ) {
		return false;
	}
	if ( transfer.nBufferImageHeight != 0 &&
	     ( transfer.nBufferImageHeight < transfer.extent.height || transfer.nBufferImageHeight % block.nBlockHeight != 0 ) ) {
		return false;
	}

	const uint64_t nBlockRows = ( uint64_t( transfer.extent.height ) + block.nBlockHeight - 1 ) / block.nBlockHeight;
	const uint64_t nBlockColumns = ( uint64_t( transfer.extent.width ) + block.nBlockWidth - 1 ) / block.nBlockWidth;
	const uint64_t nRowBlocks = transfer.nBufferRowLength != 0 ? transfer.nBufferRowLength / block.nBlockWidth : nBlockColumns;
	const uint64_t nRowPitch = nRowBlocks * block.nBytesPerBlock;
	const uint64_t nLastRowBytes = nBlockColumns * block.nBytesPerBlock;
	if ( nRowPitch == 0 || nBlockRows == 0 || nBlockRows - 1 > ( UINT64_MAX - nLastRowBytes ) / nRowPitch ) {
		return false;
	}
	const uint64_t nLastSliceBytes = ( nBlockRows - 1 ) * nRowPitch + nLastRowBytes;
	const uint64_t nImageRows = transfer.nBufferImageHeight != 0 ? transfer.nBufferImageHeight / block.nBlockHeight : nBlockRows;
	if ( nImageRows > UINT64_MAX / nRowPitch ) {
		return false;
	}
	const uint64_t nSlicePitch = nImageRows * nRowPitch;
	if ( uint64_t( transfer.extent.depth ) - 1 > ( UINT64_MAX - nLastSliceBytes ) / nSlicePitch ) {
		return false;
	}
	*pBytes = ( uint64_t( transfer.extent.depth ) - 1 ) * nSlicePitch + nLastSliceBytes;
	return true;
}

sirVulkanResult_t sirVulkanValidateTextureTransfer( const sirVulkanBuffer_t *pBuffer, const sirVulkanImage_t *pImage,
                                                    const sirVulkanTextureTransfer_t& transfer ) noexcept
{
	if ( pBuffer == nullptr || pImage == nullptr || ( pImage->eType != VK_IMAGE_TYPE_2D && pImage->eType != VK_IMAGE_TYPE_3D ) ||
	     pImage->nMips == 0 || pImage->nMips > 32 || pImage->nLayers == 0 ||
	     pImage->nLayers > SIR_VULKAN_MAX_TRACKED_SUBRESOURCES / pImage->nMips || transfer.nMip >= pImage->nMips ||
	     transfer.nLayer >= pImage->nLayers || transfer.extent.width == 0 || transfer.extent.height == 0 || transfer.extent.depth == 0 ||
	     transfer.offset.x < 0 || transfer.offset.y < 0 || transfer.offset.z < 0 ||
	     ( pImage->eType == VK_IMAGE_TYPE_2D && ( transfer.extent.depth != 1 || transfer.offset.z != 0 ) ) ||
	     ( pImage->eType == VK_IMAGE_TYPE_3D && ( pImage->nLayers != 1 || transfer.nLayer != 0 ) ) ) {
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
	uint32_t nMipDepth = pImage->nDepth >> transfer.nMip;
	if ( nMipDepth == 0 ) {
		nMipDepth = 1;
	}
	const uint32_t nZ = ( uint32_t )transfer.offset.z;
	if ( nZ >= nMipDepth || transfer.extent.depth > nMipDepth - nZ ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t nX = ( uint32_t )transfer.offset.x;
	const uint32_t nY = ( uint32_t )transfer.offset.y;
	if ( nX >= nMipWidth || nY >= nMipHeight || transfer.extent.width > nMipWidth - nX || transfer.extent.height > nMipHeight - nY ||
	     ( nX % block.nBlockWidth ) != 0 || ( nY % block.nBlockHeight ) != 0 ||
	     ( transfer.extent.width % block.nBlockWidth != 0 && transfer.extent.width != nMipWidth - nX ) ||
	     ( transfer.extent.height % block.nBlockHeight != 0 && transfer.extent.height != nMipHeight - nY ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( transfer.nBufferRowLength != 0 &&
	     ( transfer.nBufferRowLength < transfer.extent.width || transfer.nBufferRowLength % block.nBlockWidth != 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( transfer.nBufferImageHeight != 0 &&
	     ( transfer.nBufferImageHeight < transfer.extent.height || transfer.nBufferImageHeight % block.nBlockHeight != 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t nOffsetAlignment = block.nBytesPerBlock >= 4 ? block.nBytesPerBlock : 4;
	if ( transfer.nBufferOffset % nOffsetAlignment != 0 || transfer.nBufferOffset > pBuffer->nSize ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	const bool bUpload = transfer.bBufferToImage;
	if ( bUpload &&
	     ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 || ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( !bUpload &&
	     ( ( pBuffer->nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 || ( pImage->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint32_t nIndex = transfer.nLayer * pImage->nMips + transfer.nMip;
	if ( !bUpload && ( pImage->szUploadedSubresources[nIndex / 64] & ( 1ULL << ( nIndex % 64 ) ) ) == 0 ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}

	uint64_t nRequiredBytes = 0;
	if ( !sirVulkanTextureTransferFootprint( pImage, transfer, &nRequiredBytes ) ||
	     nRequiredBytes > pBuffer->nSize - transfer.nBufferOffset ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	return SIR_VULKAN_SUCCESS;
}
