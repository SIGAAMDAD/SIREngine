/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/D3D10/SIRD3D10FeatureLevels.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                             sirGraphicsABIDriver_t *pDriver ) noexcept;

static void sirD3D10Require( bool bCondition ) noexcept
{
	if ( !bCondition ) {
		abort();
	}
}

int main( void )
{
	// Test valid probe for 10.0
	{
		sirD3D10FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe );
		probe.nFeatureLevel = SIR_D3D10_FEATURE_LEVEL_10_0;
		probe.nGeometryShaderAvailable = 1;
		probe.nStreamOutputAvailable = 1;
		probe.nMultisampleAvailable = 1;
		probe.nAlphaToCoverageAvailable = 1;

		sirD3D10FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D10Require( sirD3D10BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( profile.nFeatureLevel == SIR_D3D10_FEATURE_LEVEL_10_0 );
		sirD3D10Require( profile.nShaderProfile == SIR_D3D10_SHADER_PROFILE_4_0 );
		sirD3D10Require( profile.nMaximumTexture2D == 8192 );
		sirD3D10Require( profile.nMaximumColorTargets == 8 );
		sirD3D10Require( profile.nMaximumShaderResourceSlots == 128 );
		sirD3D10Require( profile.nSupportsInstancing == 1 );
		sirD3D10Require( profile.nSupportsGeometryShader == 1 );
		sirD3D10Require( profile.nSupportsStreamOutput == 1 );
		sirD3D10Require( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) != 0 );
		sirD3D10Require( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER ) != 0 );
		sirD3D10Require( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_MRT ) != 0 );
		sirD3D10Require( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_MULTISAMPLE ) != 0 );
	}

	// Test valid probe for 10.1
	{
		sirD3D10FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe );
		probe.nFeatureLevel = SIR_D3D10_FEATURE_LEVEL_10_1;
		probe.nGeometryShaderAvailable = 1;
		probe.nStreamOutputAvailable = 1;
		probe.nMultisampleAvailable = 1;
		probe.nAlphaToCoverageAvailable = 1;

		sirD3D10FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D10Require( sirD3D10BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( profile.nFeatureLevel == SIR_D3D10_FEATURE_LEVEL_10_1 );
		sirD3D10Require( profile.nShaderProfile == SIR_D3D10_SHADER_PROFILE_4_1 );
		sirD3D10Require( profile.nMaximumTexture2D == 8192 );
		sirD3D10Require( ( profile.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) != 0 );
	}

	// Test invalid probe flags
	{
		sirD3D10FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe );
		probe.nFeatureLevel = SIR_D3D10_FEATURE_LEVEL_10_0;
		probe.nGeometryShaderAvailable = 2; // Invalid
		sirD3D10FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D10Require( sirD3D10BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test null pointers
	{
		sirD3D10FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe );
		probe.nFeatureLevel = SIR_D3D10_FEATURE_LEVEL_10_0;
		sirD3D10FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D10Require( sirD3D10BuildFeatureProfile( nullptr, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
		sirD3D10Require( sirD3D10BuildFeatureProfile( &probe, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test size validation
	{
		sirD3D10FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe ) - 1; // Too small
		probe.nFeatureLevel = SIR_D3D10_FEATURE_LEVEL_10_0;
		sirD3D10FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D10Require( sirD3D10BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	}

	// Test unsupported feature level
	{
		sirD3D10FeatureProbe_t probe = {};
		probe.nSize = sizeof( probe );
		probe.nFeatureLevel = 0x9999; // Invalid
		sirD3D10FeatureProfile_t profile = {};
		profile.nSize = sizeof( profile );
		sirD3D10Require( sirD3D10BuildFeatureProfile( &probe, &profile ) == SIR_GRAPHICS_UNSUPPORTED );
	}

	return 0;
}