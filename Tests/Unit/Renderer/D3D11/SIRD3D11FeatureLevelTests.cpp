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

#include <Renderer/Drivers/D3D11/SIRD3D11FeatureLevels.h>
#include <stdio.h>
#include <string.h>

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
	const uint32_t nLevels[] = { SIR_D3D11_FEATURE_LEVEL_9_1,  SIR_D3D11_FEATURE_LEVEL_9_2,  SIR_D3D11_FEATURE_LEVEL_9_3,
	                             SIR_D3D11_FEATURE_LEVEL_10_0, SIR_D3D11_FEATURE_LEVEL_10_1, SIR_D3D11_FEATURE_LEVEL_11_0,
	                             SIR_D3D11_FEATURE_LEVEL_11_1 };
	for ( uint32_t iLevel = 0; iLevel < 7; ++iLevel ) {
		sirD3D11FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe );
		probe.nFeatureLevel = nLevels[iLevel];
		probe.nD3D11_1InterfaceAvailable = iLevel == 6 ? 1U : 0U;
		sirD3D11FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
		sirD3D11Check( profile.nFeatureLevel == probe.nFeatureLevel );
		sirD3D11Check( profile.nDirectQueueMask == ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) );
		sirD3D11Check( profile.nPipelineModelMask == ( 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE - 1U ) ) );
		sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) == 0 );
		sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_COPY_QUEUE ) == 0 );
		sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE ) == 0 );
		sirD3D11Check( profile.nMaximumTexture2D >= 2048 );
		sirD3D11Check( profile.nMaximumColorTargets >= 1 );
		sirD3D11Check( profile.nMaximumUavSlots == ( iLevel == 6 ? 64U : iLevel == 5 ? 8U : 0U ) );
		sirD3D11Check( ( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_TESSELLATION ) != 0 ) == ( iLevel >= 5 ) );
		sirD3D11Check( ( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER ) != 0 ) == ( iLevel >= 3 ) );
		probe.nMultisampleAvailable = 1;
		probe.nPresentationAvailable = 1;
		sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
		sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_MULTISAMPLE ) != 0 );
		sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_PRESENT ) != 0 );
	}
	sirD3D11FeatureProbe_t probe = {};
	probe.nSize = sizeof( probe );
	probe.nFeatureLevel = SIR_D3D11_FEATURE_LEVEL_10_0;
	sirD3D11FeatureProfile_t profile = {};
	profile.nSize = sizeof( profile );
	sirD3D11Check( sirD3D11BuildFeatureProfile( nullptr, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nFeatureLevel = 0;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_UNSUPPORTED );
	probe.nFeatureLevel = SIR_D3D11_FEATURE_LEVEL_9_3;
	probe.nComputeShader4xAvailable = 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nFeatureLevel = SIR_D3D11_FEATURE_LEVEL_10_0;
	probe.nComputeShader4xAvailable = 0;
	probe.nIndirectDrawAvailable = 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nIndirectDrawAvailable = 0;
	probe.nComputeShader4xAvailable = 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( profile.nMaximumUavSlots == 1 );
	sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 );
	probe.nSize = sizeof( probe ) - 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nSize = sizeof( probe );
	probe.nReserved = 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nReserved = 0;
	probe.nComputeShader4xAvailable = 2;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nComputeShader4xAvailable = 0;
	probe.nFeatureLevel = SIR_D3D11_FEATURE_LEVEL_11_1;
	probe.nD3D11_1InterfaceAvailable = 0;
	const sirD3D11FeatureProfile_t previous = profile;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D11Check( memcmp( &previous, &profile, sizeof( profile ) ) == 0 );
	probe.nD3D11_1InterfaceAvailable = 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( profile.nMaximumUavSlots == 64U );
	probe = {};
	probe.nSize = sizeof( probe );
	probe.nFeatureLevel = SIR_D3D11_FEATURE_LEVEL_11_0;
	profile.nSize = sizeof( profile );
	probe.nIndirectDrawAvailable = 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
	sirD3D11Check( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_INDIRECT_DRAW ) != 0 );
	probe.nIndirectDrawAvailable = 2;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nIndirectDrawAvailable = 0;
	probe.nD3D11_1InterfaceAvailable = 2;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nD3D11_1InterfaceAvailable = 0;
	probe.nMultisampleAvailable = 2;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nMultisampleAvailable = 0;
	probe.nPresentationAvailable = 2;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	probe.nPresentationAvailable = 0;
	profile.nSize = sizeof( profile ) - 1;
	sirD3D11Check( sirD3D11BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	printf( "D3D11 capability tests: %u assertions, %u failures\n", s_nAssertions, s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
