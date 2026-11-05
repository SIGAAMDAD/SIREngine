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

#include <Renderer/Drivers/D3D11/SIRD3D11Driver.h>
#include <stdio.h>

static uint32_t s_nFailures = 0;
static uint32_t s_nAssertions = 0;

static void sirD3D11Check( bool bSuccess, const char *pszTest ) noexcept
{
	++s_nAssertions;
	if ( !bSuccess ) {
		fprintf( stderr, "D3D11 registration failure: %s\n", pszTest );
		++s_nFailures;
	}
}

int main( void )
{
	SIRGraphicsRegistry registry;
	SIRD3D11Driver driver;
	sirD3D11Check( driver.getBackend() == SIR_GRAPHICS_BACKEND_D3D11, "native backend identity" );
	sirD3D11Check( sirD3D11RegisterDriver( nullptr, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null registry" );
	sirD3D11Check( sirD3D11RegisterDriver( &registry, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null driver" );
	sirD3D11Check( sirD3D11RegisterDriver( &registry, &driver ) == SIR_GRAPHICS_SUCCESS, "single registration" );
	sirD3D11Check( registry.findDriver( SIR_GRAPHICS_BACKEND_D3D11 ) == &driver, "backend lookup" );
	sirD3D11Check( sirD3D11RegisterDriver( &registry, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "duplicate registration" );
	sirD3D11Check( driver.getAdapterCount( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null adapter count" );
	uint32_t nCount = 0;
	uint32_t nWarpCount = 0;
	uint32_t nHardwareCount = 0;
	uint32_t nPreviousGeneration = 0;
	sirD3D11Check( driver.getAdapterCount( &nCount ) == SIR_GRAPHICS_SUCCESS, "native DXGI enumeration" );
	sirD3D11Check( nCount > 0, "at least hardware or WARP" );
	sirD3D11Check( driver.queryAdapter( 0, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null adapter output" );
	sirGraphicsAdapterInfo_t adapter = {};
	sirD3D11Check( driver.queryAdapter( nCount, &adapter ) == SIR_GRAPHICS_UNSUPPORTED, "out of bounds adapter" );

	for ( uint32_t iAdapter = 0; iAdapter < nCount; ++iAdapter ) {
		adapter = {};
		sirD3D11Check( driver.queryAdapter( iAdapter, &adapter ) == SIR_GRAPHICS_SUCCESS, "native adapter query" );
		sirD3D11Check( adapter.nAdapterIndex == iAdapter, "adapter indexing" );
		sirD3D11Check( adapter.nApiVersion.nMajor == 11, "D3D11 API identity" );
		sirD3D11Check( adapter.szName[0] != '\0', "adapter diagnostic name" );
		if ( adapter.bSoftwareAdapter ) {
			++nWarpCount;
		} else {
			++nHardwareCount;
		}
		printf( "D3D11 adapter %u: %s, vendor=0x%04x device=0x%04x LUID=0x%016llx name=%s\n", iAdapter,
		        adapter.bSoftwareAdapter ? "WARP" : "hardware", adapter.nVendorId, adapter.nDeviceId,
		        ( unsigned long long )adapter.nAdapterLuid, adapter.szName );
		sirD3D11Check( ( adapter.nFeatureMask & SIR_GRAPHICS_FEATURE_GRAPHICS ) != 0, "graphics capability" );
		sirD3D11Check( ( adapter.nFeatureMask & SIR_GRAPHICS_FEATURE_FIXED_FUNCTION ) == 0, "no fixed function" );

		sirGraphicsDeviceCreateDesc_t request = {};
		request.nAdapterIndex = iAdapter;
		request.nFramesInFlight = 2;
		request.bAllowSoftware = false;
		SIRRenderingDevice *pDevice = ( SIRRenderingDevice * )0x1;
		if ( adapter.bSoftwareAdapter ) {
			sirD3D11Check( driver.createDevice( request, &pDevice ) == SIR_GRAPHICS_UNSUPPORTED && pDevice == nullptr,
			               "WARP blocked without explicit opt in" );
		}
		request.bAllowSoftware = true;
		sirD3D11Check( driver.createDevice( request, &pDevice ) == SIR_GRAPHICS_SUCCESS && pDevice != nullptr, "native D3D11 device" );
		if ( pDevice != nullptr ) {
			sirGraphicsCapabilities_t capabilities = {};
			sirD3D11Check( pDevice->queryCapabilities( &capabilities ) == SIR_GRAPHICS_SUCCESS, "device capabilities" );
			sirD3D11Check( capabilities.eBackend == SIR_GRAPHICS_BACKEND_D3D11, "device backend" );
			sirD3D11Check( capabilities.nDeviceGeneration == pDevice->getDeviceGeneration(), "device generation matches capabilities" );
			sirD3D11Check( capabilities.nDeviceGeneration > nPreviousGeneration, "monotonic device generation" );
			nPreviousGeneration = capabilities.nDeviceGeneration;
			sirGraphicsABIPipelineModelCaps_t model = {};
			model.nSize = sizeof( model );
			sirD3D11Check( pDevice->queryPipelineModelCaps( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE, &model ) == SIR_GRAPHICS_SUCCESS,
			               "programmable model query" );
			sirD3D11Check( model.nDeviceGeneration == nPreviousGeneration, "model generation matches device" );
			sirD3D11Check( capabilities.bSoftwareAdapter == adapter.bSoftwareAdapter, "software admission" );
			SIRRenderingDevice *pSecondDevice = ( SIRRenderingDevice * )0x1;
			sirD3D11Check( driver.createDevice( request, &pSecondDevice ) == SIR_GRAPHICS_BUSY && pSecondDevice == nullptr,
			               "second device prohibited" );
			sirD3D11Check( driver.destroyDevice( pDevice ) == SIR_GRAPHICS_SUCCESS, "device destroy" );
			SIRRenderingDevice *pRecreated = nullptr;
			sirD3D11Check( driver.createDevice( request, &pRecreated ) == SIR_GRAPHICS_SUCCESS && pRecreated != nullptr,
			               "native device recreation" );
			if ( pRecreated != nullptr ) {
				sirD3D11Check( pRecreated->getDeviceGeneration() > nPreviousGeneration, "generation increases across recreation" );
				nPreviousGeneration = pRecreated->getDeviceGeneration();
				sirD3D11Check( driver.destroyDevice( pRecreated ) == SIR_GRAPHICS_SUCCESS, "recreated device destroy" );
			}
		}
		request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_RAY_TRACING;
		pDevice = ( SIRRenderingDevice * )0x1;
		sirD3D11Check( driver.createDevice( request, &pDevice ) == SIR_GRAPHICS_UNSUPPORTED && pDevice == nullptr,
		               "unsupported capability rejected" );
	}

	sirD3D11Check( nWarpCount == 1, "exactly one advertised and created WARP device" );
	printf( "D3D11 adapter matrix: %u hardware, %u WARP; %u device generations\n", nHardwareCount, nWarpCount, nPreviousGeneration );
	sirD3D11Check( driver.destroyDevice( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null destroy" );
	sirD3D11Check( registry.unregisterDriver( SIR_GRAPHICS_BACKEND_D3D11 ) == SIR_GRAPHICS_SUCCESS, "unregister" );
	sirD3D11Check( registry.getCount() == 0, "registry restored" );
	printf( "D3D11 native registration: %u assertions, %u failures\n", s_nAssertions, s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
