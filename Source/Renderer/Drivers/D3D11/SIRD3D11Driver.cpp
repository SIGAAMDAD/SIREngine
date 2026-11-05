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

// Keep the SDK entirely inside the native driver translation unit.
#include <d3d11.h>
#include <dxgi.h>

sirGraphicsBackend_t SIRD3D11Driver::getBackend( void ) const noexcept
{
	return SIR_GRAPHICS_BACKEND_D3D11;
}

sirGraphicsResult_t SIRD3D11Driver::getAdapterCount( uint32_t *pCount ) const noexcept
{
	if ( pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D11Driver::queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept
{
	( void )nAdapterIndex;
	if ( pInfo == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

sirGraphicsResult_t SIRD3D11Driver::createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept
{
	( void )desc;
	if ( ppDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDevice = nullptr;
	return SIR_GRAPHICS_UNSUPPORTED;
}

sirGraphicsResult_t SIRD3D11Driver::destroyDevice( SIRRenderingDevice *pDevice ) noexcept
{
	return pDevice == nullptr ? SIR_GRAPHICS_INVALID_ARGUMENT : SIR_GRAPHICS_UNSUPPORTED;
}

sirGraphicsResult_t sirD3D11RegisterDriver( SIRGraphicsRegistry *pRegistry, SIRD3D11Driver *pDriver ) noexcept
{
	if ( pRegistry == nullptr || pDriver == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return pRegistry->registerDriver( pDriver );
}
