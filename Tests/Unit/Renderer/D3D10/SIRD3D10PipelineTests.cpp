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

#include <Renderer/Drivers/D3D10/SIRD3D10Pipelines.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static void sirD3D10Require( bool bCondition, const char *pszMsg ) noexcept
{
	if ( !bCondition ) {
		std::fprintf( stderr, "FAIL: %s\n", pszMsg );
		abort();
	}
	else {
		std::printf( "PASS: %s\n", pszMsg );
	}
}

// Simple DXBC-like shader bytecode for testing
static const uint8_t s_validVS40[] = {
	'D', 'X', 'B', 'C', // FourCC
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // Hash
	0x00, 0x04, 0x00, 0x00, // Version: VS 4.0 (0x4000)
	0x40, 0x00, 0x00, 0x00, // Size
	// ... rest would be actual bytecode
};

static const uint8_t s_validPS40[] = {
	'D', 'X', 'B', 'C',
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0x01, 0x04, 0x00, 0x00, // Version: PS 4.0 (0x4001)
	0x40, 0x00, 0x00, 0x00,
};

static const uint8_t s_validGS40[] = {
	'D', 'X', 'B', 'C',
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0x02, 0x04, 0x00, 0x00, // Version: GS 4.0 (0x4002)
	0x40, 0x00, 0x00, 0x00,
};

static const uint8_t s_invalidDXIL[] = {
	'D', 'X', 'I', 'L', // DXIL FourCC
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0x00, 0x05, 0x00, 0x00, // SM 5.0
	0x40, 0x00, 0x00, 0x00,
};

int main( void )
{
	std::printf( "D3D10 Pipeline validation tests (CPU mock)\n" );

	// Test DXBC shader validation
	{
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_validVS40, sizeof( s_validVS40 ),
		                                                           SIR_GRAPHICS_STAGE_VERTEX, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "valid VS 4.0 accepted" );
	}

	{
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_validPS40, sizeof( s_validPS40 ),
		                                                           SIR_GRAPHICS_STAGE_FRAGMENT, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "valid PS 4.0 accepted" );
	}

	{
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_validGS40, sizeof( s_validGS40 ),
		                                                           SIR_GRAPHICS_STAGE_GEOMETRY, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "valid GS 4.0 accepted" );
	}

	// Test DXIL rejection
	{
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_invalidDXIL, sizeof( s_invalidDXIL ),
		                                                           SIR_GRAPHICS_STAGE_VERTEX, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_UNSUPPORTED, "DXIL rejected on D3D10" );
	}

	// Test SM 5.0 rejection
	{
		static const uint8_t s_sm50[] = {
			'D', 'X', 'B', 'C',
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0x00, 0x05, 0x00, 0x00, // SM 5.0
			0x40, 0x00, 0x00, 0x00,
		};
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_sm50, sizeof( s_sm50 ),
		                                                           SIR_GRAPHICS_STAGE_VERTEX, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_UNSUPPORTED, "SM 5.0 rejected on D3D10 10.0" );
	}

	// Test stage mismatch
	{
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_validPS40, sizeof( s_validPS40 ),
		                                                           SIR_GRAPHICS_STAGE_VERTEX, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_INVALID_ARGUMENT, "stage mismatch rejected" );
	}

	// Test 10.1 accepts 4.1
	{
		static const uint8_t s_validPS41[] = {
			'D', 'X', 'B', 'C',
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0x01, 0x04, 0x01, 0x00, // Version: PS 4.1 (0x40101)
			0x40, 0x00, 0x00, 0x00,
		};
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_validPS41, sizeof( s_validPS41 ),
		                                                           SIR_GRAPHICS_STAGE_FRAGMENT, SIR_D3D10_FEATURE_LEVEL_10_1,
		                                                           SIR_D3D10_SHADER_PROFILE_4_1, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_SUCCESS, "valid PS 4.1 accepted on 10.1" );
	}

	// Test 10.0 rejects 4.1
	{
		static const uint8_t s_validPS41[] = {
			'D', 'X', 'B', 'C',
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
			0x01, 0x04, 0x01, 0x00, // Version: PS 4.1 (0x40101)
			0x40, 0x00, 0x00, 0x00,
		};
		sirGraphicsResult_t eResult = sirD3D10ValidateDXBCShader( s_validPS41, sizeof( s_validPS41 ),
		                                                           SIR_GRAPHICS_STAGE_FRAGMENT, SIR_D3D10_FEATURE_LEVEL_10_0,
		                                                           SIR_D3D10_SHADER_PROFILE_4_0, "main" );
		sirD3D10Require( eResult == SIR_GRAPHICS_UNSUPPORTED, "PS 4.1 rejected on 10.0" );
	}

	std::printf( "\nAll D3D10 Pipeline validation tests passed!\n" );
	return 0;
}