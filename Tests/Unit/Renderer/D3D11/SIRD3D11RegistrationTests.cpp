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

static void sirD3D11Check( bool bSuccess, const char *pszTest ) noexcept
{
	if ( !bSuccess ) {
		fprintf( stderr, "D3D11 registration failure: %s\n", pszTest );
		++s_nFailures;
	}
}

int main( void )
{
	SIRGraphicsRegistry registry;
	SIRD3D11Driver driver;
	uint32_t nCount = 7;
	sirD3D11Check( driver.getBackend() == SIR_GRAPHICS_BACKEND_D3D11, "native backend identity" );
	sirD3D11Check( sirD3D11RegisterDriver( nullptr, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null registry rejected" );
	sirD3D11Check( sirD3D11RegisterDriver( &registry, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null driver rejected" );
	sirD3D11Check( sirD3D11RegisterDriver( &registry, &driver ) == SIR_GRAPHICS_SUCCESS, "single registration" );
	sirD3D11Check( registry.getCount() == 1, "single registered driver" );
	sirD3D11Check( registry.findDriver( SIR_GRAPHICS_BACKEND_D3D11 ) == &driver, "explicit identity resolution" );
	sirD3D11Check( sirD3D11RegisterDriver( &registry, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT, "duplicate rejected" );
	sirD3D11Check( driver.getAdapterCount( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null count rejected" );
	sirD3D11Check( driver.getAdapterCount( &nCount ) == SIR_GRAPHICS_SUCCESS && nCount == 0, "no phantom adapters" );
	sirD3D11Check( driver.queryAdapter( 0, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null adapter rejected" );
	sirGraphicsAdapterInfo_t adapter = {};
	sirD3D11Check( driver.queryAdapter( 0, &adapter ) == SIR_GRAPHICS_UNSUPPORTED, "unimplemented discovery explicit" );
	SIRRenderingDevice *pDevice = ( SIRRenderingDevice * )0x1;
	sirGraphicsDeviceCreateDesc_t request = {};
	sirD3D11Check( driver.createDevice( request, &pDevice ) == SIR_GRAPHICS_UNSUPPORTED && pDevice == nullptr,
	               "no phantom device from stub" );
	sirD3D11Check( driver.destroyDevice( nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null destroy rejected" );
	sirD3D11Check( registry.unregisterDriver( SIR_GRAPHICS_BACKEND_D3D11 ) == SIR_GRAPHICS_SUCCESS, "unregister success" );
	sirD3D11Check( registry.getCount() == 0, "registry restored" );
	return s_nFailures == 0 ? 0 : 1;
}
