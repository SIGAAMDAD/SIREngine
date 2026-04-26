#include "VulkanCommandBuffer.h"
#include "VulkanUtils.h"

using namespace SIREngine::Rendering::Vulkan;

VulkanCommandBuffer::VulkanCommandBuffer( const SVulkanInstance& instance, VulkanSwapChain *pSwapChain )
	: m_Instance( instance ), m_pSwapChain( pSwapChain ), m_nPresentId( 0 )
{
	{
		VkCommandPoolCreateInfo createInfo;
		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = instance.QueueFamilyIndices.nGraphicsFamily.value();

		VkResult result = vkCreateCommandPool( instance.hDevice, &createInfo, NULL, &m_hCommandPool );
	}
	{
		VkCommandBufferAllocateInfo allocInfo;
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_hCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

		VkResult result = vkAllocateCommandBuffers( m_Instance.hDevice, &allocInfo, m_hCommandBuffers );
	}
	{
		VkSemaphoreCreateInfo createInfo;
		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		uint32_t nImageCount = pSwapChain->GetImageCount();
		m_parrhImageAvailableSemaphores = (VkSemaphore *)calloc( sizeof( *m_parrhImageAvailableSemaphores ) * nImageCount, 1 );
		m_parrhRenderFinishedSemaphores = (VkSemaphore *)calloc( sizeof( *m_parrhRenderFinishedSemaphores ) * nImageCount, 1 );

		for ( uint32_t i = 0; i < nImageCount; i++ ) {
			vkCreateSemaphore( m_Instance.hDevice, &createInfo, NULL, &m_parrhImageAvailableSemaphores[ i ] );
			vkCreateSemaphore( m_Instance.hDevice, &createInfo, NULL, &m_parrhRenderFinishedSemaphores[ i ] );
		}
	}
	{
		VkFenceCreateInfo createInfo;
		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		vkCreateFence( m_Instance.hDevice, &createInfo, NULL, &m_hInFlightFences[ 0 ] );
		vkCreateFence( m_Instance.hDevice, &createInfo, NULL, &m_hInFlightFences[ 1 ] );
	}

	m_nCurrentFrame = 0;
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
	for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
		vkDestroyFence( m_Instance.hDevice, m_hInFlightFences[ i ], NULL );
	}
	for ( uint32_t i = 0; i < m_pSwapChain->GetImageCount(); i++ ) {
		vkDestroySemaphore( m_Instance.hDevice, m_parrhRenderFinishedSemaphores[ i ], NULL );
		vkDestroySemaphore( m_Instance.hDevice, m_parrhImageAvailableSemaphores[ i ], NULL );
	}
	if ( m_hCommandPool != VK_NULL_HANDLE ) {
		vkDestroyCommandPool( m_Instance.hDevice, m_hCommandPool, NULL );
	}
}

void VulkanCommandBuffer::BeginRecord( void )
{
	VkCommandBufferBeginInfo beginInfo;
	memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = NULL;

	VkResult result = vkBeginCommandBuffer( m_hCommandBuffers[ m_nCurrentFrame ], &beginInfo );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vkBeginCommandBuffer: %i\n", result );
	}
}

void VulkanCommandBuffer::EndRecord( void )
{
	VkResult result = vkEndCommandBuffer( m_hCommandBuffers[ m_nCurrentFrame ] );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vkEndCommandBuffer: %i\n", result );
	}
}

void VulkanCommandBuffer::BindVertexBuffer( const VulkanVertexBuffer *pBuffer )
{
	const VkBuffer arrBuffers[] = { pBuffer->GetNativeHandle() };
	const VkDeviceSize arrOffsets[] = { 0 };
	vkCmdBindVertexBuffers( m_hCommandBuffers[ m_nCurrentFrame ], 0, 1, arrBuffers, arrOffsets );
}

