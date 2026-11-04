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
	return 0;
}
