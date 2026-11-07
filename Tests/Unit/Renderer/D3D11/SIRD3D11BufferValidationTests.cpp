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

#include <Renderer/Drivers/D3D11/SIRD3D11BufferValidation.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t s_nAssertions = 0;
static uint32_t s_nFailures = 0;
static void sirD3D11Check( bool bValid ) noexcept
{
	++s_nAssertions;
	if ( !bValid ) {
		++s_nFailures;
	}
}

int main( void )
{
	sirGraphicsBufferDesc_t desc = { 256, 16, SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_COPY_DESTINATION,
	                                 SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	sirD3D11BufferPlan_t plan = {};
	sirD3D11Check( sirD3D11PlanBuffer( nullptr, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 256, &plan ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( plan.eStorage == SIR_D3D11_BUFFER_DEFAULT && plan.nSizeBytes == 256 );
	sirD3D11Check( plan.nNativeBindUsage == SIR_GRAPHICS_USAGE_VERTEX && plan.nMappedAccess == 0 );
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 1, 256, &plan ) == SIR_GRAPHICS_OUT_OF_MEMORY );
	sirD3D11Check( sirD3D11PlanBuffer( &desc, UINT64_MAX, 0, &plan ) == SIR_GRAPHICS_OUT_OF_MEMORY );
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS );
	desc.nSizeBytes = 0;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nSizeBytes = ( uint64_t )UINT32_MAX + 1U;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nSizeBytes = 256;
	desc.nAlignmentBytes = 3;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nAlignmentBytes = 32;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nAlignmentBytes = 0;
	desc.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nUsage = SIR_GRAPHICS_USAGE_INDIRECT;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nUsage = SIR_GRAPHICS_USAGE_CONSTANT;
	desc.nSizeBytes = 17;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nSizeBytes = 65552;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nSizeBytes = 64;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS );
	desc.nUsage |= SIR_GRAPHICS_USAGE_INDEX;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( plan.eStorage == SIR_D3D11_BUFFER_DYNAMIC && plan.nMappedAccess == 1 );
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_READBACK;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( plan.eStorage == SIR_D3D11_BUFFER_STAGING && plan.nMappedAccess == 2 );
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_SUCCESS );
	desc.nUsage = 0;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_UPLOAD;
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	sirD3D11Check( sirD3D11PlanBuffer( &desc, 0, 0, &plan ) == SIR_GRAPHICS_UNSUPPORTED );
	printf( "D3D11 buffer validation tests: %u assertions, %u failures\n", s_nAssertions, s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
