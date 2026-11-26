#include "SIRVulkanCommandList.h"
#include "SIRVulkanBindingSet.h"
#include "SIRVulkanPipeline.h"
#include "SIRVulkanStorageBuffer.h"
#include "SIRVulkanTexture.h"

#include <algorithm>
#include <limits>

static bool sirVulkanCommandCanRecord( const SIRVulkanCommandList& list ) noexcept
{
	return list.getDevice().get() != nullptr;
}

static bool sirVulkanBufferIsMappedForCommand( const sirVulkanBuffer_t& buffer ) noexcept
{
	return buffer.bMapped && !buffer.bPersistentlyMapped;
}

static bool sirVulkanCommandAppend( SIRVector<sirVulkanRecordedCommand_t>& commands, const sirVulkanRecordedCommand_t& command ) noexcept
{
	return commands.getSize() < SIR_VULKAN_COPY_BATCH_CAPACITY && commands.tryEmplaceBack( command ) == SIRContainerResult::SIR_CONTAINER_OK;
}

SIRVulkanCommandList::SIRVulkanCommandList( const SIRSharedPtr<SIRRenderDevice>& device, SIRRenderQueueType queueType ) noexcept
    : SIRRenderCommandList( device, queueType ), m_Commands( nullptr )
{
}

SIRRenderResult SIRVulkanCommandList::begin( void ) noexcept
{
	if ( m_bRecording || !sirVulkanCommandCanRecord( *this ) )
		return SIRRenderResult::SIR_RENDER_BUSY;
	m_bRecording = true;
	m_bInRenderPass = false;
	m_bViewportSet = false;
	m_bIndexBufferBound = false;
	m_pBoundPipeline = nullptr;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::end( void ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass )
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	m_bRecording = false;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::reset( void ) noexcept
{
	m_Commands.clear();
	m_bRecording = false;
	m_bInRenderPass = false;
	m_bViewportSet = false;
	m_bIndexBufferBound = false;
	m_pBoundPipeline = nullptr;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::copyBuffer( const SIRRenderStorageBuffer& destination, const SIRRenderStorageBuffer& source, const sirRenderBufferRange_t& destinationRange, const sirRenderBufferRange_t& sourceRange ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || destinationRange.nSize == 0 || destinationRange.nSize != sourceRange.nSize || sourceRange.nOffset > source.getSize() || destinationRange.nOffset > destination.getSize() || sourceRange.nSize > source.getSize() - sourceRange.nOffset || destinationRange.nSize > destination.getSize() - destinationRange.nOffset ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pDestination = dynamic_cast<const SIRVulkanStorageBuffer *>( &destination );
	const auto *pSource = dynamic_cast<const SIRVulkanStorageBuffer *>( &source );
	if ( pDestination == nullptr || pSource == nullptr || pDestination == pSource || sirVulkanBufferIsMappedForCommand( pDestination->getNativeBuffer() ) ||
	     sirVulkanBufferIsMappedForCommand( pSource->getNativeBuffer() ) ||
	     ( pSource->getNativeBuffer().nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 ||
	     ( pDestination->getNativeBuffer().nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 )
		return pSource != nullptr && pDestination != nullptr &&
		               ( sirVulkanBufferIsMappedForCommand( pSource->getNativeBuffer() ) || sirVulkanBufferIsMappedForCommand( pDestination->getNativeBuffer() ) )
		           ? SIRRenderResult::SIR_RENDER_BUSY
		           : SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_COPY;
	command.bufferCopy = { const_cast<sirVulkanBuffer_t *>( &pSource->getNativeBuffer() ), const_cast<sirVulkanBuffer_t *>( &pDestination->getNativeBuffer() ), sourceRange.nOffset, destinationRange.nOffset, sourceRange.nSize };
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::copyBufferToTexture( const SIRRenderStorageBuffer& source, const SIRRenderTexture& destination,
                                                           const sirRenderBufferRange_t& sourceRange, const sirRenderSubresourceRange_t& destinationRange ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || destinationRange.nMipCount != 1 || destinationRange.nLayerCount != 1 || sourceRange.nSize == 0 ||
	     sourceRange.nOffset > source.getSize() || sourceRange.nSize > source.getSize() - sourceRange.nOffset ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pSource = dynamic_cast<const SIRVulkanStorageBuffer *>( &source );
	const auto *pDestination = dynamic_cast<const SIRVulkanTexture *>( &destination );
	if ( pSource == nullptr || pDestination == nullptr || sirVulkanBufferIsMappedForCommand( pSource->getNativeBuffer() ) ||
	     destinationRange.nBaseMip >= pDestination->getMipLevels() || destinationRange.nBaseLayer >= pDestination->getArrayLayers() )
		return pSource != nullptr && sirVulkanBufferIsMappedForCommand( pSource->getNativeBuffer() ) ? SIRRenderResult::SIR_RENDER_BUSY
		                                                                               : SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const uint32_t nWidth = std::max( 1U, pDestination->getWidth() >> destinationRange.nBaseMip );
	const uint32_t nHeight = std::max( 1U, pDestination->getHeight() >> destinationRange.nBaseMip );
	const uint32_t nDepth = pDestination->getDimension() == SIRRenderTextureDimension::SIR_RENDER_THREE
	                            ? std::max( 1U, pDestination->getDepth() >> destinationRange.nBaseMip )
	                            : 1U;
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY;
	command.pBuffer = const_cast<sirVulkanBuffer_t *>( &pSource->getNativeBuffer() );
	command.pImage = const_cast<sirVulkanImage_t *>( &pDestination->getNativeImage() );
	command.textureCopy = { sourceRange.nOffset, 0, 0, destinationRange.nBaseMip, destinationRange.nBaseLayer, { 0, 0, 0 }, { nWidth, nHeight, nDepth }, true };
	uint64_t nFootprint = 0;
	if ( !sirVulkanTextureTransferFootprint( &pDestination->getNativeImage(), command.textureCopy, &nFootprint ) || nFootprint > sourceRange.nSize )
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::copyTextureToBuffer( const SIRRenderTexture& source, const SIRRenderStorageBuffer& destination,
                                                           const sirRenderSubresourceRange_t& sourceRange, const sirRenderBufferRange_t& destinationRange ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || sourceRange.nMipCount != 1 || sourceRange.nLayerCount != 1 || destinationRange.nSize == 0 ||
	     destinationRange.nOffset > destination.getSize() || destinationRange.nSize > destination.getSize() - destinationRange.nOffset ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pSource = dynamic_cast<const SIRVulkanTexture *>( &source );
	const auto *pDestination = dynamic_cast<const SIRVulkanStorageBuffer *>( &destination );
	if ( pSource == nullptr || pDestination == nullptr || sirVulkanBufferIsMappedForCommand( pDestination->getNativeBuffer() ) ||
	     sourceRange.nBaseMip >= pSource->getMipLevels() || sourceRange.nBaseLayer >= pSource->getArrayLayers() )
		return pDestination != nullptr && sirVulkanBufferIsMappedForCommand( pDestination->getNativeBuffer() ) ? SIRRenderResult::SIR_RENDER_BUSY
		                                                                                         : SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const uint32_t nWidth = std::max( 1U, pSource->getWidth() >> sourceRange.nBaseMip );
	const uint32_t nHeight = std::max( 1U, pSource->getHeight() >> sourceRange.nBaseMip );
	const uint32_t nDepth = pSource->getDimension() == SIRRenderTextureDimension::SIR_RENDER_THREE
	                           ? std::max( 1U, pSource->getDepth() >> sourceRange.nBaseMip )
	                           : 1U;
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_TEXTURE_COPY;
	command.pBuffer = const_cast<sirVulkanBuffer_t *>( &pDestination->getNativeBuffer() );
	command.pImage = const_cast<sirVulkanImage_t *>( &pSource->getNativeImage() );
	command.textureCopy = { destinationRange.nOffset, 0, 0, sourceRange.nBaseMip, sourceRange.nBaseLayer, { 0, 0, 0 }, { nWidth, nHeight, nDepth }, false };
	uint64_t nFootprint = 0;
	if ( !sirVulkanTextureTransferFootprint( &pSource->getNativeImage(), command.textureCopy, &nFootprint ) || nFootprint > destinationRange.nSize )
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::copyTexture( const SIRRenderTexture& destination, const SIRRenderTexture& source,
                                                   const sirRenderSubresourceRange_t& destinationRange, const sirRenderSubresourceRange_t& sourceRange ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || destinationRange.nMipCount != 1 || destinationRange.nLayerCount != 1 || sourceRange.nMipCount != 1 ||
	     sourceRange.nLayerCount != 1 ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	const auto *pDestination = dynamic_cast<const SIRVulkanTexture *>( &destination );
	const auto *pSource = dynamic_cast<const SIRVulkanTexture *>( &source );
	if ( pDestination == nullptr || pSource == nullptr || pDestination == pSource ||
	     destinationRange.nBaseMip >= pDestination->getMipLevels() || destinationRange.nBaseLayer >= pDestination->getArrayLayers() ||
	     sourceRange.nBaseMip >= pSource->getMipLevels() || sourceRange.nBaseLayer >= pSource->getArrayLayers() ||
	     pDestination->getFormat() != pSource->getFormat() ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	const uint32_t nWidth = std::max( 1U, pDestination->getWidth() >> destinationRange.nBaseMip );
	const uint32_t nHeight = std::max( 1U, pDestination->getHeight() >> destinationRange.nBaseMip );
	const uint32_t nDepth = pDestination->getDimension() == SIRRenderTextureDimension::SIR_RENDER_THREE
	                            ? std::max( 1U, pDestination->getDepth() >> destinationRange.nBaseMip )
	                            : 1U;
	if ( nWidth != std::max( 1U, pSource->getWidth() >> sourceRange.nBaseMip ) ||
	     nHeight != std::max( 1U, pSource->getHeight() >> sourceRange.nBaseMip ) ||
	     ( pSource->getDimension() == SIRRenderTextureDimension::SIR_RENDER_THREE
	           ? std::max( 1U, pSource->getDepth() >> sourceRange.nBaseMip )
	           : 1U ) != nDepth ) {
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_COPY;
	command.imageCopy.pSource = const_cast<sirVulkanImage_t *>( &pSource->getNativeImage() );
	command.imageCopy.pDestination = const_cast<sirVulkanImage_t *>( &pDestination->getNativeImage() );
	command.pImage = command.imageCopy.pDestination;
	command.imageCopy.region.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
	command.imageCopy.region.srcSubresource.aspectMask = sirVulkanImageAspectMask( pSource->getNativeImage().eFormat );
	command.imageCopy.region.srcSubresource.mipLevel = sourceRange.nBaseMip;
	command.imageCopy.region.srcSubresource.baseArrayLayer = sourceRange.nBaseLayer;
	command.imageCopy.region.srcSubresource.layerCount = 1;
	command.imageCopy.region.dstSubresource.aspectMask = sirVulkanImageAspectMask( pDestination->getNativeImage().eFormat );
	command.imageCopy.region.dstSubresource.mipLevel = destinationRange.nBaseMip;
	command.imageCopy.region.dstSubresource.baseArrayLayer = destinationRange.nBaseLayer;
	command.imageCopy.region.dstSubresource.layerCount = 1;
	command.imageCopy.region.extent = { nWidth, nHeight, nDepth };
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::beginRenderPass( void *pPass ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || pPass == nullptr || m_Commands.getSize() >= SIR_VULKAN_COPY_BATCH_CAPACITY ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto& pass = *static_cast<const RenderPassDescription *>( pPass );
	if ( pass.nColorCount > 8 || pass.extent.width == 0 || pass.extent.height == 0 || ( pass.nColorCount == 0 && pass.pDepthImage == nullptr ) ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	for ( uint32_t iColor = 0; iColor < pass.nColorCount; ++iColor ) {
		if ( pass.szColorViews[iColor] == VK_NULL_HANDLE || pass.szColorImages[iColor] == nullptr ||
		     ( pass.szColorImages[iColor]->nUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) == 0 ||
		     sirVulkanImageAspectMask( pass.szColorImages[iColor]->eFormat ) != VK_IMAGE_ASPECT_COLOR_BIT ||
		     pass.szColorImages[iColor]->nWidth < pass.extent.width || pass.szColorImages[iColor]->nHeight < pass.extent.height )
			return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	if ( pass.pDepthImage != nullptr &&
	     ( pass.hDepthView == VK_NULL_HANDLE || ( pass.pDepthImage->nUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) == 0 ||
	       ( sirVulkanImageAspectMask( pass.pDepthImage->eFormat ) & VK_IMAGE_ASPECT_DEPTH_BIT ) == 0 ||
	       pass.pDepthImage->nWidth < pass.extent.width || pass.pDepthImage->nHeight < pass.extent.height ) )
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_BEGIN;
	command.nColorCount = pass.nColorCount; command.extent = pass.extent;
	for ( uint32_t i = 0; i < pass.nColorCount; ++i ) { command.szColorViews[i] = pass.szColorViews[i]; command.szColorImages[i] = pass.szColorImages[i]; command.szColorMips[i] = pass.szColorMips[i]; command.szColorLayers[i] = pass.szColorLayers[i]; command.szClearColors[i] = pass.szClearColors[i]; command.szColorClear[i] = pass.szColorClear[i]; command.szColorStore[i] = pass.szColorStore[i]; }
	command.hDepthView = pass.hDepthView; command.pDepthImage = pass.pDepthImage; command.nDepthMip = pass.nDepthMip; command.nDepthLayer = pass.nDepthLayer; command.depthClear = pass.depthClear; command.bClearDepth = pass.bClearDepth; command.bStoreDepth = pass.bStoreDepth;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	m_bInRenderPass = true;
	m_bViewportSet = false;
	m_bIndexBufferBound = false;
	m_pBoundPipeline = nullptr;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::endRenderPass( void ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || m_Commands.getSize() >= SIR_VULKAN_COPY_BATCH_CAPACITY ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{}; command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_RENDER_END;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	m_bInRenderPass = false;
	m_bViewportSet = false;
	m_bIndexBufferBound = false;
	m_pBoundPipeline = nullptr;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::setViewport( const sirRenderViewport_t& viewport, const sirRenderRect_t& scissor ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || viewport.fWidth <= 0.0F || viewport.fHeight <= 0.0F || viewport.fMinDepth < 0.0F || viewport.fMaxDepth > 1.0F || viewport.fMinDepth > viewport.fMaxDepth || scissor.nWidth == 0 || scissor.nHeight == 0 || scissor.nX < 0 || scissor.nY < 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{}; command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VIEWPORT; command.viewport = { viewport.fX, viewport.fY, viewport.fWidth, viewport.fHeight, viewport.fMinDepth, viewport.fMaxDepth }; command.scissor = { { scissor.nX, scissor.nY }, { scissor.nWidth, scissor.nHeight } };
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	m_bViewportSet = true;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::bindVertexBuffer( uint32_t nSlot, const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nStrideBytes ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || nSlot >= 32 || nStrideBytes == 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pBuffer = dynamic_cast<const SIRVulkanStorageBuffer *>( &buffer );
	if ( pBuffer == nullptr || sirVulkanBufferIsMappedForCommand( pBuffer->getNativeBuffer() ) || nOffsetBytes >= buffer.getSize() ||
	     ( pBuffer->getNativeBuffer().nUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ) == 0 )
		return pBuffer != nullptr && sirVulkanBufferIsMappedForCommand( pBuffer->getNativeBuffer() ) ? SIRRenderResult::SIR_RENDER_BUSY
		                                                                               : SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{}; command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_VERTEX_BUFFER; command.pBuffer = const_cast<sirVulkanBuffer_t *>( &pBuffer->getNativeBuffer() ); command.nBufferOffset = nOffsetBytes; command.nBufferSlot = nSlot;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::bindIndexBuffer( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nIndexStrideBytes ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || ( nIndexStrideBytes != 2 && nIndexStrideBytes != 4 ) ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pBuffer = dynamic_cast<const SIRVulkanStorageBuffer *>( &buffer );
	if ( pBuffer == nullptr || sirVulkanBufferIsMappedForCommand( pBuffer->getNativeBuffer() ) || nOffsetBytes >= buffer.getSize() ||
	     nOffsetBytes % nIndexStrideBytes != 0 || ( pBuffer->getNativeBuffer().nUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) == 0 )
		return pBuffer != nullptr && sirVulkanBufferIsMappedForCommand( pBuffer->getNativeBuffer() ) ? SIRRenderResult::SIR_RENDER_BUSY
		                                                                               : SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{}; command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_INDEX_BUFFER; command.pBuffer = const_cast<sirVulkanBuffer_t *>( &pBuffer->getNativeBuffer() ); command.nBufferOffset = nOffsetBytes; command.eIndexType = nIndexStrideBytes == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	m_bIndexBufferBound = true;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::draw( uint32_t nVertexCount, uint32_t nInstanceCount, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || m_pBoundPipeline == nullptr || !m_bViewportSet || nVertexCount == 0 || nInstanceCount == 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{}; command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW; command.nDrawCount = nVertexCount; command.nInstanceCount = nInstanceCount; command.nFirstElement = nFirstVertex; command.nFirstInstance = nFirstInstance;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::drawIndexed( uint32_t nIndexCount, uint32_t nInstanceCount, uint32_t nFirstIndex, int32_t nVertexOffset, uint32_t nFirstInstance ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || m_pBoundPipeline == nullptr || !m_bViewportSet || !m_bIndexBufferBound || nIndexCount == 0 || nInstanceCount == 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{}; command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED; command.nDrawCount = nIndexCount; command.nInstanceCount = nInstanceCount; command.nFirstElement = nFirstIndex; command.nVertexOffset = nVertexOffset; command.nFirstInstance = nFirstInstance;
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

static SIRRenderResult sirVulkanRecordIndirectDraw( SIRVector<sirVulkanRecordedCommand_t>& commands, bool bIndexed,
                                                     const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes,
                                                     uint32_t nDrawCount, uint32_t nStrideBytes, bool bRecording ) noexcept
{
	const auto *pBuffer = dynamic_cast<const SIRVulkanStorageBuffer *>( &buffer );
	const uint64_t nCommandBytes = bIndexed ? sizeof( VkDrawIndexedIndirectCommand ) : sizeof( VkDrawIndirectCommand );
	if ( !bRecording || pBuffer == nullptr || ( pBuffer != nullptr && sirVulkanBufferIsMappedForCommand( pBuffer->getNativeBuffer() ) ) || nDrawCount == 0 || nStrideBytes < nCommandBytes || ( nStrideBytes & 3U ) != 0 ||
	     ( nOffsetBytes & 3U ) != 0 || nOffsetBytes > buffer.getSize() ||
	     buffer.getSize() - nOffsetBytes < nCommandBytes ||
	     static_cast<uint64_t>( nDrawCount - 1 ) > ( buffer.getSize() - nOffsetBytes - nCommandBytes ) / nStrideBytes ||
	     ( pBuffer->getNativeBuffer().nUsage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT ) == 0 ) {
		return pBuffer != nullptr && sirVulkanBufferIsMappedForCommand( pBuffer->getNativeBuffer() ) ? SIRRenderResult::SIR_RENDER_BUSY
		                                                                                : SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	}
	sirVulkanRecordedCommand_t command{};
	command.eKind = bIndexed ? SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDEXED_INDIRECT
	                         : SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_DRAW_INDIRECT;
	command.pBuffer = const_cast<sirVulkanBuffer_t *>( &pBuffer->getNativeBuffer() );
	command.nIndirectOffset = nOffsetBytes;
	command.nDrawCount = nDrawCount;
	command.nIndirectStride = nStrideBytes;
	return sirVulkanCommandAppend( commands, command ) ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
}

SIRRenderResult SIRVulkanCommandList::transition( SIRSpan<const uint8_t> barriers ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || barriers.getData() == nullptr || barriers.getSize() == 0 || barriers.getSize() % sizeof( TransitionDescription ) != 0 ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pTransitions = reinterpret_cast<const TransitionDescription *>( barriers.getData() );
	const uint32_t nTransitions = static_cast<uint32_t>( barriers.getSize() / sizeof( TransitionDescription ) );
	if ( nTransitions > SIR_VULKAN_COPY_BATCH_CAPACITY - m_Commands.getSize() ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	for ( uint32_t i = 0; i < nTransitions; ++i ) { sirVulkanRecordedCommand_t command{}; command.eKind = pTransitions[i].bImage ? SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_IMAGE_BARRIER : SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BUFFER_BARRIER; command.pImage = pTransitions[i].pImage; command.pBuffer = pTransitions[i].pBuffer; if ( pTransitions[i].bImage ) command.imageBarrier = pTransitions[i].image; else command.bufferBarrier = pTransitions[i].buffer; if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY; }
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::submitNative( sirVulkanCompletion_t *pCompletion ) noexcept
{
	if ( m_bRecording || m_Commands.getSize() == 0 || getDevice().get() == nullptr ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	SIRVulkanDevice& device = static_cast<SIRVulkanDevice&>( *getDevice() );
	const SIRVulkanResult result = device.submitRecordedCommands( m_Commands.data(), m_Commands.getSize(), pCompletion );
	return result == SIRVulkanResult::SIR_VULKAN_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_BACKENDERROR;
}

#define SIR_VK_UNSUPPORTED(name, signature) SIRRenderResult SIRVulkanCommandList::name signature noexcept { return SIRRenderResult::SIR_RENDER_UNSUPPORTED; }
SIRRenderResult SIRVulkanCommandList::bindPipeline( const SIRRenderPipeline& pipeline ) noexcept
{
	if ( !m_bRecording || m_Commands.getSize() >= SIR_VULKAN_COPY_BATCH_CAPACITY ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pPipeline = dynamic_cast<const SIRVulkanPipeline *>( &pipeline );
	if ( pPipeline == nullptr || !pPipeline->isValid() ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{};
	if ( pipeline.getType() == SIRRenderPipelineType::SIR_RENDER_GRAPHICS ) {
		if ( !m_bInRenderPass ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
		command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_GRAPHICS_PIPELINE;
	} else if ( pipeline.getType() == SIRRenderPipelineType::SIR_RENDER_COMPUTE ) {
		if ( m_bInRenderPass ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
		command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_PIPELINE;
	} else {
		return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	command.pPipeline = const_cast<sirVulkanComputePipeline_t *>( &pPipeline->getNativePipelineState() );
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	m_pBoundPipeline = command.pPipeline;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanCommandList::bindBindingSet( uint32_t nSetIndex, const SIRRenderBindingSet& bindingSet ) noexcept
{
	if ( !m_bRecording || m_pBoundPipeline == nullptr || m_Commands.getSize() >= SIR_VULKAN_COPY_BATCH_CAPACITY ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	const auto *pSet = dynamic_cast<const SIRVulkanBindingSet *>( &bindingSet );
	if ( pSet == nullptr || !pSet->isValid() || nSetIndex != pSet->getNativeDescriptorSet().nSetIndex ||
	     nSetIndex >= m_pBoundPipeline->nSetLayoutCount || pSet->getNativeLayout() != m_pBoundPipeline->szSetLayouts[nSetIndex] )
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_BIND_DESCRIPTOR_SET;
	command.pPipeline = const_cast<sirVulkanComputePipeline_t *>( m_pBoundPipeline );
	command.pDescriptorSet = const_cast<sirVulkanDescriptorSet_t *>( &pSet->getNativeDescriptorSet() );
	if ( !sirVulkanCommandAppend( m_Commands, command ) ) return SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}
SIR_VK_UNSUPPORTED( setConstants, ( uint32_t, SIRSpan<const uint8_t> ) )
SIRRenderResult SIRVulkanCommandList::drawIndirect( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nDrawCount,
                                                    uint32_t nStrideBytes ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || m_pBoundPipeline == nullptr || !m_bViewportSet ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	return sirVulkanRecordIndirectDraw( m_Commands, false, buffer, nOffsetBytes, nDrawCount, nStrideBytes, true );
}
SIRRenderResult SIRVulkanCommandList::drawIndexedIndirect( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes,
                                                           uint32_t nDrawCount, uint32_t nStrideBytes ) noexcept
{
	if ( !m_bRecording || !m_bInRenderPass || m_pBoundPipeline == nullptr || !m_bViewportSet || !m_bIndexBufferBound ) return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	return sirVulkanRecordIndirectDraw( m_Commands, true, buffer, nOffsetBytes, nDrawCount, nStrideBytes, true );
}
SIR_VK_UNSUPPORTED( fillBuffer, ( const SIRRenderStorageBuffer&, uint64_t, uint64_t, uint32_t ) )
SIR_VK_UNSUPPORTED( clearTexture, ( const SIRRenderTexture& ) )
SIR_VK_UNSUPPORTED( resolveTexture, ( const SIRRenderTexture&, const SIRRenderTexture& ) )
SIRRenderResult SIRVulkanCommandList::dispatch( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept
{
	if ( !m_bRecording || m_bInRenderPass || m_pBoundPipeline == nullptr || m_pBoundPipeline->bGraphics || nGroupCountX == 0 || nGroupCountY == 0 || nGroupCountZ == 0 ||
	     m_Commands.getSize() >= SIR_VULKAN_COPY_BATCH_CAPACITY )
		return SIRRenderResult::SIR_RENDER_INVALIDARGUMENT;
	sirVulkanRecordedCommand_t command{};
	command.eKind = SIRVulkanRecordedKind::SIR_VULKAN_RECORDED_COMPUTE_DISPATCH;
	command.pPipeline = const_cast<sirVulkanComputePipeline_t *>( m_pBoundPipeline );
	command.nGroupCountX = nGroupCountX;
	command.nGroupCountY = nGroupCountY;
	command.nGroupCountZ = nGroupCountZ;
	return sirVulkanCommandAppend( m_Commands, command ) ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_OUTOFMEMORY;
}
SIR_VK_UNSUPPORTED( dispatchIndirect, ( const SIRRenderStorageBuffer&, uint64_t ) )
SIR_VK_UNSUPPORTED( beginQuery, ( const SIRRenderQueryPool&, uint32_t ) )
SIR_VK_UNSUPPORTED( endQuery, ( const SIRRenderQueryPool&, uint32_t ) )
SIR_VK_UNSUPPORTED( writeTimestamp, ( const SIRRenderQueryPool&, uint32_t ) )
SIR_VK_UNSUPPORTED( resetQueries, ( const SIRRenderQueryPool&, uint32_t, uint32_t ) )
SIR_VK_UNSUPPORTED( resolveQueries, ( const SIRRenderQueryPool&, uint32_t, uint32_t, const SIRRenderStorageBuffer&, uint64_t ) )
SIR_VK_UNSUPPORTED( dispatchMesh, ( uint32_t, uint32_t, uint32_t ) )
SIR_VK_UNSUPPORTED( buildAccelerationStructure, ( const void* ) )
SIR_VK_UNSUPPORTED( copyAccelerationStructure, ( const void*, const void* ) )
SIR_VK_UNSUPPORTED( traceRays, ( const void* ) )
#undef SIR_VK_UNSUPPORTED
