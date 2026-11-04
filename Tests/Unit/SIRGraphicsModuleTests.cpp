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

#include <Renderer/SIRGraphicsModule.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <stdio.h>
#include <stdlib.h>

static void *testAlloc( void *, uint64_t nBytes, uint64_t ) noexcept
{
	return malloc( ( size_t )nBytes );
}

static void testFree( void *, void *pData ) noexcept
{
	free( pData );
}

static void testLog( void *, uint32_t, const char * ) noexcept
{
}

typedef void ( *sirMockSetMode_t )( uint32_t );

static bool testBinaryModule( void ) noexcept
{
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = testAlloc;
	host.pFree = testFree;
	host.pLog = testLog;
	sirGraphicsABIModule_t module = {};
	if ( sirGraphicsLoadABIModule( "/not/a/real/graphics-driver", &host, &module ) != SIR_GRAPHICS_ABI_UNSUPPORTED ||
	     module.library.pNativeHandle != nullptr ) {
		return false;
	}
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsLoadABIModule( SIR_TEST_ABI_MODULE_PATH, &host, &module ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	const sirMockSetMode_t pMode = ( sirMockSetMode_t )SIRDynamicLibrary::findSymbol( &module.library, "SIRSetGraphicsABIMockMode" );
	if ( pMode == nullptr ) {
		return false;
	}
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	if ( sirGraphicsQueryABIAdapter( &module, 0, &adapter ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     adapter.nKind != SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE || adapter.nBackend != SIR_GRAPHICS_BACKEND_VULKAN ) {
		return false;
	}
	pMode( 1 );
	adapter.nStableId = 0x4321;
	if ( sirGraphicsQueryABIAdapter( &module, 0, &adapter ) != SIR_GRAPHICS_ABI_DRIVER_ERROR ||
	     adapter.nStableId != 0x4321 ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsQueryABIAdapter( &module, 1, &adapter ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}

	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nAdapterIndex = 0;
	request.nFramesInFlight = 2;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_UNSUPPORTED ||
	     module.nActiveDevices != 0 ) {
		return false;
	}
	request.nAllowSoftware = 1;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	pMode( 5 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_DRIVER_ERROR ||
	     module.nActiveDevices != 0 || device.pDevice != nullptr ) {
		return false;
	}
	pMode( 3 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_UNSUPPORTED ||
	     module.nActiveDevices != 0 ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsCreateABIDevice( &module, &request, &device ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     module.nActiveDevices != 1 ) {
		return false;
	}
	if ( sirGraphicsUnloadABIModule( &module ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	if ( sirGraphicsQueryABICapabilities( &device, SIR_GRAPHICS_BACKEND_VULKAN, &caps ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     caps.nDeviceGeneration != 5 ) {
		return false;
	}
	caps.nDeviceGeneration = 93;
	if ( sirGraphicsQueryABICapabilities( &device, SIR_GRAPHICS_BACKEND_D3D12, &caps ) != SIR_GRAPHICS_ABI_DRIVER_ERROR ||
	     caps.nDeviceGeneration != 93 ) {
		return false;
	}
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	if ( sirGraphicsQueryABIFormat( &device, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ) != SIR_GRAPHICS_ABI_SUCCESS ||
	     format.nFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM ) {
		return false;
	}
	pMode( 2 );
	format.nFormat = 123;
	if ( sirGraphicsQueryABIFormat( &device, SIR_GRAPHICS_FORMAT_RGBA8_UNORM, &format ) != SIR_GRAPHICS_ABI_DRIVER_ERROR ||
	     format.nFormat != 123 ) {
		return false;
	}
	pMode( 0 );
	if ( sirGraphicsQueryABIFormat( &device, SIR_GRAPHICS_FORMAT_D16_UNORM, &format ) != SIR_GRAPHICS_ABI_UNSUPPORTED ) {
		return false;
	}
	if ( sirGraphicsDestroyABIDevice( &module, &device ) != SIR_GRAPHICS_ABI_SUCCESS || device.pDevice != nullptr ||
	     module.nActiveDevices != 0 ) {
		return false;
	}
	if ( sirGraphicsDestroyABIDevice( &module, &device ) != SIR_GRAPHICS_ABI_INVALID_ARGUMENT ) {
		return false;
	}
	if ( sirGraphicsUnloadABIModule( &module ) != SIR_GRAPHICS_ABI_SUCCESS || module.library.pNativeHandle != nullptr ) {
		return false;
	}
	return sirGraphicsUnloadABIModule( &module ) == SIR_GRAPHICS_ABI_INVALID_ARGUMENT;
}

int main( void )
{
	if ( !testBinaryModule() ) {
		fputs( "graphics.abi.module FAILED\n", stderr );
		return 1;
	}
	puts( "PASS: dynamic graphics ABI module, software gating, device lifetime and malformed native responses" );
	return 0;
}
