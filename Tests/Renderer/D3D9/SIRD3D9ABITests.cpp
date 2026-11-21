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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

extern "C" uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                             sirGraphicsABIDriver_t *pDriver ) noexcept;

static uint32_t s_nAllocated = 0;
static uint32_t s_nFreed = 0;
static bool s_bFailAlloc = false;

static void *sirD3D9TestAllocate( void *pContext, uint64_t nBytes, uint64_t nAlignment )
{
	( void )pContext;
	if ( nAlignment > alignof( max_align_t ) || s_bFailAlloc ) {
		return nullptr;
	}
	void *pMemory = malloc( ( size_t )nBytes );
	if ( pMemory != nullptr ) {
		++s_nAllocated;
	}
	return pMemory;
}

static void sirD3D9TestFree( void *pContext, void *pMemory )
{
	( void )pContext;
	if ( pMemory == nullptr )
		abort();
	++s_nFreed;
	free( pMemory );
}

static void sirD3D9TestLog( void *pContext, uint32_t nSeverity, const char *pszMessage )
{
	( void )pContext;
	( void )nSeverity;
	( void )pszMessage;
}

static void sirD3D9Require( bool bCondition )
{
	if ( !bCondition )
		abort();
}

int main( void )
{
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = sirD3D9TestAllocate;
	host.pFree = sirD3D9TestFree;
	host.pLog = sirD3D9TestLog;

	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	sirD3D9Require( SIRGetGraphicsDriverABI( 2, 0, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH );
	sirD3D9Require( SIRGetGraphicsDriverABI( 1, 1, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH );
	sirD3D9Require( SIRGetGraphicsDriverABI( 1, 0, nullptr, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D9Require( SIRGetGraphicsDriverABI( 1, 0, &host, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	s_bFailAlloc = true;
	sirD3D9Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_OUT_OF_MEMORY );
	s_bFailAlloc = false;
	sirD3D9Require( s_nAllocated == 0 );
	sirD3D9Require( sirGraphicsNegotiateDriverABI( SIRGetGraphicsDriverABI, &host, &driver ) == SIR_GRAPHICS_SUCCESS );
	sirD3D9Require( driver.nBackend == SIR_GRAPHICS_BACKEND_D3D9 );
	sirD3D9Require( sirGraphicsValidateABIDriver( &driver ) == SIR_GRAPHICS_SUCCESS );
	sirD3D9Require( s_nAllocated == 1 );
	uint32_t nCount = 42;
	const uint32_t nCountResult = driver.pAdapterCount( driver.pDriver, &nCount );
#ifdef _WIN32
	sirD3D9Require( nCountResult == SIR_GRAPHICS_SUCCESS || nCountResult == SIR_GRAPHICS_UNSUPPORTED );
#else
	sirD3D9Require( nCountResult == SIR_GRAPHICS_UNSUPPORTED );
#endif
	sirD3D9Require( nCount == 0 || nCountResult == SIR_GRAPHICS_SUCCESS );
	sirD3D9Require( driver.pAdapterCount( driver.pDriver, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nFramesInFlight = 2;
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	sirD3D9Require( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED );
	sirD3D9Require( device.pDevice == nullptr );
	request.nFramesInFlight = 0;
	sirD3D9Require( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nFramesInFlight = 2;
	request.nReserved = 1;
	sirD3D9Require( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D9Require( driver.pDestroyDevice( driver.pDriver, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	driver.pReleaseDriver( driver.pDriver );
	sirD3D9Require( s_nAllocated == s_nFreed );
	return 0;
}
