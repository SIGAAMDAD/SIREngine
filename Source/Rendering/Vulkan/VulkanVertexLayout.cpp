#include "VulkanVertexLayout.h"

using namespace SIREngine::Rendering;
using namespace SIREngine::Rendering::Vulkan;

static VkFormat ConvertToVkFormat( EVertexAttributeType eType )
{
	switch ( eType ) {
		case EVertexAttributeType::Float:
			return VK_FORMAT_R32_SFLOAT;
		case EVertexAttributeType::Float2:
			return VK_FORMAT_R32G32_SFLOAT;
		case EVertexAttributeType::Float3:
			return VK_FORMAT_R32G32B32_SFLOAT;
		case EVertexAttributeType::Float4:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case EVertexAttributeType::Int:
			return VK_FORMAT_R32_SINT;
		case EVertexAttributeType::Int2:
			return VK_FORMAT_R32G32_SINT;
		case EVertexAttributeType::Int3:
			return VK_FORMAT_R32G32B32_SINT;
		case EVertexAttributeType::Int4:
			return VK_FORMAT_R32G32B32A32_SINT;
		case EVertexAttributeType::UInt:
			return VK_FORMAT_R32_UINT;
		case EVertexAttributeType::UInt2:
			return VK_FORMAT_R32G32_UINT;
		case EVertexAttributeType::UInt3:
			return VK_FORMAT_R32G32B32_UINT;
		case EVertexAttributeType::UInt4:
			return VK_FORMAT_R32G32B32A32_UINT;
		case EVertexAttributeType::Byte:
			return VK_FORMAT_R8_UINT;
		case EVertexAttributeType::Byte2:
			return VK_FORMAT_R8G8_UINT;
		case EVertexAttributeType::Byte4:
			return VK_FORMAT_R8G8B8A8_UINT;
		default:
			return (VkFormat)0;
	}
}

VulkanVertexLayout::VulkanVertexLayout( const SVulkanInstance& instance )
{
}

VulkanVertexLayout::~VulkanVertexLayout()
{
}

void VulkanVertexLayout::AddAttribute( const SVertexAttribute& attrib )
{
	VkVertexInputAttributeDescription attribDescription;
	memset( &attribDescription, 0, sizeof( attribDescription ) );
	attribDescription.location = attrib.nLocation;
	attribDescription.binding = 0;
	attribDescription.format = ConvertToVkFormat( attrib.eType );
	attribDescription.offset = attrib.nOffset;

	m_Attributes.emplace_back( attribDescription );
}

void VulkanVertexLayout::AddBinding( const SVertexBinding& binding )
{
	VkVertexInputBindingDescription bindingDescription;
	memset( &bindingDescription, 0, sizeof( bindingDescription ) );
	bindingDescription.binding = binding.nBinding;
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDescription.stride = binding.nStride;

	m_Bindings.emplace_back( bindingDescription );
}

void VulkanVertexLayout::LinkAttributeToBinding( uint32_t nAttributeLocation, uint32_t nBindingIndex )
{
	m_Attributes[ nAttributeLocation ].binding = nBindingIndex;
}

bool VulkanVertexLayout::Build( void )
{
	memset( &m_VertexInputInfo, 0, sizeof( m_VertexInputInfo ) );
	m_VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_VertexInputInfo.vertexAttributeDescriptionCount = m_Attributes.size();
	m_VertexInputInfo.pVertexAttributeDescriptions = m_Attributes.data();
	m_VertexInputInfo.vertexBindingDescriptionCount = m_Bindings.size();
	m_VertexInputInfo.pVertexBindingDescriptions = m_Bindings.data();

	return true;
}

size_t VulkanVertexLayout::GetHash( void ) const
{
	return 0;
}