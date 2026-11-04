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

#include <stdio.h>

int main( void )
{
	sirVulkanBuffer_t buffer = {};
	buffer.nSize = 4096;
	buffer.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
	                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	sirGraphicsBarrier_t barrier = {};
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.eBefore = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	sirVulkanBufferBarrier_t native = {};
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.nSourceStages != VK_PIPELINE_STAGE_2_COPY_BIT || native.nDestinationStages != VK_PIPELINE_STAGE_2_COPY_BIT ||
	     native.nSourceAccess != VK_ACCESS_2_TRANSFER_WRITE_BIT || native.nDestinationAccess != VK_ACCESS_2_TRANSFER_READ_BIT ||
	     native.bUav ) {
		return 1;
	}
	sirVulkanRecordedCommand_t commands[2] = {};
	commands[0].eKind = SIR_VULKAN_RECORDED_BUFFER_BARRIER;
	commands[0].pBuffer = &buffer;
	commands[0].bufferBarrier = native;
	if ( sirVulkanValidateRecordedCommands( commands, 1 ) != SIR_VULKAN_SUCCESS || buffer.nLastGpuUse != 0 ) {
		return 2;
	}
	barrier.eBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.nSourceStages != VK_PIPELINE_STAGE_2_NONE || native.nSourceAccess != VK_ACCESS_2_NONE ) {
		return 3;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_UAV;
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_SUCCESS || !native.bUav ||
	     native.nSourceAccess != VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ||
	     native.nDestinationAccess != VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ) {
		return 4;
	}
	commands[0].bufferBarrier = native;
	if ( sirVulkanValidateRecordedCommands( commands, 1 ) != SIR_VULKAN_SUCCESS ) {
		return 5;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.nSourceAccess != VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT ||
	     native.nDestinationAccess != VK_ACCESS_2_SHADER_STORAGE_READ_BIT ) {
		return 6;
	}
	barrier.eBefore = SIR_GRAPHICS_STATE_VERTEX_INDEX;
	barrier.eAfter = SIR_GRAPHICS_STATE_CONSTANT;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_SUCCESS ||
	     native.nSourceStages != VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT ||
	     native.nSourceAccess != ( VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT ) ||
	     native.nDestinationAccess != VK_ACCESS_2_UNIFORM_READ_BIT ) {
		return 7;
	}
	buffer.nUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 8;
	}
	barrier.eBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 9;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_UAV;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 10;
	}
	buffer.nUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 11;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_ALIAS;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 12;
	}
	barrier.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.eAfter = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 13;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	barrier.nMipCount = 1;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return 14;
	}
	barrier.nMipCount = 0;
	barrier.eAfter = SIR_GRAPHICS_STATE_UNDEFINED;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 15;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( sirVulkanPlanBufferBarrier( barrier, &buffer, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 16;
	}
	barrier.eAfter = SIR_GRAPHICS_STATE_COPY_SOURCE;
	if ( sirVulkanPlanBufferBarrier( barrier, nullptr, &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirVulkanPlanBufferBarrier( barrier, &buffer, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 17;
	}
	commands[0].pBuffer = nullptr;
	if ( sirVulkanValidateRecordedCommands( commands, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 18;
	}
	commands[0].pBuffer = &buffer;
	commands[0].bufferBarrier = {};
	if ( sirVulkanValidateRecordedCommands( commands, 1 ) != SIR_VULKAN_INVALID_ARGUMENT ) {
		return 19;
	}
	printf( "Vulkan buffer barrier contracts passed\n" );
	return 0;
}
