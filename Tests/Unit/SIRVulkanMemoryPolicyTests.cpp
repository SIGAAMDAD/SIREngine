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

int main( void )
{
	if ( sirVulkanValidatePoolBudget( nullptr ) ) {
		return 1;
	}
	sirVulkanPoolBudget_t budget = {};
	if ( sirVulkanValidatePoolBudget( &budget ) ) {
		return 2;
	}
	budget = { 8ULL << 20, 2ULL << 20, 2ULL << 20 };
	if ( !sirVulkanValidatePoolBudget( &budget ) ) {
		return 3;
	}
	const sirVulkanPoolBudget_t overflowing = { UINT64_MAX - 65535ULL, 65536, 65536 };
	if ( sirVulkanValidatePoolBudget( &overflowing ) ) {
		return 16;
	}
	budget.nResidentBytes++;
	if ( sirVulkanValidatePoolBudget( &budget ) ) {
		return 4;
	}
	budget.nResidentBytes--;
	budget.nUploadBytes = 0;
	if ( sirVulkanValidatePoolBudget( &budget ) ) {
		return 5;
	}
	budget.nUploadBytes = 65535;
	if ( sirVulkanValidatePoolBudget( &budget ) ) {
		return 6;
	}
	budget.nUploadBytes = 2ULL << 20;
	budget.nReadbackBytes = UINT64_MAX;
	if ( sirVulkanValidatePoolBudget( &budget ) ) {
		return 7;
	}
	SIRVulkanDriver driver;
	sirGraphicsDeviceCreateDesc_t create = {};
	create.bHeadless = true;
	create.nFramesInFlight = 2;
	create.nResidentBudgetBytes = 8ULL << 20;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT || pDevice != nullptr ) {
		return 8;
	}
	create.nUploadBudgetBytes = 2ULL << 20;
	create.nReadbackBudgetBytes = 2ULL << 20;
	create.nResidentBudgetBytes++;
	if ( driver.createDevice( create, &pDevice ) != SIR_GRAPHICS_INVALID_ARGUMENT || pDevice != nullptr ) {
		return 9;
	}
	if ( sirVulkanPlacementPool( SIR_VULKAN_BUFFER_GPU_LOCAL ) != 0 ||
	     sirVulkanPlacementPool( SIR_VULKAN_BUFFER_CPU_UPLOAD ) != 1 ||
	     sirVulkanPlacementPool( SIR_VULKAN_BUFFER_GPU_UPLOAD ) != 1 ||
	     sirVulkanPlacementPool( SIR_VULKAN_BUFFER_CPU_READBACK ) != 2 ) {
		return 10;
	}
	if ( sirVulkanPlacementHostVisible( SIR_VULKAN_BUFFER_GPU_LOCAL ) ||
	     !sirVulkanPlacementHostVisible( SIR_VULKAN_BUFFER_CPU_UPLOAD ) ||
	     !sirVulkanPlacementHostVisible( SIR_VULKAN_BUFFER_CPU_READBACK ) ||
	     !sirVulkanPlacementHostVisible( SIR_VULKAN_BUFFER_GPU_UPLOAD ) ) {
		return 11;
	}
	if ( !sirVulkanMemoryTypeCompatible( 0, 1U ) ||
	     !sirVulkanMemoryTypeCompatible( 31, 1U << 31 ) ||
	     sirVulkanMemoryTypeCompatible( 32, UINT32_MAX ) ||
	     sirVulkanMemoryTypeCompatible( 3, 1U << 2 ) ||
	     sirVulkanMemoryTypeCompatible( 0, 0 ) ) {
		return 12;
	}
	uint64_t nPeakCommitted = 0;
	uint64_t nPeakAllocated = 0;
	sirVulkanAdvanceMemoryPeak( 1024, 512, &nPeakCommitted, &nPeakAllocated );
	sirVulkanAdvanceMemoryPeak( 256, 128, &nPeakCommitted, &nPeakAllocated );
	if ( nPeakCommitted != 1024 || nPeakAllocated != 512 ) {
		return 13;
	}
	sirVulkanAdvanceMemoryPeak( 2048, 1024, &nPeakCommitted, &nPeakAllocated );
	if ( nPeakCommitted != 2048 || nPeakAllocated != 1024 ) {
		return 14;
	}
	sirVulkanAdvanceMemoryPeak( 4096, 4096, nullptr, &nPeakAllocated );
	sirVulkanAdvanceMemoryPeak( 4096, 4096, &nPeakCommitted, nullptr );
	if ( nPeakCommitted != 2048 || nPeakAllocated != 1024 ) {
		return 15;
	}
	return 0;
}
