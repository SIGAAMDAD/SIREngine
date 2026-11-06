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

#include <Renderer/Drivers/Vulkan/SIRVulkanGraphics.h>

// The shared graphics barrier has no buffer offset/range. Buffer
// transitions therefore synchronize the entire native allocation.
// Vulkan buffer barriers order access; buffers have no image-like layout.
static sirGraphicsResult_t sirVulkanBufferAccess( sirGraphicsState_t eState, VkBufferUsageFlags nUsage, VkPipelineStageFlags2 *pStages,
                                                  VkAccessFlags2 *pAccess ) noexcept
{
	if ( pStages == nullptr || pAccess == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pStages = VK_PIPELINE_STAGE_2_NONE;
	*pAccess = VK_ACCESS_2_NONE;
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_UNDEFINED:
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_COMMON:
		*pStages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*pAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_VERTEX_INDEX:
		if ( ( nUsage & ( VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pStages = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		if ( ( nUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ) != 0 ) {
			*pAccess |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		}
		if ( ( nUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) != 0 ) {
			*pAccess |= VK_ACCESS_2_INDEX_READ_BIT;
		}
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_CONSTANT:
		if ( ( nUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pStages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*pAccess = VK_ACCESS_2_UNIFORM_READ_BIT;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_SHADER_READ:
		if ( ( nUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pStages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*pAccess = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		if ( ( nUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pStages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*pAccess = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
		if ( ( nUsage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pStages = VK_PIPELINE_STAGE_2_COPY_BIT;
		*pAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		if ( ( nUsage & VK_BUFFER_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pStages = VK_PIPELINE_STAGE_2_COPY_BIT;
		*pAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		return SIR_GRAPHICS_SUCCESS;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
}

sirGraphicsResult_t sirVulkanPlanBufferBarrier( const sirGraphicsBarrier_t& barrier, const sirVulkanBuffer_t *pBuffer,
                                                sirVulkanBufferBarrier_t *pNative ) noexcept
{
	if ( pNative == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	if ( pBuffer == nullptr || pBuffer->nSize == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( barrier.eType != SIR_GRAPHICS_BARRIER_TRANSITION && barrier.eType != SIR_GRAPHICS_BARRIER_UAV ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( barrier.nBaseMip != 0 || barrier.nMipCount != 0 || barrier.nBaseLayer != 0 || barrier.nLayerCount != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( barrier.eAfter == SIR_GRAPHICS_STATE_UNDEFINED ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( barrier.eType == SIR_GRAPHICS_BARRIER_UAV ) {
		if ( barrier.eBefore != SIR_GRAPHICS_STATE_SHADER_WRITE || barrier.eAfter != SIR_GRAPHICS_STATE_SHADER_WRITE ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( barrier.eBefore == barrier.eAfter ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirVulkanBufferBarrier_t planned = {};
	const sirGraphicsResult_t eBefore =
	    sirVulkanBufferAccess( barrier.eBefore, pBuffer->nUsage, &planned.nSourceStages, &planned.nSourceAccess );
	if ( eBefore != SIR_GRAPHICS_SUCCESS ) {
		return eBefore;
	}
	const sirGraphicsResult_t eAfter =
	    sirVulkanBufferAccess( barrier.eAfter, pBuffer->nUsage, &planned.nDestinationStages, &planned.nDestinationAccess );
	if ( eAfter != SIR_GRAPHICS_SUCCESS ) {
		return eAfter;
	}
	planned.bUav = barrier.eType == SIR_GRAPHICS_BARRIER_UAV;
	*pNative = planned;
	return SIR_GRAPHICS_SUCCESS;
}
