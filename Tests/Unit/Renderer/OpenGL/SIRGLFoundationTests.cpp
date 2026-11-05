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

#include <Renderer/Drivers/OpenGL/SIRGLCapabilities.h>
#include <stdio.h>
#include <string.h>

static int s_nFailures = 0;

static void sirGLCheck( bool bGood, const char *pszCase ) noexcept
{
	if ( !bGood ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
		++s_nFailures;
	}
}

int main( void )
{
	sirGLProbe_t probe = {};
	probe.nMajor = 4;
	probe.nMinor = 6;
	probe.nCoreProfileMask = 1;
	probe.nMaximumTextureSize = 16384;
	probe.nMaximumColorAttachments = 8;
	probe.nMaximumUniformBindings = 84;
	probe.nMaximumStorageBindings = 16;
	probe.nMaximumSamples = 8;
	memcpy( probe.szVendor, "Mesa", sizeof( "Mesa" ) );
	memcpy( probe.szRenderer, "llvmpipe (LLVM 23)", sizeof( "llvmpipe (LLVM 23)" ) );

	sirGLCheck( sirGLValidateCore46( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null probe" );
	sirGLCheck( sirGLValidateCore46( &probe ) == SIR_GRAPHICS_SUCCESS, "GL46 valid" );
	probe.nMinor = 5;
	sirGLCheck( sirGLValidateCore46( &probe ) == SIR_GRAPHICS_UNSUPPORTED, "GL45 rejected" );
	probe.nMajor = 3;
	probe.nMinor = 3;
	sirGLCheck( sirGLValidateCore46( &probe ) == SIR_GRAPHICS_UNSUPPORTED, "GL33 rejected" );
	probe.nMajor = 4;
	probe.nMinor = 6;
	probe.nCoreProfileMask = 2;
	sirGLCheck( sirGLValidateCore46( &probe ) == SIR_GRAPHICS_UNSUPPORTED, "compatibility rejected" );
	probe.nCoreProfileMask = 1;
	probe.nMaximumStorageBindings = 0;
	sirGLCheck( sirGLValidateCore46( &probe ) == SIR_GRAPHICS_UNSUPPORTED, "missing SSBO limit" );
	probe.nMaximumStorageBindings = 16;
	probe.szVendor[0] = '\0';
	sirGLCheck( sirGLValidateCore46( &probe ) == SIR_GRAPHICS_UNSUPPORTED, "missing vendor" );
	memcpy( probe.szVendor, "Mesa", sizeof( "Mesa" ) );

	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	sirGLCheck( sirGLBuildAdapter( &probe, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null adapter" );
	adapter.nIndex = 1;
	sirGLCheck( sirGLBuildAdapter( &probe, &adapter ) == SIR_GRAPHICS_INVALID_ARGUMENT, "wrong index" );
	adapter.nIndex = 0;
	sirGLCheck( sirGLBuildAdapter( &probe, &adapter ) == SIR_GRAPHICS_SUCCESS, "build adapter" );
	sirGLCheck( adapter.nBackend == SIR_GRAPHICS_BACKEND_OPENGL, "correct backend" );
	sirGLCheck( adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE, "software classification" );
	sirGLCheck( adapter.nQueueMask == ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ), "single queue" );
	sirGLCheck( adapter.nFeatureMask == 0, "unimplemented ops not advertised" );
	sirGLCheck( adapter.nStableId != 0, "adapter identity" );
	memcpy( probe.szRenderer, "Real GPU", sizeof( "Real GPU" ) );
	adapter.nSize = sizeof( adapter );
	sirGLCheck( sirGLBuildAdapter( &probe, &adapter ) == SIR_GRAPHICS_SUCCESS, "hardware adapter" );
	sirGLCheck( adapter.nKind == SIR_GRAPHICS_ABI_ADAPTER_OTHER, "honest hardware kind" );

	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	sirGLCheck( sirGLBuildCapabilities( &probe, 0, &caps ) == SIR_GRAPHICS_INVALID_ARGUMENT, "generation zero" );
	sirGLCheck( sirGLBuildCapabilities( &probe, 4, &caps ) == SIR_GRAPHICS_SUCCESS, "build capabilities" );
	sirGLCheck( caps.nDeviceGeneration == 4 && caps.nShaderProfile == 460, "device generation and GLSL" );
	sirGLCheck( caps.nMaximumFramesInFlight == 1 && caps.nMaximumSamples == 1, "unimplemented native paths gated" );
	sirGLCheck( caps.nEnabledFeatures == 0 && caps.nEnabledQueues == 1, "only serialized queue" );
	sirGLCheck( caps.nMaximumTextureDimension == 16384 && caps.nMaximumBindings == 84, "GL queried limits" );
	caps.nSize = 0;
	sirGLCheck( sirGLBuildCapabilities( &probe, 4, &caps ) == SIR_GRAPHICS_INVALID_ARGUMENT, "short ABI caps" );

	if ( s_nFailures == 0 ) {
		printf( "OpenGL 4.6 foundation capability tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
