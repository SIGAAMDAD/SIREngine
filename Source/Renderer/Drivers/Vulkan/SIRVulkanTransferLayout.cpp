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

// Each 2-bit entry addresses one independently tracked mip/layer pair.
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
	const uint32_t nShift = ( nIndex & 3U ) * 2U;
	const uint8_t nValue = ( pImage->szSubresourceLayouts[nIndex / 4] >> nShift ) & 3U;
	switch ( nValue ) {
	case 0:
		return true;
	case 1:
		*pLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return true;
	case 2:
		*pLayout = VK_IMAGE_LAYOUT_GENERAL;
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
	default:
		return false;
	}
	const uint32_t nShift = ( nIndex & 3U ) * 2U;
	const uint8_t nMask = ( uint8_t )( 3U << nShift );
	uint8_t *pEntry = &pImage->szSubresourceLayouts[nIndex / 4];
	*pEntry = ( uint8_t )( ( *pEntry & ~nMask ) | ( nValue << nShift ) );
	return true;
}

bool sirVulkanTransferCoversSubresource( const sirVulkanImage_t *pImage, const sirVulkanTextureTransfer_t& transfer ) noexcept
{
	if ( pImage == nullptr || !transfer.bBufferToImage || pImage->nMips == 0 || pImage->nMips > 32 || transfer.nMip >= pImage->nMips ||
	     pImage->nLayers == 0 || transfer.nLayer >= pImage->nLayers || pImage->nWidth == 0 || pImage->nHeight == 0 ||
	     pImage->nDepth == 0 ) {
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
	     pImage->nMips > 32 || pImage->nLayers == 0 ) {
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

	const uint64_t nCopyBlockRows = ( uint64_t( transfer.extent.height ) + block.nBlockHeight - 1 ) / block.nBlockHeight;
	const uint64_t nCopyBlockColumns = ( uint64_t( transfer.extent.width ) + block.nBlockWidth - 1 ) / block.nBlockWidth;
	const uint64_t nRowBlocks = transfer.nBufferRowLength != 0 ? transfer.nBufferRowLength / block.nBlockWidth : nCopyBlockColumns;
	const uint64_t nRowPitch = nRowBlocks * block.nBytesPerBlock;
	const uint64_t nLastRowBytes = nCopyBlockColumns * block.nBytesPerBlock;
	if ( nRowPitch == 0 || nCopyBlockRows == 0 || nCopyBlockRows - 1 > ( UINT64_MAX - nLastRowBytes ) / nRowPitch ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint64_t nLastSliceBytes = ( nCopyBlockRows - 1 ) * nRowPitch + nLastRowBytes;
	const uint64_t nImageRows = transfer.nBufferImageHeight != 0 ? transfer.nBufferImageHeight / block.nBlockHeight : nCopyBlockRows;
	if ( nImageRows > UINT64_MAX / nRowPitch ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint64_t nSlicePitch = nImageRows * nRowPitch;
	const uint64_t nCopySlices = transfer.extent.depth;
	if ( nCopySlices - 1 > ( UINT64_MAX - nLastSliceBytes ) / nSlicePitch ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	const uint64_t nRequiredBytes = ( nCopySlices - 1 ) * nSlicePitch + nLastSliceBytes;
	if ( nRequiredBytes > pBuffer->nSize - transfer.nBufferOffset ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	return SIR_VULKAN_SUCCESS;
}
