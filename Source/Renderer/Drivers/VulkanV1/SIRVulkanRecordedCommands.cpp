/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

#include <string.h>

static constexpr uint32_t SIR_VULKAN_RECORDED_SHADOW_CAPACITY = SIR_VULKAN_COPY_BATCH_CAPACITY * 9;

static sirVulkanImage_t *sirVulkanRecordedShadow( sirVulkanImage_t *pImage, sirVulkanImage_t **ppOriginals, sirVulkanImage_t *pShadows,
                                                  uint32_t *pCount ) noexcept
{
	if ( pImage == nullptr || ppOriginals == nullptr || pShadows == nullptr || pCount == nullptr ) {
		return nullptr;
	}
	for ( uint32_t iImage = 0; iImage < *pCount; ++iImage ) {
		if ( ppOriginals[iImage] == pImage ) {
			return &pShadows[iImage];
		}
	}
	if ( *pCount >= SIR_VULKAN_RECORDED_SHADOW_CAPACITY ) {
		return nullptr;
	}
	ppOriginals[*pCount] = pImage;
	pShadows[*pCount] = *pImage;
	return &pShadows[( *pCount )++];
}

static bool sirVulkanRecordedFinalizePass( const sirVulkanRecordedCommand_t& pass, sirVulkanImage_t **ppOriginals,
                                           sirVulkanImage_t *pShadows, uint32_t *pCount ) noexcept
{
	for ( uint32_t i = 0; i <= pass.nColorCount; ++i ) {
		const bool bDepth = i == pass.nColorCount;
		sirVulkanImage_t *pImage = bDepth ? pass.pDepthImage : pass.szColorImages[i];
		if ( pImage == nullptr )
			continue;
		const uint32_t nMip = bDepth ? pass.nDepthMip : pass.szColorMips[i];
		const uint32_t nLayer = bDepth ? pass.nDepthLayer : pass.szColorLayers[i];
		sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( pImage, ppOriginals, pShadows, pCount );
		if ( pShadow == nullptr || nMip >= pShadow->nMips || nLayer >= pShadow->nLayers )
			return false;
		const uint32_t nIndex = nLayer * pShadow->nMips + nMip;
		const bool bStore = bDepth ? pass.bStoreDepth : pass.szColorStore[i];
		const bool bClear = bDepth ? pass.bClearDepth : pass.szColorClear[i];
		const uint64_t nBit = 1ULL << ( nIndex % 64 );
		if ( !bStore )
			pShadow->szUploadedSubresources[nIndex / 64] &= ~nBit;
		else if ( bClear )
			pShadow->szUploadedSubresources[nIndex / 64] |= nBit;
	}
	return true;
}