void VulkanCommandBuffer::BeginRenderPass( VkRenderPass hRenderPass, const VkViewport& viewport, const VkRect2D& scissor )
{
	VkRenderPassBeginInfo renderPassInfo;
	memset( &renderPassInfo, 0, sizeof( renderPassInfo ) );
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = hRenderPass;
	renderPassInfo.framebuffer = m_pSwapChain->GetFramebuffer( m_nImageIndex );
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = m_pSwapChain->GetExtent();
	
	VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass( m_hCommandBuffers[ m_nCurrentFrame ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

	vkCmdSetViewport( m_hCommandBuffers[ m_nCurrentFrame ], 0, 1, &viewport );
	vkCmdSetScissor( m_hCommandBuffers[ m_nCurrentFrame ], 0, 1, &scissor );
}

void VulkanCommandBuffer::EndRenderPass( void )
{
	vkCmdEndRenderPass( m_hCommandBuffers[ m_nCurrentFrame ] );
}

void VulkanCommandBuffer::BindPipeline( VkPipeline hPipeline )
{
	vkCmdBindPipeline( m_hCommandBuffers[ m_nCurrentFrame ], VK_PIPELINE_BIND_POINT_GRAPHICS, hPipeline );
}

void VulkanCommandBuffer::Draw( uint32_t nVertexCount, uint32_t nInstanceCount, uint32_t nFirstVertex, uint32_t nFirstInstance )
{
	vkCmdDraw( m_hCommandBuffers[ m_nCurrentFrame ], nVertexCount, nInstanceCount, nFirstVertex, nFirstInstance );
}

void VulkanCommandBuffer::Reset( void )
{
	VkResult result = vkWaitForFences( m_Instance.hDevice, 1, &m_hInFlightFences[ m_nPreviousFrame ], VK_TRUE, SIRENGINE_UINT64_MAX );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vkWaitForFences: %i\n", result );
	}

	vkResetFences( m_Instance.hDevice, 1, &m_hInFlightFences[ m_nPreviousFrame ] );

	result = vkAcquireNextImageKHR( m_Instance.hDevice, m_pSwapChain->GetSwapChain(), SIRENGINE_UINT64_MAX, m_parrhImageAvailableSemaphores[ m_nImageIndex ], VK_NULL_HANDLE, &m_nImageIndex );
	if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
		m_pSwapChain->Recreate();
		return;
	}
	else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
		printf( "Error in vkAcquireNextImageKHR: %i\n", result );
	}

	result = vkResetCommandBuffer( m_hCommandBuffers[ m_nCurrentFrame ], 0 );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vkResetCommandBuffer: %i\n", result );
	}
}

void VulkanCommandBuffer::PresentFrame( VkQueue hPresentQueue )
{
	VkPresentInfoKHR presentInfo;
	memset( &presentInfo, 0, sizeof( presentInfo ) );
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_parrhRenderFinishedSemaphores[ m_nImageIndex ];

	const VkSwapchainKHR arrSwapChains[] = { m_pSwapChain->GetSwapChain() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = arrSwapChains;
	presentInfo.pImageIndices = &m_nImageIndex;
	presentInfo.pResults = NULL;

	VkResult result = vkQueuePresentKHR( hPresentQueue, &presentInfo );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vkQueuePresentKHR: %i\n", result );
	}

	m_nPreviousFrame = m_nCurrentFrame;
	m_nCurrentFrame = ( m_nCurrentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanCommandBuffer::SubmitQueue( VkQueue hQueue )
{
	VkSubmitInfo submitInfo;
	memset( &submitInfo, 0, sizeof( submitInfo ) );
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_parrhImageAvailableSemaphores[ m_nImageIndex ];
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_hCommandBuffers[ m_nCurrentFrame ];

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_parrhRenderFinishedSemaphores[ m_nImageIndex ];

	VkResult result = vkQueueSubmit( hQueue, 1, &submitInfo, m_hInFlightFences[ m_nCurrentFrame ] );
	if ( result != VK_SUCCESS ) {
		printf( "Error in vkQueueSubmit: %i\n", result );
	}
}