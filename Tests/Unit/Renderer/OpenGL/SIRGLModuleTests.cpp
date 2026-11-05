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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#if defined( _WIN32 )
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifndef SIR_TEST_GL46_MODULE_PATH
#error SIR_TEST_GL46_MODULE_PATH must be the actual built driver module
#endif

static int s_nOutstanding = 0;
static int s_nFailures = 0;

static void *sirGLTestAllocate( void *, uint64_t nBytes, uint64_t )
{
	void *pMemory = malloc( ( size_t )nBytes );
	if ( pMemory != nullptr ) {
		++s_nOutstanding;
	}
	return pMemory;
}

static void sirGLTestFree( void *, void *pMemory )
{
	if ( pMemory != nullptr ) {
		--s_nOutstanding;
		free( pMemory );
	}
}

static void sirGLTestLog( void *, uint32_t, const char *pszMessage )
{
	fprintf( stderr, "GL46: %s\n", pszMessage );
}

static void sirGLTestCheck( bool bGood, const char *pszCase )
{
	if ( !bGood ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
		++s_nFailures;
	}
}

int main( void )
{
#if defined( _WIN32 )
	HMODULE pModule = LoadLibraryA( SIR_TEST_GL46_MODULE_PATH );
	sirGraphicsABIFactory_t pFactory =
	    pModule != nullptr ? ( sirGraphicsABIFactory_t )GetProcAddress( pModule, SIR_GRAPHICS_DRIVER_ABI_ENTRY ) : nullptr;
#else
	void *pModule = dlopen( SIR_TEST_GL46_MODULE_PATH, RTLD_NOW | RTLD_LOCAL );
	sirGraphicsABIFactory_t pFactory =
	    pModule != nullptr ? ( sirGraphicsABIFactory_t )dlsym( pModule, SIR_GRAPHICS_DRIVER_ABI_ENTRY ) : nullptr;
#endif
	sirGLTestCheck( pFactory != nullptr, "load native exported driver" );
	if ( pFactory == nullptr ) {
		return 1;
	}
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = sirGLTestAllocate;
	host.pFree = sirGLTestFree;
	host.pLog = sirGLTestLog;

	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	sirGLTestCheck( pFactory( 99, 0, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH, "wrong ABI version rejected" );
	sirGLTestCheck( s_nOutstanding == 0, "version mismatch is failure atomic" );
	sirGLTestCheck( pFactory( SIR_GRAPHICS_DRIVER_ABI_MAJOR, 0, &host, &driver ) == SIR_GRAPHICS_SUCCESS, "initialize native module" );
	sirGLTestCheck( driver.nBackend == SIR_GRAPHICS_BACKEND_OPENGL && driver.pDriver != nullptr, "correct exported backend" );
	sirGLTestCheck( driver.pAdapterCount != nullptr && driver.pCreateDevice != nullptr && driver.pDestroyDevice != nullptr,
	                "required factory callbacks" );
	uint32_t nAdapters = 999;
	sirGLTestCheck( driver.pAdapterCount( driver.pDriver, &nAdapters ) == SIR_GRAPHICS_SUCCESS && nAdapters <= 1,
	                "actual GL context admission query" );
	if ( nAdapters == 1 ) {
		sirGraphicsABIAdapter_t adapter = {};
		adapter.nSize = sizeof( adapter );
		sirGLTestCheck( driver.pQueryAdapter( driver.pDriver, &adapter ) == SIR_GRAPHICS_SUCCESS, "real adapter info" );
		sirGLTestCheck( adapter.nApiMajor >= 4 && adapter.nQueueMask == 1 && adapter.nFeatureMask == 0,
		                "core version / truthful serial capabilities" );
		sirGraphicsABIDeviceRequest_t request = {};
		request.nSize = sizeof( request );
		request.nHeadless = 1;
		request.nAllowSoftware = 1;
		request.nFramesInFlight = 1;
		request.nResidentBudgetBytes = 64 * 1024 * 1024;
		sirGraphicsABIDevice_t device = {};
		device.nSize = sizeof( device );
		request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
		sirGLTestCheck( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED,
		                "cannot enable unimplemented graphics commands" );
		request.nRequiredFeatures = 0;
		sirGLTestCheck( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_SUCCESS,
		                "real headless GL46 context created" );
		if ( device.pDevice != nullptr ) {
			sirGraphicsABIPipelineModelCaps_t model = {};
			model.nSize = sizeof( model );
			sirGLTestCheck( device.pQueryPipelineModelCaps != nullptr, "pipeline-model query callback provided" );
			sirGLTestCheck( device.pQueryPipelineModelCaps( device.pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ) ==
			                    SIR_GRAPHICS_UNSUPPORTED,
			                "unimplemented pipeline model denied" );
			sirGraphicsABICapabilities_t caps = {};
			caps.nSize = sizeof( caps );
			sirGLTestCheck( device.pQueryCapabilities( device.pDevice, &caps ) == SIR_GRAPHICS_SUCCESS, "real GL46 capabilities" );
			sirGLTestCheck( caps.nShaderProfile == 460 && caps.nEnabledQueues == 1 && caps.nEnabledFeatures == 0,
			                "usable context yet no false draw feature" );
			sirGraphicsMemoryBudget_t budget = {};
			sirGLTestCheck( device.pQueryMemoryBudget( device.pDevice, &budget ) == SIR_GRAPHICS_SUCCESS &&
			                    budget.nBudgetBytes == request.nResidentBudgetBytes && budget.nCommittedBytes == 0,
			                "budget request and physical allocation truth" );
			sirGraphicsSamplerDesc_t sampler = {};
			sampler.eFilter = SIR_GRAPHICS_FILTER_TRILINEAR;
			sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
			sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
			sampler.eAddressW = SIR_GRAPHICS_ADDRESS_MIRROR;
			sampler.fMaximumAnisotropy = 1.0f;
			sampler.fMinimumLod = 0.0f;
			sampler.fMaximumLod = 12.0f;
			sirGraphicsHandle_t hSampler = {};
			sirGLTestCheck( device.pCreateSampler( device.pDevice, &sampler, &hSampler ) == SIR_GRAPHICS_SUCCESS &&
			                    hSampler.eType == SIR_GRAPHICS_HANDLE_SAMPLER,
			                "native sampler ABI creation" );
			if ( hSampler.nGeneration != 0 ) {
				sirGLTestCheck( device.pRetireResource( device.pDevice, hSampler, nullptr ) == SIR_GRAPHICS_SUCCESS,
				                "native sampler ABI retirement" );
				sirGLTestCheck( device.pRetireResource( device.pDevice, hSampler, nullptr ) == SIR_GRAPHICS_INVALID_HANDLE,
				                "stale sampler rejected" );
			}
			uint32_t nForeignResult = 0;
			std::thread foreign( [&]() { nForeignResult = device.pShutdown( device.pDevice ); } );
			foreign.join();
			sirGLTestCheck( nForeignResult == SIR_GRAPHICS_BUSY, "foreign thread cannot destroy context" );
			sirGLTestCheck( driver.pDestroyDevice( driver.pDriver, &device ) == SIR_GRAPHICS_SUCCESS, "owner-thread device retirement" );
		}
	} else {
		printf( "OpenGL 4.6 native GPU context unavailable here; ABI negatives verified\n" );
	}
	driver.pReleaseDriver( driver.pDriver );
	sirGLTestCheck( s_nOutstanding == 0, "all native allocations released" );
#if defined( _WIN32 )
	FreeLibrary( pModule );
#else
	dlclose( pModule );
#endif
	if ( s_nFailures == 0 ) {
		printf( "OpenGL 4.6 binary module tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
