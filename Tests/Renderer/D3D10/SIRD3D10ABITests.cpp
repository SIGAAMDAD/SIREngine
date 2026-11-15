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
#include <Renderer/SIRGraphicsTypes.h>
#include <cstddef>
#include <stdint.h>
#include <stdlib.h>

extern "C" uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                             sirGraphicsABIDriver_t *pDriver ) noexcept;

static void sirD3D10Require( bool bCondition ) noexcept
{
	if ( !bCondition ) {
		abort();
	}
}

static uint32_t s_nAllocations = 0;
static uint32_t s_nReleases = 0;
static bool s_bFailAllocation = false;
static bool s_bReturnMisaligned = false;

static void *sirD3D10TestAllocate( void *pContext, uint64_t nBytes, uint64_t nAlignment ) noexcept
{
	( void )pContext;
	sirD3D10Require( nAlignment <= alignof( std::max_align_t ) );
	if ( s_bFailAllocation ) {
		return nullptr;
	}
	void *pMemory = malloc( ( size_t )nBytes + ( s_bReturnMisaligned ? 1U : 0U ) );
	if ( pMemory != nullptr ) {
		++s_nAllocations;
		if ( s_bReturnMisaligned ) {
			return ( ( uint8_t * )pMemory ) + 1;
		}
	}
	return pMemory;
}

static void sirD3D10TestFree( void *pContext, void *pMemory ) noexcept
{
	( void )pContext;
	sirD3D10Require( pMemory != nullptr );
	++s_nReleases;
	if ( s_bReturnMisaligned ) {
		pMemory = ( ( uint8_t * )pMemory ) - 1;
	}
	free( pMemory );
}

static void sirD3D10UnexpectedFree( void *pContext, void *pMemory ) noexcept
{
	( void )pContext;
	( void )pMemory;
	abort();
}

static void sirD3D10TestLog( void *pContext, uint32_t nSeverity, const char *pszMessage ) noexcept
{
	( void )pContext;
	( void )nSeverity;
	( void )pszMessage;
}

int main( void )
{
	static_assert( sizeof( void * ) == 8, "D3D10 module ABI requires x64" );
	static_assert( sizeof( sirGraphicsHandle_t ) == 16, "graphics resource handle ABI drift" );
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = sirD3D10TestAllocate;
	host.pFree = sirD3D10TestFree;
	host.pLog = sirD3D10TestLog;

	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	sirD3D10Require( SIRGetGraphicsDriverABI( 2, 0, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH );
	sirD3D10Require( s_nAllocations == 0 );
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 1, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH );
	sirD3D10Require( s_nAllocations == 0 );
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, nullptr, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );

	const uint32_t nHostSize = host.nSize;
	host.nSize = sizeof( host ) - 1;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	host.nSize = nHostSize;
	driver.nSize = sizeof( driver ) - 1;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	driver.nSize = sizeof( driver );

	s_bFailAllocation = true;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_DRIVER_ERROR );
	s_bFailAllocation = false;
	sirD3D10Require( s_nAllocations == 0 );

	s_bReturnMisaligned = true;
	sirD3D10Require( SIRGetGraphicsDriverABI( 1, 0, &host, &driver ) == SIR_GRAPHICS_DRIVER_ERROR );
	sirD3D10Require( s_nAllocations == 1 && s_nReleases == 1 );
	s_bReturnMisaligned = false;

	struct
	{
		sirGraphicsABIDriver_t driver;
		uint64_t nCanary;
	} result = {};
	result.driver.nSize = sizeof( result );
	result.nCanary = UINT64_C( 0xA551D10A551D10A5 );
	sirD3D10Require( sirGraphicsNegotiateDriverABI( SIRGetGraphicsDriverABI, &host, &result.driver ) == SIR_GRAPHICS_SUCCESS );
	sirD3D10Require( result.nCanary == UINT64_C( 0xA551D10A551D10A5 ) );
	sirD3D10Require( result.driver.nBackend == SIR_GRAPHICS_BACKEND_D3D10 );
	sirD3D10Require( result.driver.nMajor == 1 && result.driver.nMinor == 0 );
	sirD3D10Require( sirGraphicsValidateABIDriver( &result.driver ) == SIR_GRAPHICS_SUCCESS );
	sirD3D10Require( s_nAllocations == 2 && s_nReleases == 1 );

	uint32_t nAdapters = 99;
	sirD3D10Require( result.driver.pAdapterCount( result.driver.pDriver, &nAdapters ) == SIR_GRAPHICS_SUCCESS );
	sirD3D10Require( nAdapters == 0 );
	sirD3D10Require( result.driver.pAdapterCount( result.driver.pDriver, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	sirD3D10Require( result.driver.pQueryAdapter( result.driver.pDriver, &adapter ) == SIR_GRAPHICS_UNSUPPORTED );

	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nFramesInFlight = 2;
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED );
	sirD3D10Require( device.pDevice == nullptr );
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED );
	request.nRequiredFeatures = 0;
	request.nFramesInFlight = 0;
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	request.nFramesInFlight = 2;
	request.nReserved = 1;
	sirD3D10Require( result.driver.pCreateDevice( result.driver.pDriver, &request, &device ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D10Require( result.driver.pDestroyDevice( result.driver.pDriver, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );

	// The state holds its own copy of the validated host callbacks.
	host.pFree = sirD3D10UnexpectedFree;
	result.driver.pReleaseDriver( result.driver.pDriver );
	host.pFree = sirD3D10TestFree;
	result.driver.pReleaseDriver( nullptr );
	sirD3D10Require( s_nAllocations == s_nReleases );

	// Repeated load/unload does not grow the host allocation count.
	for ( uint32_t iIteration = 0; iIteration < 64; ++iIteration ) {
		sirGraphicsABIDriver_t current = {};
		current.nSize = sizeof( current );
		sirD3D10Require( sirGraphicsNegotiateDriverABI( SIRGetGraphicsDriverABI, &host, &current ) == SIR_GRAPHICS_SUCCESS );
		sirD3D10Require( s_nAllocations == s_nReleases + 1 );
		current.pReleaseDriver( current.pDriver );
		sirD3D10Require( s_nAllocations == s_nReleases );
	}
	return 0;
}