static SIRVulkanResult sirVulkanAdvanceRecordedState( sirVulkanImage_t *pShadow, const sirVulkanRecordedCommand_t& command ) noexcept
{
	if ( pShadow == nullptr ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY ) {
		const SIRVulkanResult eCheck = sirVulkanValidateTextureTransfer( command.pBuffer, pShadow, command.textureCopy );
		if ( eCheck != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return eCheck;
		}
		VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
		if ( !sirVulkanGetSubresourceLayout( pShadow, command.textureCopy.nMip, command.textureCopy.nLayer, &eOld ) ||
		     ( !command.textureCopy.bBufferToImage && eOld == VK_IMAGE_LAYOUT_UNDEFINED ) ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
		if ( command.textureCopy.bBufferToImage ) {
			const VkImageLayout eFinal = sirVulkanUploadFinalLayout( pShadow, command.textureCopy );
			if ( sirVulkanTransferCoversSubresource( pShadow, command.textureCopy ) ) {
				const uint32_t nIndex = command.textureCopy.nLayer * pShadow->nMips + command.textureCopy.nMip;
				pShadow->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
			}
			static_cast<void>(sirVulkanSetSubresourceLayout( pShadow, command.textureCopy.nMip, command.textureCopy.nLayer, eFinal ));
		}
		return SIRVulkanResult::SIR_VULKAN_SUCCESS;
	}
	if ( command.eKind != SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_BARRIER ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const sirVulkanImageBarrier_t& barrier = command.imageBarrier;
	if ( !sirVulkanValidateNativeImageBarrier( pShadow, barrier ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	for ( uint32_t iLayer = 0; iLayer < barrier.nLayerCount; ++iLayer ) {
		for ( uint32_t iMip = 0; iMip < barrier.nMipCount; ++iMip ) {
			static_cast<void>(sirVulkanSetSubresourceLayout( pShadow, barrier.nMip + iMip, barrier.nLayer + iLayer, barrier.eAfter ));
		}
	}
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

static bool sirVulkanValidateRecordedImageCopy( const sirVulkanImageCopy_t& copy, VkImageLayout *pSourceLayout,
                                                VkImageLayout *pDestinationLayout ) noexcept
{
	if ( copy.pSource == nullptr || copy.pDestination == nullptr || copy.pSource == copy.pDestination || pSourceLayout == nullptr ||
	     pDestinationLayout == nullptr || copy.pSource->hImage == VK_NULL_HANDLE || copy.pDestination->hImage == VK_NULL_HANDLE ||
	     copy.pSource->eFormat != copy.pDestination->eFormat ||
	     ( copy.pSource->nUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ||
	     ( copy.pDestination->nUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 || copy.region.extent.width == 0 ||
	     copy.region.extent.height == 0 || copy.region.extent.depth == 0 ||
	     copy.region.srcSubresource.aspectMask != sirVulkanImageAspectMask( copy.pSource->eFormat ) ||
	     copy.region.dstSubresource.aspectMask != sirVulkanImageAspectMask( copy.pDestination->eFormat ) ||
	     copy.region.srcSubresource.layerCount != 1 || copy.region.dstSubresource.layerCount != 1 ||
	     copy.region.srcSubresource.mipLevel >= copy.pSource->nMips || copy.region.dstSubresource.mipLevel >= copy.pDestination->nMips ||
	     copy.region.srcSubresource.baseArrayLayer >= copy.pSource->nLayers ||
	     copy.region.dstSubresource.baseArrayLayer >= copy.pDestination->nLayers ) {
		return false;
	}
	if ( copy.pSource->eType == VK_IMAGE_TYPE_3D || copy.pDestination->eType == VK_IMAGE_TYPE_3D ) {
		if ( copy.pSource->eType != VK_IMAGE_TYPE_3D || copy.pDestination->eType != VK_IMAGE_TYPE_3D ||
		     copy.region.srcSubresource.baseArrayLayer != 0 || copy.region.dstSubresource.baseArrayLayer != 0 ||
		     copy.pSource->nLayers != 1 || copy.pDestination->nLayers != 1 ) {
			return false;
		}
	} else if ( copy.region.extent.depth != 1 ) {
		return false;
	}
	const uint32_t nSourceWidth = std::max( 1U, copy.pSource->nWidth >> copy.region.srcSubresource.mipLevel );
	const uint32_t nSourceHeight = std::max( 1U, copy.pSource->nHeight >> copy.region.srcSubresource.mipLevel );
	const uint32_t nSourceDepth = copy.pSource->eType == VK_IMAGE_TYPE_3D
	                                  ? std::max( 1U, copy.pSource->nDepth >> copy.region.srcSubresource.mipLevel )
	                                  : 1U;
	const uint32_t nDestinationWidth = std::max( 1U, copy.pDestination->nWidth >> copy.region.dstSubresource.mipLevel );
	const uint32_t nDestinationHeight = std::max( 1U, copy.pDestination->nHeight >> copy.region.dstSubresource.mipLevel );
	const uint32_t nDestinationDepth = copy.pDestination->eType == VK_IMAGE_TYPE_3D
	                                       ? std::max( 1U, copy.pDestination->nDepth >> copy.region.dstSubresource.mipLevel )
	                                       : 1U;
	if ( copy.region.srcOffset.x < 0 || copy.region.srcOffset.y < 0 || copy.region.srcOffset.z < 0 ||
	     copy.region.dstOffset.x < 0 || copy.region.dstOffset.y < 0 || copy.region.dstOffset.z < 0 ||
	     static_cast<uint32_t>( copy.region.srcOffset.x ) > nSourceWidth ||
	     static_cast<uint32_t>( copy.region.srcOffset.y ) > nSourceHeight ||
	     static_cast<uint32_t>( copy.region.srcOffset.z ) > nSourceDepth ||
	     static_cast<uint32_t>( copy.region.dstOffset.x ) > nDestinationWidth ||
	     static_cast<uint32_t>( copy.region.dstOffset.y ) > nDestinationHeight ||
	     static_cast<uint32_t>( copy.region.dstOffset.z ) > nDestinationDepth ||
	     copy.region.extent.width > nSourceWidth - static_cast<uint32_t>( copy.region.srcOffset.x ) ||
	     copy.region.extent.height > nSourceHeight - static_cast<uint32_t>( copy.region.srcOffset.y ) ||
	     copy.region.extent.depth > nSourceDepth - static_cast<uint32_t>( copy.region.srcOffset.z ) ||
	     copy.region.extent.width > nDestinationWidth - static_cast<uint32_t>( copy.region.dstOffset.x ) ||
	     copy.region.extent.height > nDestinationHeight - static_cast<uint32_t>( copy.region.dstOffset.y ) ||
	     copy.region.extent.depth > nDestinationDepth - static_cast<uint32_t>( copy.region.dstOffset.z ) ||
	     !sirVulkanGetSubresourceLayout( copy.pSource, copy.region.srcSubresource.mipLevel,
	                                      copy.region.srcSubresource.baseArrayLayer, pSourceLayout ) ||
	     !sirVulkanGetSubresourceLayout( copy.pDestination, copy.region.dstSubresource.mipLevel,
	                                      copy.region.dstSubresource.baseArrayLayer, pDestinationLayout ) ||
	     *pSourceLayout == VK_IMAGE_LAYOUT_UNDEFINED ) {
		return false;
	}
	return true;
}

SIRVulkanResult sirVulkanValidateImageCopy( const sirVulkanImageCopy_t& copy ) noexcept
{
	VkImageLayout eSource = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout eDestination = VK_IMAGE_LAYOUT_UNDEFINED;
	return sirVulkanValidateRecordedImageCopy( copy, &eSource, &eDestination ) ? SIRVulkanResult::SIR_VULKAN_SUCCESS
	                                                                                : SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
}

static bool sirVulkanValidateRecordedIndirect( const sirVulkanRecordedCommand_t& command ) noexcept
{
	const bool bIndexed = command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT;
	const uint64_t nCommandBytes = bIndexed ? sizeof( VkDrawIndexedIndirectCommand ) : sizeof( VkDrawIndirectCommand );
	return command.pBuffer != nullptr && command.pBuffer->hBuffer != VK_NULL_HANDLE &&
	       ( command.pBuffer->nUsage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT ) != 0 && command.nDrawCount != 0 &&
	       command.nIndirectStride >= nCommandBytes && ( command.nIndirectStride & 3U ) == 0 &&
	       ( command.nIndirectOffset & 3U ) == 0 && command.nIndirectOffset <= command.pBuffer->nSize &&
	       command.pBuffer->nSize - command.nIndirectOffset >= nCommandBytes &&
	       static_cast<uint64_t>( command.nDrawCount - 1 ) <=
	           ( command.pBuffer->nSize - command.nIndirectOffset - nCommandBytes ) / command.nIndirectStride;
}

typedef struct sirVulkanBufferSpan_s
{
	const sirVulkanBuffer_t *pBuffer;
	uint64_t nOffset;
	uint64_t nBytes;
} sirVulkanBufferSpan_t;

static sirVulkanBufferSpan_t sirVulkanRecordedSpan( const sirVulkanBuffer_t *pBuffer, uint64_t nOffset, uint64_t nBytes ) noexcept
{
	if ( pBuffer == nullptr ) {
		return {};
	}
	if ( nBytes == 0 || nOffset > pBuffer->nSize || nBytes > pBuffer->nSize - nOffset ) {
		return { pBuffer, 0, pBuffer->nSize };
	}
	return { pBuffer, nOffset, nBytes };
}

static void sirVulkanRecordedBufferAccess( const sirVulkanRecordedCommand_t& command, sirVulkanBufferSpan_t *pRead,
                                           sirVulkanBufferSpan_t *pWrite ) noexcept
{
	*pRead = {};
	*pWrite = {};
	if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_COPY ) {
		const sirVulkanBufferCopy_t& copy = command.bufferCopy;
		*pRead = sirVulkanRecordedSpan( copy.pSource, copy.nSourceOffset, copy.nBytes );
		*pWrite = sirVulkanRecordedSpan( copy.pDestination, copy.nDestinationOffset, copy.nBytes );
	} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY && command.pBuffer != nullptr ) {
		// The transfer footprint includes row/slice padding through the last
		// copied byte. An invalid image/copy conservatively aliases the whole buffer.
		uint64_t nBytes = 0;
		const uint64_t nOffset = command.textureCopy.nBufferOffset;
		if ( !sirVulkanTextureTransferFootprint( command.pImage, command.textureCopy, &nBytes ) ) {
			nBytes = 0;
		}
		const sirVulkanBufferSpan_t span = sirVulkanRecordedSpan( command.pBuffer, nOffset, nBytes );
		if ( command.textureCopy.bBufferToImage ) {
			*pRead = span;
		} else {
			*pWrite = span;
		}
	} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT ||
	            command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT ) {
		const uint64_t nCommandBytes = command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT
		                                   ? sizeof( VkDrawIndexedIndirectCommand )
		                                   : sizeof( VkDrawIndirectCommand );
		uint64_t nBytes = 0;
		if ( command.nDrawCount != 0 && command.nIndirectStride != 0 &&
		     command.nDrawCount - 1 <= ( UINT64_MAX - nCommandBytes ) / command.nIndirectStride ) {
			nBytes = nCommandBytes + static_cast<uint64_t>( command.nDrawCount - 1 ) * command.nIndirectStride;
		}
		*pRead = sirVulkanRecordedSpan( command.pBuffer, command.nIndirectOffset, nBytes );
	}
}

static bool sirVulkanSpansOverlap( const sirVulkanBufferSpan_t& a, const sirVulkanBufferSpan_t& b ) noexcept
{
	if ( a.pBuffer == nullptr || a.pBuffer != b.pBuffer ) {
		return false;
	}
	if ( a.nBytes == 0 || b.nBytes == 0 ) {
		return true;
	}
	// Subtract before comparing: additions could overflow for malformed
	// commands, which must remain conservative until validation rejects them.
	if ( a.nOffset >= b.nOffset ) {
		return a.nOffset - b.nOffset < b.nBytes;
	}
	return b.nOffset - a.nOffset < a.nBytes;
}

bool sirVulkanRecordedNeedsBufferBarrier( const sirVulkanRecordedCommand_t *pCommands, uint32_t nIndex ) noexcept
{
	if ( pCommands == nullptr || nIndex == 0 || nIndex >= SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return false;
	}
	sirVulkanBufferSpan_t read {};
	sirVulkanBufferSpan_t write {};
	sirVulkanRecordedBufferAccess( pCommands[nIndex], &read, &write );
	if ( read.pBuffer == nullptr && write.pBuffer == nullptr ) {
		return false;
	}
	for ( uint32_t iEarlier = 0; iEarlier < nIndex; ++iEarlier ) {
		sirVulkanBufferSpan_t earlierRead {};
		sirVulkanBufferSpan_t earlierWrite {};
		sirVulkanRecordedBufferAccess( pCommands[iEarlier], &earlierRead, &earlierWrite );
		if ( sirVulkanSpansOverlap( write, earlierRead ) || sirVulkanSpansOverlap( write, earlierWrite ) ||
		     sirVulkanSpansOverlap( read, earlierWrite ) ) {
			return true;
		}
	}
	return false;
}

static bool sirVulkanRecordedValidBufferScope( VkPipelineStageFlags2 nStages, VkAccessFlags2 nAccess, VkBufferUsageFlags nUsage ) noexcept
{
	if ( nStages == VK_PIPELINE_STAGE_2_NONE ) {
		return nAccess == VK_ACCESS_2_NONE;
	}
	if ( nAccess == VK_ACCESS_2_NONE ) {
		return false;
	}
	const VkPipelineStageFlags2 nSupportedStages =
	    VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	if ( ( nStages & ~nSupportedStages ) != 0 ) {
		return false;
	}
	const bool bAllCommands = ( nStages & VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT ) != 0;
	VkAccessFlags2 nAllowed = VK_ACCESS_2_NONE;
	if ( bAllCommands || ( nStages & VK_PIPELINE_STAGE_2_COPY_BIT ) != 0 ) {
		nAllowed |= ( ( nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) != 0 ? VK_ACCESS_2_TRANSFER_READ_BIT : 0 ) |
		            ( ( nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) != 0 ? VK_ACCESS_2_TRANSFER_WRITE_BIT : 0 );
	}
	if ( bAllCommands || ( nStages & VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT ) != 0 ) {
		nAllowed |= ( ( nUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ) != 0 ? VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT : 0 ) |
		            ( ( nUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) != 0 ? VK_ACCESS_2_INDEX_READ_BIT : 0 );
	}
	if ( bAllCommands ) {
		nAllowed |= VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT |
		            ( ( nUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ) != 0 ? VK_ACCESS_2_UNIFORM_READ_BIT : 0 ) |
		            ( ( nUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) != 0
		                  ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
						  : 0 );
	}
	return ( nAccess & ~nAllowed ) == 0;
}

SIRVulkanResult sirVulkanValidateRecordedCommands( const sirVulkanRecordedCommand_t *pCommands, uint32_t nCount ) noexcept
{
	if ( pCommands == nullptr || nCount == 0 || nCount > SIR_VULKAN_COPY_BATCH_CAPACITY ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	sirVulkanImage_t *szOriginals[SIR_VULKAN_RECORDED_SHADOW_CAPACITY] = {};
	sirVulkanImage_t szShadows[SIR_VULKAN_RECORDED_SHADOW_CAPACITY];
	memset( szShadows, 0, sizeof( szShadows ) );
	uint32_t nImages = 0;
	bool bRendering = false;
	bool bGraphicsBound = false;
	bool bComputeBound = false;
	bool bViewportSet = false;
	bool bIndexBound = false;
	uint64_t nIndexCapacity = 0;
	const sirVulkanComputePipeline_t *pGraphicsPipeline = nullptr;
	const sirVulkanComputePipeline_t *pComputePipeline = nullptr;
	const sirVulkanRecordedCommand_t *pActivePass = nullptr;
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t& command = pCommands[iCommand];
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_BEGIN ) {
			if ( bRendering || command.nColorCount > 8 || command.extent.width == 0 || command.extent.height == 0 ||
			     ( command.nColorCount == 0 && command.pDepthImage == nullptr ) ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			for ( uint32_t i = 0; i < command.nColorCount; ++i ) {
				if ( command.szColorViews[i] == VK_NULL_HANDLE || command.szColorImages[i] == nullptr ||
				     ( command.szColorImages[i]->nUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) == 0 ||
				     command.szColorImages[i]->eFormat == VK_FORMAT_UNDEFINED ) {
					return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
				}
			}
			if ( command.pDepthImage != nullptr &&
			     ( command.hDepthView == VK_NULL_HANDLE ||
			       ( command.pDepthImage->nUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) == 0 ) ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			for ( uint32_t i = 0; i <= command.nColorCount; ++i ) {
				const bool bDepth = i == command.nColorCount;
				sirVulkanImage_t *pImage = bDepth ? command.pDepthImage : command.szColorImages[i];
				if ( pImage == nullptr )
					continue;
				const uint32_t nMip = bDepth ? command.nDepthMip : command.szColorMips[i];
				const uint32_t nLayer = bDepth ? command.nDepthLayer : command.szColorLayers[i];
				sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( pImage, szOriginals, szShadows, &nImages );
				VkImageLayout eBefore = VK_IMAGE_LAYOUT_UNDEFINED;
				if ( pShadow == nullptr || !sirVulkanGetSubresourceLayout( pShadow, nMip, nLayer, &eBefore ) ||
				     ( eBefore == VK_IMAGE_LAYOUT_UNDEFINED && !( bDepth ? command.bClearDepth : command.szColorClear[i] ) ) ||
				     !sirVulkanSetSubresourceLayout( pShadow, nMip, nLayer,
				                                     bDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
				                                            : VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL ) ) {
					return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
				}
			}
			bRendering = true;
			bGraphicsBound = false;
			bComputeBound = false;
			pGraphicsPipeline = nullptr;
			pComputePipeline = nullptr;
			bViewportSet = false;
			bIndexBound = false;
			pActivePass = &command;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_END ) {
			if ( !bRendering || !sirVulkanRecordedFinalizePass( *pActivePass, szOriginals, szShadows, &nImages ) )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			bRendering = false;
			bGraphicsBound = false;
			pActivePass = nullptr;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_PIPELINE ) {
			if ( bRendering || command.pPipeline == nullptr || command.pPipeline->bGraphics || command.pPipeline->hPipeline == VK_NULL_HANDLE )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			bComputeBound = true;
			pComputePipeline = command.pPipeline;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE ) {
			if ( !bRendering || command.pPipeline == nullptr || !command.pPipeline->bGraphics ||
			     command.pPipeline->hPipeline == VK_NULL_HANDLE || command.pPipeline->nColorFormatCount != pActivePass->nColorCount ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			for ( uint32_t i = 0; i < pActivePass->nColorCount; ++i ) {
				if ( command.pPipeline->szColorFormats[i] != pActivePass->szColorImages[i]->eFormat ) {
					return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
				}
			}
			if ( ( pActivePass->pDepthImage != nullptr ? pActivePass->pDepthImage->eFormat : VK_FORMAT_UNDEFINED ) !=
			     command.pPipeline->eDepthFormat ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			bGraphicsBound = true;
			bComputeBound = false;
			pComputePipeline = nullptr;
			pGraphicsPipeline = command.pPipeline;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET ) {
			const bool bGraphicsSet = bRendering && bGraphicsBound && pGraphicsPipeline != nullptr;
			const bool bComputeSet = !bRendering && bComputeBound && pComputePipeline != nullptr;
			const sirVulkanComputePipeline_t *pPipeline = bGraphicsSet ? pGraphicsPipeline : bComputeSet ? pComputePipeline : nullptr;
			if ( pPipeline == nullptr || command.pDescriptorSet == nullptr || command.pDescriptorSet->hSet == VK_NULL_HANDLE ||
			     command.pDescriptorSet->nSetIndex >= pPipeline->nSetLayoutCount ||
			     command.pDescriptorSet->hSetLayout != pPipeline->szSetLayouts[command.pDescriptorSet->nSetIndex] || command.pPipeline != pPipeline )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VERTEX_BUFFER || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_INDEX_BUFFER ) {
			if ( !bRendering || command.pBuffer == nullptr || command.pBuffer->hBuffer == VK_NULL_HANDLE ||
			     command.nBufferOffset >= command.pBuffer->nSize ||
			     ( command.pBuffer->nUsage & ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VERTEX_BUFFER ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			                                                                                      : VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) ) ==
			         0 ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_INDEX_BUFFER ) {
				if ( command.eIndexType != VK_INDEX_TYPE_UINT16 && command.eIndexType != VK_INDEX_TYPE_UINT32 )
					return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
				const uint64_t nIndexSize = command.eIndexType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
				if ( command.nBufferOffset % nIndexSize != 0 )
					return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
				nIndexCapacity = ( command.pBuffer->nSize - command.nBufferOffset ) / nIndexSize;
				bIndexBound = true;
			}
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VIEWPORT ) {
			if ( !bRendering || command.viewport.width <= 0 || command.viewport.height <= 0 || command.scissor.extent.width == 0 ||
			     command.scissor.extent.height == 0 ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			bViewportSet = true;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED ||
		     command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT ||
		     command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT ) {
			if ( !bRendering || !bGraphicsBound || !bViewportSet || command.nDrawCount == 0 ||
			     ( ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW ||
			         command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED ) && command.nInstanceCount == 0 ) ||
			     ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED &&
			       ( !bIndexBound || command.nFirstElement > nIndexCapacity ||
			         command.nDrawCount > nIndexCapacity - command.nFirstElement ) ) ||
			     ( ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT ||
			         command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT ) &&
			       !sirVulkanValidateRecordedIndirect( command ) ) ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT && !bIndexBound ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( bRendering )
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			if ( sirVulkanValidateBufferCopies( &command.bufferCopy, 1 ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			const sirVulkanBufferBarrier_t& barrier = command.bufferBarrier;
			if ( command.pBuffer == nullptr || command.pBuffer->nSize == 0 || barrier.nDestinationStages == VK_PIPELINE_STAGE_2_NONE ||
			     !sirVulkanRecordedValidBufferScope( barrier.nSourceStages, barrier.nSourceAccess, command.pBuffer->nUsage ) ||
			     !sirVulkanRecordedValidBufferScope( barrier.nDestinationStages, barrier.nDestinationAccess, command.pBuffer->nUsage ) ||
			     ( barrier.bUav && ( ( command.pBuffer->nUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) == 0 ||
			                         ( barrier.nSourceAccess & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ) == 0 ||
			                         ( barrier.nDestinationAccess & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ) == 0 ) ) ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			if ( !bComputeBound || pComputePipeline == nullptr || command.pPipeline != pComputePipeline || command.pPipeline == nullptr ||
			     command.nGroupCountX == 0 || command.nGroupCountY == 0 || command.nGroupCountZ == 0 ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_COPY ) {
			sirVulkanImage_t *pSourceShadow = sirVulkanRecordedShadow( command.imageCopy.pSource, szOriginals, szShadows, &nImages );
			sirVulkanImage_t *pDestinationShadow = sirVulkanRecordedShadow( command.imageCopy.pDestination, szOriginals, szShadows, &nImages );
			if ( pSourceShadow == nullptr || pDestinationShadow == nullptr ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			sirVulkanImageCopy_t shadowCopy = command.imageCopy;
			shadowCopy.pSource = pSourceShadow;
			shadowCopy.pDestination = pDestinationShadow;
			VkImageLayout eSource = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageLayout eDestination = VK_IMAGE_LAYOUT_UNDEFINED;
			if ( !sirVulkanValidateRecordedImageCopy( shadowCopy, &eSource, &eDestination ) ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			if ( !sirVulkanSetSubresourceLayout( pDestinationShadow, command.imageCopy.region.dstSubresource.mipLevel,
			                                     command.imageCopy.region.dstSubresource.baseArrayLayer,
			                                     eDestination == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_GENERAL : eDestination ) ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			const uint32_t nWidth = std::max( 1U, pDestinationShadow->nWidth >> command.imageCopy.region.dstSubresource.mipLevel );
			const uint32_t nHeight = std::max( 1U, pDestinationShadow->nHeight >> command.imageCopy.region.dstSubresource.mipLevel );
			const uint32_t nDepth = pDestinationShadow->eType == VK_IMAGE_TYPE_3D
			                            ? std::max( 1U, pDestinationShadow->nDepth >> command.imageCopy.region.dstSubresource.mipLevel )
			                            : 1U;
			if ( command.imageCopy.region.dstOffset.x == 0 && command.imageCopy.region.dstOffset.y == 0 &&
			     command.imageCopy.region.dstOffset.z == 0 && command.imageCopy.region.extent.width == nWidth &&
			     command.imageCopy.region.extent.height == nHeight && command.imageCopy.region.extent.depth == nDepth ) {
				const uint32_t nIndex = command.imageCopy.region.dstSubresource.baseArrayLayer * pDestinationShadow->nMips +
			                                command.imageCopy.region.dstSubresource.mipLevel;
				pDestinationShadow->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
			}
			continue;
		}
		if ( command.eKind != SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY && command.eKind != SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_BARRIER ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
		sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( command.pImage, szOriginals, szShadows, &nImages );
		const SIRVulkanResult eCheck = sirVulkanAdvanceRecordedState( pShadow, command );
		if ( eCheck != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return eCheck;
		}
	}
	if ( bRendering )
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

SIRVulkanResult SIRVulkanDevice::submitRecordedCommands( const sirVulkanRecordedCommand_t *pCommands, uint32_t nCount,
                                                           sirVulkanCompletion_t *pCompletion, const sirVulkanSubmitSync_t *pSync ) noexcept
{
	if ( pCompletion != nullptr ) {
		*pCompletion = {};
	}
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pCompletion == nullptr || m_nLastTransferValue == UINT64_MAX ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const SIRVulkanResult eValidation = sirVulkanValidateRecordedCommands( pCommands, nCount );
	if ( eValidation != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
		return eValidation;
	}
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t& command = pCommands[iCommand];
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			if ( command.bufferCopy.pSource->hBuffer == VK_NULL_HANDLE || command.bufferCopy.pDestination->hBuffer == VK_NULL_HANDLE ||
			     command.bufferCopy.pSource->hBuffer == command.bufferCopy.pDestination->hBuffer ||
			     command.bufferCopy.pSource->pOwnerAllocator != m_Allocator ||
			     command.bufferCopy.pDestination->pOwnerAllocator != m_Allocator ||
			     command.bufferCopy.pSource->nDeviceSerial != m_nDeviceSerial ||
			     command.bufferCopy.pDestination->nDeviceSerial != m_nDeviceSerial ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			if ( command.pBuffer->hBuffer == VK_NULL_HANDLE || command.pBuffer->pOwnerAllocator != m_Allocator ||
			     command.pBuffer->nDeviceSerial != m_nDeviceSerial ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_BEGIN ) {
			for ( uint32_t iColor = 0; iColor < command.nColorCount; ++iColor ) {
				const sirVulkanImage_t *pImage = command.szColorImages[iColor];
				if ( pImage == nullptr || pImage->hImage == VK_NULL_HANDLE || pImage->pOwnerAllocator != m_Allocator ||
				     pImage->nDeviceSerial != m_nDeviceSerial )
					return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
			if ( command.pDepthImage != nullptr &&
			     ( command.pDepthImage->hImage == VK_NULL_HANDLE || command.pDepthImage->pOwnerAllocator != m_Allocator ||
			       command.pDepthImage->nDeviceSerial != m_nDeviceSerial ) )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE ) {
			if ( command.pPipeline->nDeviceSerial != m_nDeviceSerial )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET ) {
			if ( command.pDescriptorSet->nDeviceSerial != m_nDeviceSerial || command.pPipeline->nDeviceSerial != m_nDeviceSerial )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VERTEX_BUFFER || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_INDEX_BUFFER ) {
			if ( command.pBuffer->pOwnerAllocator != m_Allocator || command.pBuffer->nDeviceSerial != m_nDeviceSerial )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT ||
		            command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT ) {
			if ( command.pBuffer == nullptr || command.pBuffer->pOwnerAllocator != m_Allocator ||
			     command.pBuffer->nDeviceSerial != m_nDeviceSerial )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_COPY ) {
			if ( command.imageCopy.pSource == nullptr || command.imageCopy.pDestination == nullptr ||
			     command.imageCopy.pSource->pOwnerAllocator != m_Allocator ||
			     command.imageCopy.pDestination->pOwnerAllocator != m_Allocator ||
			     command.imageCopy.pSource->nDeviceSerial != m_nDeviceSerial ||
			     command.imageCopy.pDestination->nDeviceSerial != m_nDeviceSerial )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_END || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VIEWPORT ||
		            command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED ) {
			continue;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_PIPELINE ) {
			if ( !m_Adapter.bComputeCapable || command.pPipeline == nullptr || command.pPipeline->bGraphics ||
			     command.pPipeline->hPipeline == VK_NULL_HANDLE || command.pPipeline->nDeviceSerial != m_nDeviceSerial )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			if ( !m_Adapter.bComputeCapable ) {
				return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
			}
			if ( command.pPipeline->hPipeline == VK_NULL_HANDLE || command.pPipeline->nDeviceSerial != m_nDeviceSerial ||
			     command.nGroupCountX > m_Adapter.szMaximumComputeGroupCount[0] ||
			     command.nGroupCountY > m_Adapter.szMaximumComputeGroupCount[1] ||
			     command.nGroupCountZ > m_Adapter.szMaximumComputeGroupCount[2] ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
		} else if ( command.pImage->hImage == VK_NULL_HANDLE || command.pImage->pOwnerAllocator != m_Allocator ||
		            command.pImage->nDeviceSerial != m_nDeviceSerial ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY &&
		            ( command.pBuffer == nullptr || command.pBuffer->hBuffer == VK_NULL_HANDLE ||
		              command.pBuffer->pOwnerAllocator != m_Allocator || command.pBuffer->nDeviceSerial != m_nDeviceSerial ) ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
	}

	const PFN_vkCmdBindDescriptorSets pfnBindDescriptorSets =
	    m_Procs.vkCmdBindDescriptorSets;
	const PFN_vkCmdBeginRendering pfnBeginRendering = m_Procs.vkCmdBeginRendering;
	const PFN_vkCmdEndRendering pfnEndRendering = m_Procs.vkCmdEndRendering;
	const PFN_vkCmdBindVertexBuffers pfnBindVertex =
	    m_Procs.vkCmdBindVertexBuffers;
	const PFN_vkCmdBindIndexBuffer pfnBindIndex = m_Procs.vkCmdBindIndexBuffer;
	const PFN_vkCmdSetViewport pfnViewport = m_Procs.vkCmdSetViewport;
	const PFN_vkCmdSetScissor pfnScissor = m_Procs.vkCmdSetScissor;
	const PFN_vkCmdDraw pfnDraw = m_Procs.vkCmdDraw;
	const PFN_vkCmdDrawIndexed pfnDrawIndexed = m_Procs.vkCmdDrawIndexed;
	const PFN_vkCmdDrawIndirect pfnDrawIndirect = m_Procs.vkCmdDrawIndirect;
	const PFN_vkCmdDrawIndexedIndirect pfnDrawIndexedIndirect = m_Procs.vkCmdDrawIndexedIndirect;
	const PFN_vkCmdDispatch pfnDispatch = m_Procs.vkCmdDispatch;
	const PFN_vkCmdCopyImage2 pfnCopyImage = m_Procs.vkCmdCopyImage2;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const bool bGraphicsCommand = pCommands[i].eKind >= SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_BEGIN &&
		                              pCommands[i].eKind <= SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET;
		if ( bGraphicsCommand &&
		     ( pfnBeginRendering == nullptr || pfnEndRendering == nullptr || pfnBindVertex == nullptr || pfnBindIndex == nullptr ||
		       pfnViewport == nullptr || pfnScissor == nullptr || pfnDraw == nullptr || pfnDrawIndexed == nullptr ||
		       pfnBindDescriptorSets == nullptr ||
		       ( pCommands[i].eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT && pfnDrawIndirect == nullptr ) ||
		       ( pCommands[i].eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT && pfnDrawIndexedIndirect == nullptr ) ) )
			return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
		if ( pCommands[i].eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_COPY && pfnCopyImage == nullptr )
			return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
		if ( ( pCommands[i].eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_PIPELINE ||
		       pCommands[i].eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) &&
		     pfnDispatch == nullptr )
			return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}
	uint64_t nCompleted = 0;
	const VkResult ePoll = m_Procs.vkGetSemaphoreCounterValue( m_Device, m_TransferTimeline, &nCompleted );
	if ( ePoll != VK_SUCCESS ) {
		m_bDeviceLost = ePoll == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	sirVulkanTransferSlot_t *pSlot = nullptr;
	for ( uint32_t iCheck = 0; iCheck < SIR_VULKAN_TRANSFER_SLOT_COUNT; ++iCheck ) {
		const uint32_t iSlot = ( m_iNextTransferSlot + iCheck ) % SIR_VULKAN_TRANSFER_SLOT_COUNT;
		if ( m_TransferSlots[iSlot].nLastValue <= nCompleted ) {
			pSlot = &m_TransferSlots[iSlot];
			m_iNextTransferSlot = ( iSlot + 1 ) % SIR_VULKAN_TRANSFER_SLOT_COUNT;
			break;
		}
	}
	if ( pSlot == nullptr ) {
		return SIRVulkanResult::SIR_VULKAN_BUSY;
	}
	const VkResult eReset = m_Procs.vkResetCommandBuffer( pSlot->hCommandBuffer, 0 );
	if ( eReset != VK_SUCCESS ) {
		m_bDeviceLost = eReset == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	VkCommandBufferBeginInfo begin {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	const VkResult eBegin = m_Procs.vkBeginCommandBuffer( pSlot->hCommandBuffer, &begin );
	if ( eBegin != VK_SUCCESS ) {
		m_bDeviceLost = eBegin == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}

	sirVulkanImage_t *szOriginals[SIR_VULKAN_RECORDED_SHADOW_CAPACITY] = {};
	sirVulkanImage_t szShadows[SIR_VULKAN_RECORDED_SHADOW_CAPACITY];
	memset( szShadows, 0, sizeof( szShadows ) );
	uint32_t nImages = 0;
	VkMemoryBarrier2 memory {};
	memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	memory.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	memory.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	memory.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	memory.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	VkDependencyInfo dependency {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers = &memory;
	// A single boundary barrier covers host writes and earlier queue submissions.
	m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &dependency );

	VkMemoryBarrier2 transferMemory {};
	transferMemory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	transferMemory.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	transferMemory.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
	transferMemory.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	transferMemory.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
	VkDependencyInfo transferDependency {};
	transferDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	transferDependency.memoryBarrierCount = 1;
	transferDependency.pMemoryBarriers = &transferMemory;
	const sirVulkanRecordedCommand_t *pNativePass = nullptr;
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t& command = pCommands[iCommand];
		if ( sirVulkanRecordedNeedsBufferBarrier( pCommands, iCommand ) ) {
			m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &transferDependency );
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_BEGIN ) {
			VkImageMemoryBarrier2 transitions[9];
			memset( transitions, 0, sizeof( transitions ) );
			uint32_t nTransitions = 0;
			for ( uint32_t i = 0; i <= command.nColorCount; ++i ) {
				const bool bDepth = i == command.nColorCount;
				sirVulkanImage_t *pImage = bDepth ? command.pDepthImage : command.szColorImages[i];
				if ( pImage == nullptr )
					continue;
				const uint32_t nMip = bDepth ? command.nDepthMip : command.szColorMips[i];
				const uint32_t nLayer = bDepth ? command.nDepthLayer : command.szColorLayers[i];
				sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( pImage, szOriginals, szShadows, &nImages );
				if ( pShadow == nullptr )
					return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
				VkImageLayout eBefore = VK_IMAGE_LAYOUT_UNDEFINED;
				static_cast<void>(sirVulkanGetSubresourceLayout( pShadow, nMip, nLayer, &eBefore ));
				const VkImageLayout eAfter = bDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				{
					VkImageMemoryBarrier2& transition = transitions[nTransitions++];
					transition.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
					static_cast<void>(sirVulkanImageAccessScope( eBefore, &transition.srcStageMask, &transition.srcAccessMask ));
					static_cast<void>(sirVulkanImageAccessScope( eAfter, &transition.dstStageMask, &transition.dstAccessMask ));
					transition.oldLayout = eBefore;
					transition.newLayout = eAfter;
					transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					transition.image = pImage->hImage;
					transition.subresourceRange.aspectMask = sirVulkanImageAspectMask( pImage->eFormat );
					transition.subresourceRange.baseMipLevel = nMip;
					transition.subresourceRange.levelCount = 1;
					transition.subresourceRange.baseArrayLayer = nLayer;
					transition.subresourceRange.layerCount = 1;
				}
				static_cast<void>(sirVulkanSetSubresourceLayout( pShadow, nMip, nLayer, eAfter ));
			}
			if ( nTransitions != 0 ) {
				VkDependencyInfo attachmentDependency {};
				attachmentDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
				attachmentDependency.imageMemoryBarrierCount = nTransitions;
				attachmentDependency.pImageMemoryBarriers = transitions;
				m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &attachmentDependency );
			}
			VkRenderingAttachmentInfo colors[8];
			memset( colors, 0, sizeof( colors ) );
			for ( uint32_t iColor = 0; iColor < command.nColorCount; ++iColor ) {
				colors[iColor].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				colors[iColor].imageView = command.szColorViews[iColor];
				colors[iColor].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				colors[iColor].loadOp = command.szColorClear[iColor] ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				colors[iColor].storeOp = command.szColorStore[iColor] ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
				colors[iColor].clearValue = command.szClearColors[iColor];
			}
			VkRenderingAttachmentInfo depth {};
			if ( command.pDepthImage != nullptr ) {
				depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				depth.imageView = command.hDepthView;
				depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				depth.loadOp = command.bClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
				depth.storeOp = command.bStoreDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
				depth.clearValue = command.depthClear;
			}
			VkRenderingInfo rendering {};
			rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			rendering.renderArea.extent = command.extent;
			rendering.layerCount = 1;
			rendering.colorAttachmentCount = command.nColorCount;
			rendering.pColorAttachments = colors;
			rendering.pDepthAttachment = command.pDepthImage != nullptr ? &depth : nullptr;
			rendering.pStencilAttachment = command.pDepthImage != nullptr && ( sirVulkanImageAspectMask( command.pDepthImage->eFormat ) &
			                                                                   VK_IMAGE_ASPECT_STENCIL_BIT ) != 0
			                                   ? &depth
			                                   : nullptr;
			pfnBeginRendering( pSlot->hCommandBuffer, &rendering );
			pNativePass = &command;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_END ) {
			pfnEndRendering( pSlot->hCommandBuffer );
			if ( pNativePass == nullptr || !sirVulkanRecordedFinalizePass( *pNativePass, szOriginals, szShadows, &nImages ) )
				return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
			pNativePass = nullptr;
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE ) {
			m_Procs.vkCmdBindPipeline( pSlot->hCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, command.pPipeline->hPipeline );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_PIPELINE ) {
			m_Procs.vkCmdBindPipeline( pSlot->hCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, command.pPipeline->hPipeline );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET ) {
			pfnBindDescriptorSets( pSlot->hCommandBuffer, command.pPipeline->bGraphics ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE,
			                       command.pPipeline->hLayout,
			                       command.pDescriptorSet->nSetIndex, 1, &command.pDescriptorSet->hSet, 0, nullptr );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VERTEX_BUFFER ) {
			pfnBindVertex( pSlot->hCommandBuffer, command.nBufferSlot, 1, &command.pBuffer->hBuffer, &command.nBufferOffset );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_INDEX_BUFFER ) {
			pfnBindIndex( pSlot->hCommandBuffer, command.pBuffer->hBuffer, command.nBufferOffset, command.eIndexType );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VIEWPORT ) {
			pfnViewport( pSlot->hCommandBuffer, 0, 1, &command.viewport );
			pfnScissor( pSlot->hCommandBuffer, 0, 1, &command.scissor );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW ) {
			pfnDraw( pSlot->hCommandBuffer, command.nDrawCount, command.nInstanceCount, command.nFirstElement, command.nFirstInstance );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED ) {
			pfnDrawIndexed( pSlot->hCommandBuffer, command.nDrawCount, command.nInstanceCount, command.nFirstElement, command.nVertexOffset,
			                command.nFirstInstance );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT ) {
			pfnDrawIndirect( pSlot->hCommandBuffer, command.pBuffer->hBuffer, command.nIndirectOffset, command.nDrawCount,
			                command.nIndirectStride );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT ) {
			pfnDrawIndexedIndirect( pSlot->hCommandBuffer, command.pBuffer->hBuffer, command.nIndirectOffset, command.nDrawCount,
			                       command.nIndirectStride );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			VkBufferCopy2 region {};
			region.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
			region.srcOffset = command.bufferCopy.nSourceOffset;
			region.dstOffset = command.bufferCopy.nDestinationOffset;
			region.size = command.bufferCopy.nBytes;
			VkCopyBufferInfo2 info {};
			info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
			info.srcBuffer = command.bufferCopy.pSource->hBuffer;
			info.dstBuffer = command.bufferCopy.pDestination->hBuffer;
			info.regionCount = 1;
			info.pRegions = &region;
			m_Procs.vkCmdCopyBuffer2( pSlot->hCommandBuffer, &info );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			const sirVulkanBufferBarrier_t& barrier = command.bufferBarrier;
			VkBufferMemoryBarrier2 native {};
			native.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			native.srcStageMask = barrier.nSourceStages;
			native.srcAccessMask = barrier.nSourceAccess;
			native.dstStageMask = barrier.nDestinationStages;
			native.dstAccessMask = barrier.nDestinationAccess;
			native.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			native.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			native.buffer = command.pBuffer->hBuffer;
			native.offset = 0;
			native.size = VK_WHOLE_SIZE;
			VkDependencyInfo bufferDependency {};
			bufferDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			bufferDependency.bufferMemoryBarrierCount = 1;
			bufferDependency.pBufferMemoryBarriers = &native;
			m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &bufferDependency );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			m_Procs.vkCmdBindPipeline( pSlot->hCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, command.pPipeline->hPipeline );
			m_Procs.vkCmdDispatch( pSlot->hCommandBuffer, command.nGroupCountX, command.nGroupCountY, command.nGroupCountZ );
			continue;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_COPY ) {
			sirVulkanImage_t *pSourceShadow = sirVulkanRecordedShadow( command.imageCopy.pSource, szOriginals, szShadows, &nImages );
			sirVulkanImage_t *pDestinationShadow = sirVulkanRecordedShadow( command.imageCopy.pDestination, szOriginals, szShadows, &nImages );
			if ( pSourceShadow == nullptr || pDestinationShadow == nullptr ) {
				return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
			}
			VkImageLayout eSourceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageLayout eDestinationLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			if ( !sirVulkanGetSubresourceLayout( pSourceShadow, command.imageCopy.region.srcSubresource.mipLevel,
			                                     command.imageCopy.region.srcSubresource.baseArrayLayer, &eSourceLayout ) ||
			     !sirVulkanGetSubresourceLayout( pDestinationShadow, command.imageCopy.region.dstSubresource.mipLevel,
			                                     command.imageCopy.region.dstSubresource.baseArrayLayer, &eDestinationLayout ) ) {
				return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
			}
			VkImageMemoryBarrier2 transitions[2]{};
			transitions[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			static_cast<void>( sirVulkanImageAccessScope( eSourceLayout, &transitions[0].srcStageMask, &transitions[0].srcAccessMask ) );
			static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &transitions[0].dstStageMask,
			                                             &transitions[0].dstAccessMask ) );
			transitions[0].oldLayout = eSourceLayout;
			transitions[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			transitions[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transitions[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transitions[0].image = command.imageCopy.pSource->hImage;
			transitions[0].subresourceRange.aspectMask = command.imageCopy.region.srcSubresource.aspectMask;
			transitions[0].subresourceRange.baseMipLevel = command.imageCopy.region.srcSubresource.mipLevel;
			transitions[0].subresourceRange.levelCount = 1;
			transitions[0].subresourceRange.baseArrayLayer = command.imageCopy.region.srcSubresource.baseArrayLayer;
			transitions[0].subresourceRange.layerCount = 1;
			transitions[1] = transitions[0];
			static_cast<void>( sirVulkanImageAccessScope( eDestinationLayout, &transitions[1].srcStageMask, &transitions[1].srcAccessMask ) );
			static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &transitions[1].dstStageMask,
			                                             &transitions[1].dstAccessMask ) );
			transitions[1].oldLayout = eDestinationLayout;
			transitions[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transitions[1].image = command.imageCopy.pDestination->hImage;
			transitions[1].subresourceRange.aspectMask = command.imageCopy.region.dstSubresource.aspectMask;
			transitions[1].subresourceRange.baseMipLevel = command.imageCopy.region.dstSubresource.mipLevel;
			transitions[1].subresourceRange.baseArrayLayer = command.imageCopy.region.dstSubresource.baseArrayLayer;
			VkDependencyInfo beforeDependency{};
			beforeDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			beforeDependency.imageMemoryBarrierCount = 2;
			beforeDependency.pImageMemoryBarriers = transitions;
			m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &beforeDependency );
			VkCopyImageInfo2 copyInfo{};
			copyInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
			copyInfo.srcImage = command.imageCopy.pSource->hImage;
			copyInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			copyInfo.dstImage = command.imageCopy.pDestination->hImage;
			copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			copyInfo.regionCount = 1;
			copyInfo.pRegions = &command.imageCopy.region;
			pfnCopyImage( pSlot->hCommandBuffer, &copyInfo );
			VkImageMemoryBarrier2 after[2] = { transitions[0], transitions[1] };
			const VkImageLayout eDestinationFinal = eDestinationLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_GENERAL : eDestinationLayout;
			after[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			after[0].newLayout = eSourceLayout;
			static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &after[0].srcStageMask,
			                                             &after[0].srcAccessMask ) );
			static_cast<void>( sirVulkanImageAccessScope( eSourceLayout, &after[0].dstStageMask, &after[0].dstAccessMask ) );
			after[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			after[1].newLayout = eDestinationFinal;
			static_cast<void>( sirVulkanImageAccessScope( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &after[1].srcStageMask,
			                                             &after[1].srcAccessMask ) );
			static_cast<void>( sirVulkanImageAccessScope( eDestinationFinal, &after[1].dstStageMask, &after[1].dstAccessMask ) );
			VkDependencyInfo afterDependency{};
			afterDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			afterDependency.imageMemoryBarrierCount = 2;
			afterDependency.pImageMemoryBarriers = after;
			m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &afterDependency );
			static_cast<void>( sirVulkanSetSubresourceLayout( pSourceShadow, command.imageCopy.region.srcSubresource.mipLevel,
			                                               command.imageCopy.region.srcSubresource.baseArrayLayer, eSourceLayout ) );
			static_cast<void>( sirVulkanSetSubresourceLayout( pDestinationShadow, command.imageCopy.region.dstSubresource.mipLevel,
			                                               command.imageCopy.region.dstSubresource.baseArrayLayer, eDestinationFinal ) );
			const uint32_t nDestinationWidth = std::max( 1U, pDestinationShadow->nWidth >> command.imageCopy.region.dstSubresource.mipLevel );
			const uint32_t nDestinationHeight = std::max( 1U, pDestinationShadow->nHeight >> command.imageCopy.region.dstSubresource.mipLevel );
			const uint32_t nDestinationDepth = pDestinationShadow->eType == VK_IMAGE_TYPE_3D
			                                       ? std::max( 1U, pDestinationShadow->nDepth >> command.imageCopy.region.dstSubresource.mipLevel )
			                                       : 1U;
			if ( command.imageCopy.region.dstOffset.x == 0 && command.imageCopy.region.dstOffset.y == 0 &&
			     command.imageCopy.region.dstOffset.z == 0 && command.imageCopy.region.extent.width == nDestinationWidth &&
			     command.imageCopy.region.extent.height == nDestinationHeight && command.imageCopy.region.extent.depth == nDestinationDepth ) {
				const uint32_t nIndex = command.imageCopy.region.dstSubresource.baseArrayLayer * pDestinationShadow->nMips +
			                                command.imageCopy.region.dstSubresource.mipLevel;
				pDestinationShadow->szUploadedSubresources[nIndex / 64] |= 1ULL << ( nIndex % 64 );
			}
			continue;
		}
		sirVulkanImage_t *pShadow = sirVulkanRecordedShadow( command.pImage, szOriginals, szShadows, &nImages );
		if ( pShadow == nullptr ) {
			return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
		}
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_BARRIER ) {
			const sirVulkanImageBarrier_t& barrier = command.imageBarrier;
			VkImageMemoryBarrier2 imageBarrier {};
			imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			static_cast<void>(sirVulkanImageAccessScope( barrier.eBefore, &imageBarrier.srcStageMask, &imageBarrier.srcAccessMask ));
			static_cast<void>(sirVulkanImageAccessScope( barrier.eAfter, &imageBarrier.dstStageMask, &imageBarrier.dstAccessMask ));
			imageBarrier.oldLayout = barrier.eBefore;
			imageBarrier.newLayout = barrier.eAfter;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = command.pImage->hImage;
			imageBarrier.subresourceRange.aspectMask = sirVulkanImageAspectMask( command.pImage->eFormat );
			imageBarrier.subresourceRange.baseMipLevel = barrier.nMip;
			imageBarrier.subresourceRange.levelCount = barrier.nMipCount;
			imageBarrier.subresourceRange.baseArrayLayer = barrier.nLayer;
			imageBarrier.subresourceRange.layerCount = barrier.nLayerCount;
			VkDependencyInfo imageDependency {};
			imageDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			imageDependency.imageMemoryBarrierCount = 1;
			imageDependency.pImageMemoryBarriers = &imageBarrier;
			m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &imageDependency );
			static_cast<void>(sirVulkanAdvanceRecordedState( pShadow, command ));
			continue;
		}
		const sirVulkanTextureTransfer_t& copy = command.textureCopy;
		const bool bUpload = copy.bBufferToImage;
		VkImageLayout eOld = VK_IMAGE_LAYOUT_UNDEFINED;
		static_cast<void>(sirVulkanGetSubresourceLayout( pShadow, copy.nMip, copy.nLayer, &eOld ));
		const VkImageLayout eTransfer = bUpload ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		VkImageMemoryBarrier2 before {};
		before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		static_cast<void>(sirVulkanImageAccessScope( eOld, &before.srcStageMask, &before.srcAccessMask ));
		static_cast<void>(sirVulkanImageAccessScope( eTransfer, &before.dstStageMask, &before.dstAccessMask ));
		before.oldLayout = eOld;
		before.newLayout = eTransfer;
		before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		before.image = command.pImage->hImage;
		before.subresourceRange.aspectMask = sirVulkanImageAspectMask( command.pImage->eFormat );
		before.subresourceRange.baseMipLevel = copy.nMip;
		before.subresourceRange.levelCount = 1;
		before.subresourceRange.baseArrayLayer = copy.nLayer;
		before.subresourceRange.layerCount = 1;
		VkDependencyInfo imageDependency {};
		imageDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		imageDependency.imageMemoryBarrierCount = 1;
		imageDependency.pImageMemoryBarriers = &before;
		m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &imageDependency );

		VkBufferImageCopy2 region {};
		region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
		region.bufferOffset = copy.nBufferOffset;
		region.bufferRowLength = copy.nBufferRowLength;
		region.bufferImageHeight = copy.nBufferImageHeight;
		region.imageSubresource.aspectMask = sirVulkanImageAspectMask( command.pImage->eFormat );
		region.imageSubresource.mipLevel = copy.nMip;
		region.imageSubresource.baseArrayLayer = copy.nLayer;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = copy.offset;
		region.imageExtent = copy.extent;
		if ( bUpload ) {
			VkCopyBufferToImageInfo2 info {};
			info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
			info.srcBuffer = command.pBuffer->hBuffer;
			info.dstImage = command.pImage->hImage;
			info.dstImageLayout = eTransfer;
			info.regionCount = 1;
			info.pRegions = &region;
			m_Procs.vkCmdCopyBufferToImage2( pSlot->hCommandBuffer, &info );
		} else {
			VkCopyImageToBufferInfo2 info {};
			info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2;
			info.srcImage = command.pImage->hImage;
			info.srcImageLayout = eTransfer;
			info.dstBuffer = command.pBuffer->hBuffer;
			info.regionCount = 1;
			info.pRegions = &region;
			m_Procs.vkCmdCopyImageToBuffer2( pSlot->hCommandBuffer, &info );
		}
		VkImageMemoryBarrier2 after = before;
		after.oldLayout = eTransfer;
		after.newLayout = bUpload ? sirVulkanUploadFinalLayout( pShadow, copy ) : eOld;
		static_cast<void>(sirVulkanImageAccessScope( eTransfer, &after.srcStageMask, &after.srcAccessMask ));
		static_cast<void>(sirVulkanImageAccessScope( after.newLayout, &after.dstStageMask, &after.dstAccessMask ));
		imageDependency.pImageMemoryBarriers = &after;
		m_Procs.vkCmdPipelineBarrier2( pSlot->hCommandBuffer, &imageDependency );
		static_cast<void>(sirVulkanAdvanceRecordedState( pShadow, command ));
	}
	const VkResult eEnd = m_Procs.vkEndCommandBuffer( pSlot->hCommandBuffer );
	if ( eEnd != VK_SUCCESS ) {
		m_bDeviceLost = eEnd == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	const uint64_t nNext = m_nLastTransferValue + 1;
	VkCommandBufferSubmitInfo cmd {};
	cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd.commandBuffer = pSlot->hCommandBuffer;
	VkSemaphoreSubmitInfo signals[9]{};
	uint32_t nSignalCount = pSync != nullptr ? pSync->nSignalCount : 0;
	if ( nSignalCount > 8 || ( nSignalCount != 0 && pSync->pSignals == nullptr ) ||
	     ( pSync != nullptr && pSync->nWaitCount != 0 && pSync->pWaits == nullptr ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nSignalCount; ++i ) {
		signals[i] = pSync->pSignals[i];
	}
	VkSemaphoreSubmitInfo& completionSignal = signals[nSignalCount++];
	completionSignal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	completionSignal.semaphore = m_TransferTimeline;
	completionSignal.value = nNext;
	completionSignal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submit {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.commandBufferInfoCount = 1;
	submit.pCommandBufferInfos = &cmd;
	submit.waitSemaphoreInfoCount = pSync != nullptr ? pSync->nWaitCount : 0;
	submit.pWaitSemaphoreInfos = pSync != nullptr ? pSync->pWaits : nullptr;
	submit.signalSemaphoreInfoCount = nSignalCount;
	submit.pSignalSemaphoreInfos = signals;
	const VkResult eSubmit = m_Procs.vkQueueSubmit2( m_GraphicsQueue, 1, &submit, VK_NULL_HANDLE );
	if ( eSubmit != VK_SUCCESS ) {
		m_bDeviceLost = eSubmit == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	m_nLastTransferValue = nNext;
	pSlot->nLastValue = nNext;
	for ( uint32_t iCommand = 0; iCommand < nCount; ++iCommand ) {
		const sirVulkanRecordedCommand_t& command = pCommands[iCommand];
		if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_COPY ) {
			command.bufferCopy.pSource->nLastGpuUse = nNext;
			command.bufferCopy.pDestination->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_BARRIER ) {
			command.pBuffer->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_PIPELINE ||
		            command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_DISPATCH ) {
			command.pPipeline->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_BEGIN ) {
			for ( uint32_t i = 0; i < command.nColorCount; ++i ) {
				command.szColorImages[i]->nLastGpuUse = nNext;
			}
			if ( command.pDepthImage != nullptr )
				command.pDepthImage->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE ) {
			command.pPipeline->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET ) {
			command.pDescriptorSet->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VERTEX_BUFFER || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_INDEX_BUFFER ) {
			command.pBuffer->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT ||
		            command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT ) {
			command.pBuffer->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_END || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VIEWPORT ||
		            command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW || command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED ) {
			continue;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_COPY ) {
			command.imageCopy.pSource->nLastGpuUse = nNext;
			command.imageCopy.pDestination->nLastGpuUse = nNext;
		} else if ( command.eKind == SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY ) {
			command.pBuffer->nLastGpuUse = nNext;
			command.pImage->nLastGpuUse = nNext;
		} else {
			command.pImage->nLastGpuUse = nNext;
		}
	}
	for ( uint32_t iImage = 0; iImage < nImages; ++iImage ) {
		memcpy( szOriginals[iImage]->szSubresourceLayouts, szShadows[iImage].szSubresourceLayouts,
		        sizeof( szShadows[iImage].szSubresourceLayouts ) );
		memcpy( szOriginals[iImage]->szUploadedSubresources, szShadows[iImage].szUploadedSubresources,
		        sizeof( szShadows[iImage].szUploadedSubresources ) );
	}
	pCompletion->nDeviceSerial = m_nDeviceSerial;
	pCompletion->nValue = nNext;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
