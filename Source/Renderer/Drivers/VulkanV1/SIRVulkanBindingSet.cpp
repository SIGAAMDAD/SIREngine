#include <Renderer/Drivers/Vulkan/SIRVulkanBindingSet.h>

#include <Renderer/Drivers/Vulkan/SIRVulkanStorageBuffer.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanTexture.h>

static VkDescriptorType sirVulkanBindingType( SIRRenderBindingType eType ) noexcept
{
	switch ( eType ) {
	case SIRRenderBindingType::SIR_RENDER_BINDING_CONSTANT_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case SIRRenderBindingType::SIR_RENDER_BINDING_SAMPLED_TEXTURE:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case SIRRenderBindingType::SIR_RENDER_BINDING_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case SIRRenderBindingType::SIR_RENDER_BINDING_STORAGE_TEXTURE:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case SIRRenderBindingType::SIR_RENDER_BINDING_SAMPLER:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	default:
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

SIRVulkanBindingSet::SIRVulkanBindingSet( const SIRSharedPtr<SIRRenderDevice>& device,
                                          const SIRSharedPtr<SIRRenderBindingLayout>& layout,
                                          SIRVector<SIRRenderBindingDesc>&& bindings ) noexcept
    : SIRRenderBindingSet( device, layout, static_cast<SIRVector<SIRRenderBindingDesc>&&>( bindings ) ),
      m_References( nullptr )
{
	SIRVulkanDevice *pDevice = device.get() != nullptr && device->getBackend() == SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN
	                               ? &getDevice<SIRVulkanDevice>() : nullptr;
	const SIRVulkanBindingLayout *pLayout = dynamic_cast<const SIRVulkanBindingLayout *>( layout.get() );
	if ( pDevice != nullptr && pLayout != nullptr && pLayout->isValid() && pLayout->getDevice().get() == device.get() ) {
		static_cast<void>( pDevice->createDescriptorSet( pLayout->getNativeLayout(), m_Bindings.data(), m_Bindings.getSize(), &m_Native ) );
	}
}

SIRVulkanBindingSet::~SIRVulkanBindingSet() noexcept
{
	SIRVulkanDevice *pDevice = getDevice().get() != nullptr ? &getDevice<SIRVulkanDevice>() : nullptr;
	if ( pDevice != nullptr && m_Native.hSet != VK_NULL_HANDLE ) {
		if ( m_Native.nLastGpuUse != 0 ) {
			const sirVulkanCompletion_t completion{ m_Native.nDeviceSerial, m_Native.nLastGpuUse };
			if ( pDevice->waitCompletion( &completion, UINT64_MAX ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
				return;
			}
		}
		static_cast<void>( pDevice->destroyDescriptorSet( &m_Native ) );
	}
}

SIRVulkanResult SIRVulkanBindingSet::update( SIRSpan<const SIRVulkanDescriptorWrite> writes ) noexcept
{
	SIRVulkanDevice *pDevice = getDevice().get() != nullptr ? &getDevice<SIRVulkanDevice>() : nullptr;
	if ( pDevice == nullptr || !isValid() || writes.getData() == nullptr || writes.getSize() == 0 || writes.getSize() > 32 )
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;

	VkWriteDescriptorSet nativeWrites[32]{};
	VkDescriptorBufferInfo buffers[32]{};
	VkDescriptorImageInfo images[32]{};
	const uint32_t nWrites = static_cast<uint32_t>( writes.getSize() );
	uint32_t nNewReferences = m_References.getSize();
	for ( uint32_t i = 0; i < nWrites; ++i ) {
		bool bKnown = false;
		for ( uint32_t j = 0; j < m_References.getSize(); ++j ) {
			if ( m_References[j].nBinding == writes[i].nBinding && m_References[j].nArrayIndex == writes[i].nArrayIndex ) {
				bKnown = true;
				break;
			}
		}
		for ( uint32_t j = 0; !bKnown && j < i; ++j ) {
			if ( writes[j].nBinding == writes[i].nBinding && writes[j].nArrayIndex == writes[i].nArrayIndex )
				bKnown = true;
		}
		if ( !bKnown )
			++nNewReferences;
	}
	if ( m_References.tryReserve( nNewReferences ) != SIRContainerResult::SIR_CONTAINER_OK )
		return SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED;
	for ( uint32_t i = 0; i < nWrites; ++i ) {
		const SIRVulkanDescriptorWrite& write = writes[i];
		if ( !write.pResource.isValid() || write.pResource->getDevice().get() != getDevice().get() )
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;

		const SIRRenderBindingDesc *pBinding = nullptr;
		for ( uint32_t j = 0; j < m_Bindings.getSize(); ++j ) {
			if ( m_Bindings[j].nBinding == write.nBinding ) {
				pBinding = &m_Bindings[j];
				break;
			}
		}
		if ( pBinding == nullptr || write.nArrayIndex >= pBinding->nArrayCount )
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( writes[j].nBinding == write.nBinding && writes[j].nArrayIndex == write.nArrayIndex )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}

		const VkDescriptorType eType = sirVulkanBindingType( pBinding->eType );
		if ( eType == VK_DESCRIPTOR_TYPE_MAX_ENUM )
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		nativeWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		nativeWrites[i].dstSet = m_Native.hSet;
		nativeWrites[i].dstBinding = write.nBinding;
		nativeWrites[i].dstArrayElement = write.nArrayIndex;
		nativeWrites[i].descriptorCount = 1;
		nativeWrites[i].descriptorType = eType;

		if ( eType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || eType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			const SIRVulkanStorageBuffer *pBuffer = dynamic_cast<const SIRVulkanStorageBuffer *>( write.pResource.get() );
			const VkBufferUsageFlags nUsage = eType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			if ( pBuffer == nullptr || ( pBuffer->getNativeBuffer().nUsage & nUsage ) == 0 || write.nOffsetBytes >= pBuffer->getSize() ||
			     ( write.nRangeBytes != 0 && write.nRangeBytes > pBuffer->getSize() - write.nOffsetBytes ) )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			buffers[i].buffer = pBuffer->getNativeBuffer().hBuffer;
			buffers[i].offset = write.nOffsetBytes;
			buffers[i].range = write.nRangeBytes == 0 ? VK_WHOLE_SIZE : write.nRangeBytes;
			nativeWrites[i].pBufferInfo = &buffers[i];
		} else if ( eType == VK_DESCRIPTOR_TYPE_SAMPLER ) {
			const SIRVulkanSampler *pSampler = dynamic_cast<const SIRVulkanSampler *>( write.pResource.get() );
			if ( pSampler == nullptr || write.nOffsetBytes != 0 || write.nRangeBytes != 0 || pSampler->getNativeSampler() == VK_NULL_HANDLE )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			images[i].sampler = pSampler->getNativeSampler();
			nativeWrites[i].pImageInfo = &images[i];
		} else {
			const SIRVulkanTextureView *pView = dynamic_cast<const SIRVulkanTextureView *>( write.pResource.get() );
			const SIRVulkanTexture *pTexture = pView == nullptr ? nullptr : dynamic_cast<const SIRVulkanTexture *>( &pView->getTexture() );
			const VkImageUsageFlags nUsage = eType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? VK_IMAGE_USAGE_SAMPLED_BIT : VK_IMAGE_USAGE_STORAGE_BIT;
			if ( pView == nullptr || pTexture == nullptr || pView->getNativeView() == VK_NULL_HANDLE || ( pTexture->getNativeImage().nUsage & nUsage ) == 0 ||
			     write.nOffsetBytes != 0 || write.nRangeBytes != 0 )
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			images[i].imageView = pView->getNativeView();
			images[i].imageLayout =
			    eType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
			        ? VK_IMAGE_LAYOUT_GENERAL
			        : ( ( sirVulkanImageAspectMask( pTexture->getNativeImage().eFormat ) & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			                                                                                                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
			nativeWrites[i].pImageInfo = &images[i];
		}
	}

	const SIRVulkanResult eUpdate = pDevice->updateDescriptorSet( &m_Native, nativeWrites, nWrites );
	if ( eUpdate != SIRVulkanResult::SIR_VULKAN_SUCCESS )
		return eUpdate;
	for ( uint32_t i = 0; i < nWrites; ++i ) {
		uint32_t nReference = 0;
		while ( nReference < m_References.getSize() &&
		        ( m_References[nReference].nBinding != writes[i].nBinding || m_References[nReference].nArrayIndex != writes[i].nArrayIndex ) )
			++nReference;
		if ( nReference == m_References.getSize() ) {
			if ( m_References.tryEmplaceBack( Reference{ writes[i].nBinding, writes[i].nArrayIndex, writes[i].pResource } ) != SIRContainerResult::SIR_CONTAINER_OK )
				return SIRVulkanResult::SIR_VULKAN_ALLOCATION_FAILED;
		} else {
			m_References[nReference].pResource = writes[i].pResource;
		}
	}
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}
