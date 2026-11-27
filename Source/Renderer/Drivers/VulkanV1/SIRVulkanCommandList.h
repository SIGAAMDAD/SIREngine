#ifndef __SIR_VULKAN_COMMAND_LIST_H__
#define __SIR_VULKAN_COMMAND_LIST_H__

#include <Renderer/RHI/SIRRenderCommandList.h>
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRContainerResult.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

class SIRVulkanCommandList final : public SIRRenderCommandList
{
public:
	// Native render-pass data is intentionally owned by the caller for the
	// duration of beginRenderPass(). It is copied into the recorded command.
	struct RenderPassDescription
	{
		uint32_t nColorCount{};
		VkImageView szColorViews[8]{};
		sirVulkanImage_t *szColorImages[8]{};
		uint32_t szColorMips[8]{};
		uint32_t szColorLayers[8]{};
		VkClearValue szClearColors[8]{};
		bool szColorClear[8]{};
		bool szColorStore[8]{};
		VkImageView hDepthView{ VK_NULL_HANDLE };
		sirVulkanImage_t *pDepthImage{};
		uint32_t nDepthMip{};
		uint32_t nDepthLayer{};
		VkClearValue depthClear{};
		bool bClearDepth{};
		bool bStoreDepth{};
		VkExtent2D extent{};
	};

	// transition() consumes an array of these records through SIRSpan<const
	// uint8_t>. The span must contain whole records and remain valid only for
	// the duration of the call; the command list copies each record.
	struct TransitionDescription
	{
		bool bImage{};
		sirVulkanBuffer_t *pBuffer{};
		sirVulkanImage_t *pImage{};
		sirVulkanBufferBarrier_t buffer{};
		sirVulkanImageBarrier_t image{};
	};

	SIRVulkanCommandList( const SIRSharedPtr<SIRRenderDevice>& device, SIRRenderQueueType queueType ) noexcept;
	~SIRVulkanCommandList() noexcept override = default;

	SIRRenderResult begin( void ) noexcept override;
	SIRRenderResult end( void ) noexcept override;
	SIRRenderResult reset( void ) noexcept override;
	SIRRenderResult beginRenderPass( void *pPass ) noexcept override;
	SIRRenderResult endRenderPass( void ) noexcept override;
	SIRRenderResult bindPipeline( const SIRRenderPipeline& pipeline ) noexcept override;
	SIRRenderResult bindBindingSet( uint32_t nSetIndex, const SIRRenderBindingSet& bindingSet ) noexcept override;
	SIRRenderResult bindVertexBuffer( uint32_t nSlot, const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nStrideBytes ) noexcept override;
	SIRRenderResult bindIndexBuffer( const SIRRenderStorageBuffer& buffer, uint64_t nOffsetBytes, uint32_t nIndexStrideBytes ) noexcept override;
	SIRRenderResult setViewport( const SIRRenderViewport& viewport, const SIRRenderRect& scissor ) noexcept override;
	SIRRenderResult setConstants( uint32_t nOffsetBytes, SIRSpan<const uint8_t> constants ) noexcept override;
	SIRRenderResult draw( uint32_t nVertexCount, uint32_t nInstanceCount, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept override;
	SIRRenderResult drawIndexed( uint32_t nIndexCount, uint32_t nInstanceCount, uint32_t nFirstIndex, int32_t nVertexOffset, uint32_t nFirstInstance ) noexcept override;
	SIRRenderResult drawIndirect( const SIRRenderStorageBuffer&, uint64_t, uint32_t, uint32_t ) noexcept override;
	SIRRenderResult drawIndexedIndirect( const SIRRenderStorageBuffer&, uint64_t, uint32_t, uint32_t ) noexcept override;
	SIRRenderResult copyBuffer( const SIRRenderStorageBuffer& destination, const SIRRenderStorageBuffer& source, const SIRRenderBufferRange& destinationRange, const SIRRenderBufferRange& sourceRange ) noexcept override;
	SIRRenderResult copyTexture( const SIRRenderTexture&, const SIRRenderTexture&, const SIRRenderSubresourceRange&, const SIRRenderSubresourceRange& ) noexcept override;
	SIRRenderResult copyBufferToTexture( const SIRRenderStorageBuffer&, const SIRRenderTexture&, const SIRRenderBufferRange&, const SIRRenderSubresourceRange& ) noexcept override;
	SIRRenderResult copyTextureToBuffer( const SIRRenderTexture&, const SIRRenderStorageBuffer&, const SIRRenderSubresourceRange&, const SIRRenderBufferRange& ) noexcept override;
	SIRRenderResult fillBuffer( const SIRRenderStorageBuffer&, uint64_t, uint64_t, uint32_t ) noexcept override;
	SIRRenderResult clearTexture( const SIRRenderTexture& ) noexcept override;
	SIRRenderResult resolveTexture( const SIRRenderTexture&, const SIRRenderTexture& ) noexcept override;
	SIRRenderResult transition( SIRSpan<const uint8_t> barriers ) noexcept override;
	SIRRenderResult dispatch( uint32_t nGroupCountX, uint32_t nGroupCountY, uint32_t nGroupCountZ ) noexcept override;
	SIRRenderResult dispatchIndirect( const SIRRenderStorageBuffer&, uint64_t ) noexcept override;
	SIRRenderResult beginQuery( const SIRRenderQueryPool&, uint32_t ) noexcept override;
	SIRRenderResult endQuery( const SIRRenderQueryPool&, uint32_t ) noexcept override;
	SIRRenderResult writeTimestamp( const SIRRenderQueryPool&, uint32_t ) noexcept override;
	SIRRenderResult resetQueries( const SIRRenderQueryPool&, uint32_t, uint32_t ) noexcept override;
	SIRRenderResult resolveQueries( const SIRRenderQueryPool&, uint32_t, uint32_t, const SIRRenderStorageBuffer&, uint64_t ) noexcept override;
	SIRRenderResult dispatchMesh( uint32_t, uint32_t, uint32_t ) noexcept override;
	SIRRenderResult buildAccelerationStructure( const void* ) noexcept override;
	SIRRenderResult copyAccelerationStructure( const void*, const void* ) noexcept override;
	SIRRenderResult traceRays( const void* ) noexcept override;

	SIRRenderResult submitNative( sirVulkanCompletion_t *pCompletion, const sirVulkanSubmitSync_t *pSync = nullptr ) noexcept;

private:
	SIRVector<sirVulkanRecordedCommand_t> m_Commands;
	const sirVulkanComputePipeline_t *m_pBoundPipeline{};
	bool m_bInRenderPass{};
	bool m_bViewportSet{};
	bool m_bIndexBufferBound{};
	bool m_bRecording{};
};

#endif
